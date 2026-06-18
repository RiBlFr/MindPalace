#include "aiassistantservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>

namespace MindPalace::Service {
namespace {

constexpr qint64 kMaxDocumentBytes = 20 * 1024 * 1024;
constexpr int kMaxPromptChars = 60000;

QString setError(QString* errorMessage, const QString& message) {
    if (errorMessage) *errorMessage = message;
    return message;
}

QString readTextFileUtf8(const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(errorMessage, QStringLiteral("无法读取文件：%1").arg(file.errorString()));
        return {};
    }

    const QByteArray data = file.readAll();
    QString text = QString::fromUtf8(data);
    if (text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLocal8Bit(data);
    }
    return text;
}

QString normalizeExtractedText(QString text) {
    // Keep the model prompt compact and predictable: control characters and
    // noisy whitespace hurt JSON generation more than they help summarization.
    text.replace(QRegularExpression(QStringLiteral("[\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f]")), QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("[ \\t\\r]+")), QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("\\n{3,}")), QStringLiteral("\n\n"));
    return text.trimmed();
}

QString stripXmlText(const QByteArray& xml) {
    QStringList chunks;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isCharacters() && !reader.isWhitespace()) {
            chunks << reader.text().toString();
        }
    }
    return normalizeExtractedText(chunks.join(QStringLiteral(" ")));
}

QString extractDocxText(const QString& filePath, QString* errorMessage) {
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        setError(errorMessage, QStringLiteral("无法创建临时目录来解析 Word 文档。"));
        return {};
    }

    // Expand-Archive is happier with a .zip suffix, so copy the .docx into a
    // temporary archive name before reading word/document.xml.
    const QString archivePath = QDir(tempDir.path()).filePath(QStringLiteral("document.zip"));
    if (!QFile::copy(filePath, archivePath)) {
        setError(errorMessage, QStringLiteral("无法准备 Word 文档临时副本。"));
        return {};
    }

    QProcess process;
    process.setProgram(QStringLiteral("powershell.exe"));
    process.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral("Expand-Archive -LiteralPath $args[0] -DestinationPath $args[1] -Force"),
        archivePath,
        tempDir.path()
    });
    process.start();
    if (!process.waitForFinished(25000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        setError(errorMessage, QStringLiteral("无法解压 Word 文档，请确认它是有效的 .docx 文件。"));
        return {};
    }

    QFile documentXml(QDir(tempDir.path()).filePath(QStringLiteral("word/document.xml")));
    if (!documentXml.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Word 文档结构不完整，找不到正文内容。"));
        return {};
    }

    return stripXmlText(documentXml.readAll());
}

QString decodePdfLiteral(QString value) {
    value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    value.replace(QStringLiteral("\\r"), QStringLiteral("\n"));
    value.replace(QStringLiteral("\\t"), QStringLiteral(" "));
    value.replace(QStringLiteral("\\("), QStringLiteral("("));
    value.replace(QStringLiteral("\\)"), QStringLiteral(")"));
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return value;
}

QString extractPdfWithTool(const QString& filePath) {
    const QString pdftotext = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (pdftotext.isEmpty()) {
        return {};
    }

    QTemporaryFile outFile(QDir::tempPath() + QStringLiteral("/mindpalace_pdf_XXXXXX.txt"));
    if (!outFile.open()) {
        return {};
    }
    const QString outPath = outFile.fileName();
    outFile.close();

    QProcess process;
    process.setProgram(pdftotext);
    process.setArguments({QStringLiteral("-layout"), filePath, outPath});
    process.start();
    if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return {};
    }

    QFile extracted(outPath);
    if (!extracted.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(extracted.readAll());
}

QString extractPdfBestEffort(const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("无法读取 PDF 文件：%1").arg(file.errorString()));
        return {};
    }

    const QByteArray data = file.readAll();
    if (!data.startsWith("%PDF-")) {
        setError(errorMessage, QStringLiteral("文件头不是有效的 PDF 格式。"));
        return {};
    }

    QString text = extractPdfWithTool(filePath);
    if (!text.trimmed().isEmpty()) {
        return normalizeExtractedText(text);
    }

    const QString latin = QString::fromLatin1(data);
    QStringList chunks;
    QRegularExpression literalText(QStringLiteral("\\((?:\\\\.|[^\\\\)]){2,}\\)"));
    auto it = literalText.globalMatch(latin);
    while (it.hasNext()) {
        QString item = it.next().captured(0);
        item = item.mid(1, item.size() - 2);
        item = decodePdfLiteral(item);
        const int letters = std::count_if(item.begin(), item.end(), [](QChar c) {
            return c.isLetterOrNumber();
        });
        if (letters >= 2) {
            chunks << item;
        }
        if (chunks.join(QString()).size() > kMaxPromptChars) {
            break;
        }
    }

    return normalizeExtractedText(chunks.join(QStringLiteral(" ")));
}

