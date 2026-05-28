#include "deckcontroller.h"
#include "service/storagemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace MindPalace::Controller {

DeckController::DeckController(const QString& decksDirPath, QObject* parent)
    : QObject(parent), decksDirPath(decksDirPath) {
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

bool DeckController::createDeck(const QString& name) {
    if (name.isEmpty() || deckNameExists(name)) {
        return false;
    }

    Model::Deck deck(name);
    deck.deckId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (!Service::StorageManager::saveDeck(deck, deckFilePath(deck.deckId))) {
        return false;
    }

    decks.push_back(std::move(deck));
    emit signal_deckListChanged();
    return true;
}

bool DeckController::deleteDeck(const QString& deckName) {
    const Model::Deck* deck = findDeckByName(deckName);
    if (!deck) {
        return false;
    }

    const QString filePath = deckFilePath(deck->deckId);
    if (!QFile::remove(filePath)) {
        return false;
    }

    decks.erase(
        std::remove_if(decks.begin(), decks.end(),
            [&deckName](const Model::Deck& d) { return d.deckName == deckName; }),
        decks.end());

    emit signal_deckListChanged();
    return true;
}

bool DeckController::renameDeck(const QString& deckName, const QString& newName) {
    if (newName.isEmpty() || deckNameExists(newName)) {
        return false;
    }

    Model::Deck* deck = findDeckByName(deckName);
    if (!deck) {
        return false;
    }

    deck->deckName = newName;

    if (!Service::StorageManager::saveDeck(*deck, deckFilePath(deck->deckId))) {
        deck->deckName = deckName; // rollback
        return false;
    }

    emit signal_deckListChanged();
    return true;
}

const std::vector<Model::Deck>& DeckController::getDecks() const {
    return decks;
}

bool DeckController::resetDeck(const QString& deckName) {
    Model::Deck* deck = findDeckByName(deckName);
    if (!deck) return false;

    const QDate today = QDate::currentDate();
    for (auto& cardPtr : deck->cards) {
        if (!cardPtr) continue;
        cardPtr->repetitions   = 0;
        cardPtr->interval      = 0.0f;
        cardPtr->easeFactor    = 2.5f;
        cardPtr->lastReviewed  = today;
        cardPtr->nextReviewDate = today;
    }

    return Service::StorageManager::saveDeck(*deck, deckFilePath(deck->deckId));
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
