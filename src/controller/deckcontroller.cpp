#include "deckcontroller.h"
#include "service/storagemanager.h"

#include <QDir>
#include <QFileInfo>

#include <utility>

namespace MindPalace::Controller {

DeckController::DeckController(const QString& decksDirPath)
    : decksDirPath(decksDirPath) {
    loadDecks();
}

void DeckController::loadDecks() {
    decks.clear();

    QDir decksDirectory;
    if (!decksDirectory.mkpath(decksDirPath)) {
        return;
    }
    decksDirectory.setPath(decksDirPath);

    const QFileInfoList deckFiles =
        decksDirectory.entryInfoList({"*.json"}, QDir::Files, QDir::Name);
    for (const QFileInfo& deckFile : deckFiles) {
        Model::Deck deck;
        if (!Service::StorageManager::loadDeck(deckFile.absoluteFilePath(), deck)) {
            continue;
        }

        deck.deckId = deckFile.completeBaseName();
        decks.push_back(std::move(deck));
    }
}

Model::Deck* DeckController::findDeckByName(const QString& deckName) {
    for (Model::Deck& deck : decks) {
        if (deck.deckName == deckName) {
            return &deck;
        }
    }

    return nullptr;
}

const Model::Deck* DeckController::findDeckByName(const QString& deckName) const {
    for (const Model::Deck& deck : decks) {
        if (deck.deckName == deckName) {
            return &deck;
        }
    }

    return nullptr;
}

bool DeckController::deckNameExists(const QString& deckName) const {
    return findDeckByName(deckName) != nullptr;
}

QString DeckController::deckFilePath(const QString& deckId) const {
    return QDir(decksDirPath).filePath(deckId + ".json");
}

} // namespace MindPalace::Controller