QString boundedPromptText(const QString& text) {
    if (text.size() <= kMaxPromptChars) {
        return text;
    }
    return text.left(kMaxPromptChars) + QStringLiteral("\n\n[内容过长，后续文本已截断]");
}

QString joinUrl(const QString& baseUrl, const QString& path) {
    QString base = baseUrl.trimmed();
    while (base.endsWith('/')) base.chop(1);
    return base + path;
}

QString parseOpenAiContent(const QByteArray& data, QString* errorMessage) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(errorMessage, QStringLiteral("AI 返回了无法解析的 JSON 响应。"));
        return {};
    }

    const QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        const QJsonObject error = root.value(QStringLiteral("error")).toObject();
        const QString message = error.value(QStringLiteral("message")).toString(QStringLiteral("AI 服务返回错误。"));
        setError(errorMessage, message);
        return {};
    }

    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        setError(errorMessage, QStringLiteral("AI 响应中没有生成内容。"));
        return {};
    }
    return choices.first().toObject()
            .value(QStringLiteral("message")).toObject()
            .value(QStringLiteral("content")).toString().trimmed();
}

QString parseGeminiContent(const QByteArray& data, QString* errorMessage) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(errorMessage, QStringLiteral("Gemini 返回了无法解析的 JSON 响应。"));
        return {};
    }

    const QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        const QString message = root.value(QStringLiteral("error")).toObject()
                .value(QStringLiteral("message")).toString(QStringLiteral("Gemini 服务返回错误。"));
        setError(errorMessage, message);
        return {};
    }

    const QJsonArray candidates = root.value(QStringLiteral("candidates")).toArray();
    if (candidates.isEmpty()) {
        setError(errorMessage, QStringLiteral("Gemini 响应中没有生成内容。"));
        return {};
    }

    const QJsonArray parts = candidates.first().toObject()
            .value(QStringLiteral("content")).toObject()
            .value(QStringLiteral("parts")).toArray();
    QStringList chunks;
    for (const QJsonValue& part : parts) {
        const QString text = part.toObject().value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) chunks << text;
    }
    return chunks.join(QStringLiteral("\n")).trimmed();
}

QString extractJsonObjectText(QString content) {
    content = content.trimmed();
    if (content.startsWith(QStringLiteral("```"))) {
        content.remove(QRegularExpression(QStringLiteral("^```(?:json)?\\s*")));
        content.remove(QRegularExpression(QStringLiteral("\\s*```$")));
    }

    const int start = content.indexOf('{');
    const int end = content.lastIndexOf('}');
    if (start >= 0 && end > start) {
        return content.mid(start, end - start + 1);
    }
    return content;
}

bool parseDeckJson(const QString& content, AiImportResult* outResult, QString* errorMessage) {
    // The model is instructed to return JSON, but this still tolerates a fenced
    // code block or short prose wrapper and then validates every card pair.
    const QByteArray jsonData = extractJsonObjectText(content).toUtf8();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(errorMessage, QStringLiteral("AI 结果不是有效的牌组 JSON，请重试或换一个模型。"));
        return false;
    }

    const QJsonObject root = doc.object();
    AiImportResult result;
    result.deckName = root.value(QStringLiteral("deckName")).toString().trimmed();
    result.summary = root.value(QStringLiteral("summary")).toString().trimmed();

    const QJsonArray cards = root.value(QStringLiteral("cards")).toArray();
    for (const QJsonValue& value : cards) {
        const QJsonObject card = value.toObject();
        const QString front = card.value(QStringLiteral("front")).toString().trimmed();
        const QString back = card.value(QStringLiteral("back")).toString().trimmed();
        if (!front.isEmpty() && !back.isEmpty()) {
            result.cards.push_back({QString(), front, back});
        }
        if (result.cards.size() >= 80) {
            break;
        }
    }

    if (result.cards.empty()) {
        setError(errorMessage, QStringLiteral("AI 没有生成有效卡片。"));
        return false;
    }

    *outResult = std::move(result);
    return true;
}

} // namespace

