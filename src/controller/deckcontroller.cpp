#include "deckcontroller.h"
#include "service/storagemanager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

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
        qWarning() << "loadDecks failed: unable to create or access deck directory:"
                   << decksDirPath;
        return;
    }
    decksDirectory.setPath(decksDirPath);

    const QFileInfoList deckFiles =
        decksDirectory.entryInfoList({"*.json"}, QDir::Files, QDir::Name);
    Service::StorageError error;
    for (const QFileInfo& deckFile : deckFiles) {
        Model::Deck deck;
        if (!Service::StorageManager::loadDeck(deckFile.absoluteFilePath(), deck, &error)) {
            qWarning() << "loadDecks skipped file:" << deckFile.absoluteFilePath()
                       << error.message;
            continue;
        }

        deck.deckId = deckFile.completeBaseName();
        decks.push_back(std::move(deck));
    }
}

bool DeckController::createDeck(const QString& name) {
    const QString cleanedName = name.trimmed();
    if (cleanedName.isEmpty()) {
        qWarning() << "createDeck failed: deck name is empty after trimming.";
        return false;
    }

    if (deckNameExists(cleanedName)) {
        qWarning() << "createDeck failed: deck name already exists:" << cleanedName;
        return false;
    }

    Model::Deck deck(cleanedName);
    deck.deckId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(deck, deckFilePath(deck.deckId), &error)) {
        // 保存失败时不加入内存，避免界面出现磁盘中不存在的牌组。
        qWarning() << "createDeck failed: unable to save deck file:" << error.message;
        return false;
    }

    decks.push_back(std::move(deck));
    return true;
}

bool DeckController::deleteDeck(const QString& deckName) {
    const QString cleanedName = deckName.trimmed();
    if (cleanedName.isEmpty()) {
        qWarning() << "deleteDeck failed: deck name is empty after trimming.";
        return false;
    }

    Model::Deck* deck = findDeckByName(cleanedName);
    if (!deck) {
        qWarning() << "deleteDeck failed: deck does not exist:" << cleanedName;
        return false;
    }

    const QString filePath = deckFilePath(deck->deckId);
    if (filePath.isEmpty()) {
        qWarning() << "deleteDeck failed: deck id is empty.";
        return false;
    }

    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            qWarning() << "deleteDeck failed: unable to remove deck file:" << filePath;
            return false;
        }
    } else {
        qWarning() << "deleteDeck notice: deck file does not exist:" << filePath;
    }

    for (auto iterator = decks.begin(); iterator != decks.end(); ++iterator) {
        if (&(*iterator) == deck) {
            decks.erase(iterator);
            return true;
        }
    }

    return false;
}

bool DeckController::renameDeck(const QString& deckName, const QString& newName) {
    const QString cleanedDeckName = deckName.trimmed();
    const QString cleanedNewName = newName.trimmed();
    if (cleanedDeckName.isEmpty() || cleanedNewName.isEmpty()) {
        qWarning() << "renameDeck failed: a deck name is empty after trimming.";
        return false;
    }

    Model::Deck* deck = findDeckByName(cleanedDeckName);
    if (!deck) {
        qWarning() << "renameDeck failed: deck does not exist:" << cleanedDeckName;
        return false;
    }

    if (deck->deckName == cleanedNewName) {
        return true;
    }

    if (deckNameExists(cleanedNewName)) {
        qWarning() << "renameDeck failed: deck name already exists:" << cleanedNewName;
        return false;
    }

    if (deck->deckId.isEmpty()) {
        qWarning() << "renameDeck failed: deck id is empty.";
        return false;
    }

    const QString oldName = deck->deckName;
    deck->deckName = cleanedNewName;
    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(*deck, deckFilePath(deck->deckId), &error)) {
        deck->deckName = oldName;
        qWarning() << "renameDeck failed: unable to save deck file:" << error.message;
        return false;
    }

    return true;
}

const std::vector<Model::Deck>& DeckController::getDecks() const {
    return decks;
}

Model::Deck* DeckController::findDeckByName(const QString& deckName) {
    if (deckName.isEmpty()) {
        return nullptr;
    }

    for (Model::Deck& deck : decks) {
        if (deck.deckName == deckName) {
            return &deck;
        }
    }

    return nullptr;
}

const Model::Deck* DeckController::findDeckByName(const QString& deckName) const {
    if (deckName.isEmpty()) {
        return nullptr;
    }

    for (const Model::Deck& deck : decks) {
        if (deck.deckName == deckName) {
            return &deck;
        }
    }

    return nullptr;
}

bool DeckController::deckNameExists(const QString& deckName) const {
    if (deckName.isEmpty()) {
        return false;
    }

    return findDeckByName(deckName) != nullptr;
}

QString DeckController::deckFilePath(const QString& deckId) const {
    if (deckId.isEmpty()) {
        return {};
    }

    return QDir(decksDirPath).filePath(deckId + ".json");
}

} // namespace MindPalace::Controller
