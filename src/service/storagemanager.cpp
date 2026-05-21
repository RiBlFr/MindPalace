#include "storagemanager.h"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <utility>

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

} // namespace MindPalace::Service
