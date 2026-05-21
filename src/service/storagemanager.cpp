#include "storagemanager.h"

#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace MindPalace::Service {

bool StorageManager::saveDeck(const Model::Deck& deck, const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QJsonDocument document(deck.toJson());
    const QByteArray data = document.toJson(QJsonDocument::Indented);

    if (file.write(data) != data.size()) {
        return false;
    }

    return file.flush();
}

bool StorageManager::loadDeck(const QString& filePath, Model::Deck& deck) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    deck.fromJson(document.object());
    return true;
}

} // namespace MindPalace::Service
