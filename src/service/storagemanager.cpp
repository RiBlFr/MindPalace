#include "storagemanager.h"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <utility>
#include <QDate>
#include <QCoreApplication>

namespace MindPalace::Service {

namespace {

void clearError(StorageError* error) {
    if (error) {
        *error = {};
    }
}

void setError(StorageError* error, StorageErrorType type, const QString& context,
              const QString& message, qint64 offset = -1) {
    if (error) {
        error->type = type;
        error->context = context;
        error->message = message;
        error->offset = offset;
    }
}

} // namespace


bool StorageManager::saveDeck(const Model::Deck& deck, const QString& filePath, StorageError* error) {
    clearError(error);

    const QString tempFilePath = filePath + ".tmp";
    QFile file(tempFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, StorageErrorType::OpenFailed, "open temp file for writing",
                 file.errorString());
        return false;
    }

    const QJsonDocument document(deck.toJson());
    const QByteArray data = document.toJson(QJsonDocument::Indented);

    if (file.write(data) != data.size()) {
        setError(error, StorageErrorType::WriteFailed, "write complete JSON to temp file",
                 file.errorString());
        return false;
    }

    if (!file.flush()) {
        setError(error, StorageErrorType::WriteFailed, "flush temp file", file.errorString());
        return false;
    }

    file.close();
    if (file.error() != QFile::NoError) {
        setError(error, StorageErrorType::WriteFailed, "close temp file", file.errorString());
        return false;
    }

    const QString backupFilePath = filePath + ".bak";
    QFile::remove(backupFilePath);

    const bool hadExistingFile = QFileInfo::exists(filePath);
    if (hadExistingFile && !QFile::rename(filePath, backupFilePath)) {
        setError(error, StorageErrorType::ReplaceFailed, "move old JSON to backup",
                 filePath + " -> " + backupFilePath);
        return false;
    }

    if (!QFile::rename(tempFilePath, filePath)) {
        const QString replaceMessage = tempFilePath + " -> " + filePath;
        if (hadExistingFile) {
            QFile::rename(backupFilePath, filePath);
        }
        setError(error, StorageErrorType::ReplaceFailed, "rename temp file to JSON",
                 replaceMessage);
        return false;
    }

    if (hadExistingFile) {
        QFile::remove(backupFilePath);
    }

    return true;
}

bool StorageManager::saveDeck(const Model::Deck& deck, const QString& filePath) {
    return saveDeck(deck, filePath, nullptr);
}

bool StorageManager::loadDeck(const QString& filePath, Model::Deck& deck, StorageError* error) {
    clearError(error);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, StorageErrorType::OpenFailed, "open JSON for reading",
                 file.errorString());
        return false;
    }

    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError) {
        setError(error, StorageErrorType::ReadFailed, "read complete JSON file",
                 file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, StorageErrorType::ParseFailed, "parse JSON",
                 parseError.errorString(), parseError.offset);
        return false;
    }

    if (!document.isObject()) {
        setError(error, StorageErrorType::InvalidJsonRoot, "validate JSON root",
                 "root value must be a JSON object");
        return false;
    }

    Model::Deck tempDeck;
    tempDeck.fromJson(document.object());
    deck = std::move(tempDeck);
    return true;
}

bool StorageManager::loadDeck(const QString& filePath, Model::Deck& deck) {
    return loadDeck(filePath, deck, nullptr);
}
    void StorageManager::incrementDailyReviewCount() {
    // 1. 确定文件路径 (与你存卡组的 data/ 目录平级)
    QString logPath = QCoreApplication::applicationDirPath() + "/data/review_log.json";
    QJsonObject logObj;

    // 2. 如果文件存在，先读出旧账本
    QFile file(logPath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        logObj = doc.object();
        file.close();
    }

    // 3. 给今天的数字 +1
    QString todayStr = QDate::currentDate().toString("yyyy-MM-dd");
    int currentCount = logObj.value(todayStr).toInt(0);
    logObj[todayStr] = currentCount + 1;

    // 4. 写回硬盘
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(logObj).toJson());
        file.close();
    }
}

    void StorageManager::getWeeklyReviewData(std::vector<int>& outData, QStringList& outLabels) {
    outData.clear();
    outLabels.clear();

    QString logPath = QCoreApplication::applicationDirPath() + "/data/review_log.json";
    QJsonObject logObj;

    QFile file(logPath);
    if (file.open(QIODevice::ReadOnly)) {
        logObj = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QDate today = QDate::currentDate();

    // 往前推 6 天，加上今天，一共 7 天
    for (int i = 6; i >= 0; --i) {
        QDate d = today.addDays(-i);
        QString dateStr = d.toString("yyyy-MM-dd");

        // 获取那一天的复习量，没有记录就是 0
        outData.push_back(logObj.value(dateStr).toInt(0));

        // 生成漂亮的标签
        if (i == 0) {
            outLabels.append("今");
        } else {
            // 将 1-7 映射为中文星期
            QStringList weekNames = {"", "一", "二", "三", "四", "五", "六", "日"};
            outLabels.append("周" + weekNames[d.dayOfWeek()]);
        }
    }
}
} // namespace MindPalace::Service