AiAssistantSettings AiAssistantService::loadSettings() {
    QSettings settings(QStringLiteral("MindPalace"), QStringLiteral("Settings"));
    AiAssistantSettings result;
    result.provider = settings.value(QStringLiteral("ai/provider"), result.provider).toString();
    result.apiKey = settings.value(QStringLiteral("ai/apiKey")).toString();
    result.baseUrl = settings.value(QStringLiteral("ai/baseUrl"),
                                    defaultBaseUrlForProvider(result.provider)).toString();
    result.model = settings.value(QStringLiteral("ai/model"),
                                  defaultModelForProvider(result.provider)).toString();
    return result;
}

void AiAssistantService::saveSettings(const AiAssistantSettings& settingsValue) {
    QSettings settings(QStringLiteral("MindPalace"), QStringLiteral("Settings"));
    settings.setValue(QStringLiteral("ai/provider"), settingsValue.provider);
    settings.setValue(QStringLiteral("ai/apiKey"), settingsValue.apiKey);
    settings.setValue(QStringLiteral("ai/baseUrl"), settingsValue.baseUrl);
    settings.setValue(QStringLiteral("ai/model"), settingsValue.model);
}

QString AiAssistantService::defaultBaseUrlForProvider(const QString& provider) {
    if (provider == QStringLiteral("deepseek")) return QStringLiteral("https://api.deepseek.com");
    if (provider == QStringLiteral("gemini")) return QStringLiteral("https://generativelanguage.googleapis.com/v1beta");
    return QStringLiteral("https://api.openai.com/v1");
}

QString AiAssistantService::defaultModelForProvider(const QString& provider) {
    if (provider == QStringLiteral("deepseek")) return QStringLiteral("deepseek-chat");
    if (provider == QStringLiteral("gemini")) return QStringLiteral("gemini-1.5-flash");
    return QStringLiteral("gpt-4.1-mini");
}

bool AiAssistantService::validateSettings(const AiAssistantSettings& settingsValue, QString* errorMessage) {
    if (settingsValue.apiKey.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("请先输入 API 密钥。"));
        return false;
    }
    if (settingsValue.model.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("请填写模型名称。"));
        return false;
    }
    const QUrl url(settingsValue.baseUrl.trimmed());
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        setError(errorMessage, QStringLiteral("Base URL 无效，请填写完整的 https:// 地址。"));
        return false;
    }
    return true;
}

bool AiAssistantService::testConnection(const AiAssistantSettings& settingsValue, QString* errorMessage) {
    QString ignored;
    return callModel(settingsValue,
                     QStringLiteral("请只回复 OK，用于测试 API 密钥是否可用。"),
                     &ignored,
                     errorMessage,
                     true);
}

bool AiAssistantService::extractDocumentText(const QString& filePath,
                                             QString* outText,
                                             QString* errorMessage) {
    if (!outText) return false;
    outText->clear();

    // Validate the local file before any AI request; bad inputs should fail
    // quickly and never consume API quota.
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        setError(errorMessage, QStringLiteral("文件不存在。"));
        return false;
    }
    if (info.size() <= 0) {
        setError(errorMessage, QStringLiteral("文件为空。"));
        return false;
    }
    if (info.size() > kMaxDocumentBytes) {
        setError(errorMessage, QStringLiteral("文件超过 20MB，请拆分后再导入。"));
        return false;
    }

    const QString suffix = info.suffix().toLower();
    QString text;
    if (suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")) {
        text = readTextFileUtf8(filePath, errorMessage);
    } else if (suffix == QStringLiteral("docx")) {
        text = extractDocxText(filePath, errorMessage);
    } else if (suffix == QStringLiteral("doc")) {
        setError(errorMessage, QStringLiteral("暂不支持旧版 .doc 二进制文档，请另存为 .docx 后导入。"));
        return false;
    } else if (suffix == QStringLiteral("pdf")) {
        text = extractPdfBestEffort(filePath, errorMessage);
    } else {
        setError(errorMessage, QStringLiteral("仅支持 PDF、Word(.docx) 和 Markdown 文件。"));
        return false;
    }

    text = normalizeExtractedText(text);
    if (text.size() < 40) {
        setError(errorMessage, QStringLiteral("未能从文档中抽取到足够文本。若是扫描版 PDF，请先 OCR 或转为可复制文本。"));
        return false;
    }

    *outText = boundedPromptText(text);
    return true;
}

