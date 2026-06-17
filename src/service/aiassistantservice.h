#ifndef MINDPALACE_AIASSISTANTSERVICE_H
#define MINDPALACE_AIASSISTANTSERVICE_H

#include "view/CardManagerDialog.h"

#include <QString>
#include <vector>

namespace MindPalace::Service {

struct AiAssistantSettings {
    QString provider = QStringLiteral("openai-compatible");
    QString apiKey;
    QString baseUrl = QStringLiteral("https://api.openai.com/v1");
    QString model = QStringLiteral("gpt-4.1-mini");
};

struct AiImportResult {
    QString deckName;
    QString summary;
    QString sourceFileName;
    std::vector<CardDisplayInfo> cards;
};

class AiAssistantService {
public:
    AiAssistantService() = delete;

    static AiAssistantSettings loadSettings();
    static void saveSettings(const AiAssistantSettings& settings);

    static QString defaultBaseUrlForProvider(const QString& provider);
    static QString defaultModelForProvider(const QString& provider);

    static bool validateSettings(const AiAssistantSettings& settings, QString* errorMessage);
    static bool testConnection(const AiAssistantSettings& settings, QString* errorMessage);

    static bool extractDocumentText(const QString& filePath,
                                    QString* outText,
                                    QString* errorMessage);

    static bool generateDeckFromDocument(const QString& filePath,
                                         const AiAssistantSettings& settings,
                                         AiImportResult* outResult,
                                         QString* errorMessage);

private:
    static bool callModel(const AiAssistantSettings& settings,
                          const QString& userPrompt,
                          QString* outContent,
                          QString* errorMessage,
                          bool smallProbe);
};

} // namespace MindPalace::Service

#endif // MINDPALACE_AIASSISTANTSERVICE_H