bool AiAssistantService::generateDeckFromDocument(const QString& filePath,
                                                  const AiAssistantSettings& settingsValue,
                                                  AiImportResult* outResult,
                                                  QString* errorMessage) {
    if (!outResult) return false;
    if (!validateSettings(settingsValue, errorMessage)) {
        return false;
    }

    QString documentText;
    if (!extractDocumentText(filePath, &documentText, errorMessage)) {
        return false;
    }

    const QFileInfo info(filePath);
    const QString prompt = QStringLiteral(
        "你是记忆卡片制作助手。请阅读下面文档，为间隔复习软件生成高质量双面卡片。\n"
        "要求：\n"
        "1. 只输出一个 JSON 对象，不要 Markdown 代码块。\n"
        "2. JSON 格式：{\"deckName\":\"...\",\"summary\":\"...\",\"cards\":[{\"front\":\"问题或概念\",\"back\":\"简洁答案或解释\"}]}。\n"
        "3. deckName 使用文档主题，cards 生成 15 到 40 张，正反面都不能为空。\n"
        "4. front 应适合主动回忆，back 要准确、简洁，可包含必要例子。\n\n"
        "来源文件：%1\n\n文档内容：\n%2")
        .arg(info.fileName(), documentText);

    QString content;
    if (!callModel(settingsValue, prompt, &content, errorMessage, false)) {
        return false;
    }

    AiImportResult result;
    if (!parseDeckJson(content, &result, errorMessage)) {
        return false;
    }

    result.sourceFileName = info.fileName();
    if (result.deckName.isEmpty()) {
        result.deckName = info.completeBaseName();
    }

    *outResult = std::move(result);
    return true;
}

bool AiAssistantService::callModel(const AiAssistantSettings& settingsValue,
                                   const QString& userPrompt,
                                   QString* outContent,
                                   QString* errorMessage,
                                   bool smallProbe) {
    if (!validateSettings(settingsValue, errorMessage)) {
        return false;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request;
    QJsonObject payload;
    const bool gemini = settingsValue.provider == QStringLiteral("gemini");

    // Gemini uses a different REST shape; everything else goes through the
    // OpenAI-compatible chat/completions contract.
    if (gemini) {
        QString model = settingsValue.model.trimmed();
        const QString path = QStringLiteral("/models/%1:generateContent?key=%2")
                .arg(model, QString::fromUtf8(QUrl::toPercentEncoding(settingsValue.apiKey.trimmed())));
        request.setUrl(QUrl(joinUrl(settingsValue.baseUrl, path)));

        QJsonObject part;
        part[QStringLiteral("text")] = userPrompt;
        QJsonObject content;
        content[QStringLiteral("parts")] = QJsonArray{part};
        payload[QStringLiteral("contents")] = QJsonArray{content};
        payload[QStringLiteral("generationConfig")] = QJsonObject{
            {QStringLiteral("temperature"), smallProbe ? 0.0 : 0.2},
            {QStringLiteral("maxOutputTokens"), smallProbe ? 16 : 4096}
        };
    } else {
        request.setUrl(QUrl(joinUrl(settingsValue.baseUrl, QStringLiteral("/chat/completions"))));
        request.setRawHeader("Authorization", QByteArray("Bearer ") + settingsValue.apiKey.trimmed().toUtf8());

        QJsonArray messages;
        messages.append(QJsonObject{
            {QStringLiteral("role"), QStringLiteral("system")},
            {QStringLiteral("content"), QStringLiteral("你是可靠的学习卡片生成助手，输出必须可被程序解析。")}
        });
        messages.append(QJsonObject{
            {QStringLiteral("role"), QStringLiteral("user")},
            {QStringLiteral("content"), userPrompt}
        });
        payload[QStringLiteral("model")] = settingsValue.model.trimmed();
        payload[QStringLiteral("messages")] = messages;
        payload[QStringLiteral("temperature")] = smallProbe ? 0.0 : 0.2;
        payload[QStringLiteral("max_tokens")] = smallProbe ? 16 : 4096;
    }

    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(smallProbe ? 20000 : 90000);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    QNetworkReply* reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(smallProbe ? 22000 : 95000);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        setError(errorMessage, QStringLiteral("AI 请求超时，请检查网络或模型配置。"));
        return false;
    }

    const QByteArray data = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
        QString serviceMessage;
        if (gemini) {
            serviceMessage = parseGeminiContent(data, nullptr);
        } else {
            serviceMessage = parseOpenAiContent(data, nullptr);
        }
        if (serviceMessage.isEmpty()) {
            serviceMessage = QString::fromUtf8(data).left(400);
        }
        setError(errorMessage, QStringLiteral("AI 请求失败（HTTP %1）：%2").arg(statusCode).arg(serviceMessage));
        return false;
    }

    QString content = gemini ? parseGeminiContent(data, errorMessage)
                             : parseOpenAiContent(data, errorMessage);
    if (content.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("AI 没有返回文本内容。"));
        return false;
    }

    if (outContent) {
        *outContent = content;
    }
    return true;
}

} // namespace MindPalace::Service
