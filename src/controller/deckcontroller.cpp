#include "deckcontroller.h"
#include "service/storagemanager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
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
    emit signal_deckListChanged();
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
            emit signal_deckListChanged();
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

    emit signal_deckListChanged();
    return true;
}

const std::vector<Model::Deck>& DeckController::getDecks() const {
    return decks;
}

bool DeckController::resetDeck(const QString& deckName) {
    const QString cleanedName = deckName.trimmed();
    if (cleanedName.isEmpty()) {
        qWarning() << "resetDeck failed: deck name is empty after trimming.";
        return false;
    }

    Model::Deck* deck = findDeckByName(cleanedName);
    if (!deck) {
        qWarning() << "resetDeck failed: deck does not exist:" << cleanedName;
        return false;
    }

    if (deck->deckId.isEmpty()) {
        qWarning() << "resetDeck failed: deck id is empty.";
        return false;
    }

    const QDate today = QDate::currentDate();
    for (auto& cardPtr : deck->cards) {
        if (!cardPtr) continue;
        cardPtr->repetitions   = 0;
        cardPtr->interval      = 0.0f;
        cardPtr->easeFactor    = 2.5f;
        cardPtr->lastReviewed  = today;
        cardPtr->nextReviewDate = today;
    }

    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(*deck, deckFilePath(deck->deckId), &error)) {
        qWarning() << "resetDeck failed: unable to save deck file:" << error.message;
        return false;
    }

    emit signal_deckListChanged();
    return true;
}

bool DeckController::addCardToDeck(const QString& deckName, const QString& front, const QString& back) {
    const QString cleanedName = deckName.trimmed();
    if (cleanedName.isEmpty()) {
        qWarning() << "addCardToDeck failed: deck name is empty after trimming.";
        return false;
    }

    // 1. 在内存中找到目标牌组
    Model::Deck* deck = findDeckByName(cleanedName);
    if (!deck) {
        qWarning() << "addCardToDeck failed: deck does not exist:" << cleanedName;
        return false;
    }

    if (deck->deckId.isEmpty()) {
        qWarning() << "addCardToDeck failed: deck id is empty.";
        return false;
    }

    if (front.trimmed().isEmpty() || back.trimmed().isEmpty()) {
        qWarning() << "addCardToDeck notice: card front or back is empty.";
    }

    // 2. Card 构造函数负责初始化正反面文本和 SM-2 初始状态。
    auto newCard = std::make_unique<Model::Card>(front, back);

    // 3. 通过 Deck 的封装接口转移卡片所有权。
    deck->addCard(std::move(newCard));

    // 4. 触发 StorageManager 进行持久化落盘
    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(*deck, deckFilePath(deck->deckId), &error)) {
        // 如果落盘失败，必须进行内存回滚，保证内存与硬盘一致！
        deck->cards.pop_back();
        qWarning() << "addCardToDeck failed: unable to save deck file:" << error.message;
        return false;
    }

    emit signal_deckListChanged();
    return true;
}

bool DeckController::deleteCardFromDeck(const QString& deckName, const QString& cardId) {
    const QString cleanedName = deckName.trimmed();
    if (cleanedName.isEmpty()) {
        qWarning() << "deleteCardFromDeck failed: deck name is empty after trimming.";
        return false;
    }

    const QString cleanedCardId = cardId.trimmed();
    if (cleanedCardId.isEmpty()) {
        qWarning() << "deleteCardFromDeck failed: card id is empty after trimming.";
        return false;
    }

    // 1. 找到目标牌组。删除卡片属于牌组内部数据变更，必须先定位到内存中的 Deck。
    Model::Deck* deck = findDeckByName(cleanedName);
    if (!deck) {
        qWarning() << "deleteCardFromDeck failed: deck does not exist:" << cleanedName;
        return false;
    }

    if (deck->deckId.isEmpty()) {
        qWarning() << "deleteCardFromDeck failed: deck id is empty.";
        return false;
    }

    auto cardIterator = std::find_if(deck->cards.begin(), deck->cards.end(),
        [&cleanedCardId](const std::unique_ptr<Model::Card>& cardPtr) {
            return cardPtr && cardPtr->id == cleanedCardId;
        });

    if (cardIterator == deck->cards.end()) {
        qWarning() << "deleteCardFromDeck failed: card does not exist:" << cleanedCardId;
        return false;
    }

    // 2. 先从内存移除，再尝试落盘；若保存失败，必须插回原位置保持内存与磁盘一致。
    const auto cardIndex = std::distance(deck->cards.begin(), cardIterator);
    auto removedCard = std::move(*cardIterator);
    deck->cards.erase(cardIterator);

    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(*deck, deckFilePath(deck->deckId), &error)) {
        deck->cards.insert(deck->cards.begin() + cardIndex, std::move(removedCard));
        qWarning() << "deleteCardFromDeck failed: unable to save deck file:" << error.message;
        return false;
    }

    emit signal_deckListChanged();
    return true;
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

bool DeckController::updateCard(const QString& deckName, const QString& cardId,
                                const QString& newFront, const QString& newBack) {
    const QString cleanedDeckName = deckName.trimmed();
    const QString cleanedCardId   = cardId.trimmed();
    const QString cleanedFront    = newFront.trimmed();
    const QString cleanedBack     = newBack.trimmed();

    if (cleanedDeckName.isEmpty() || cleanedCardId.isEmpty()) {
        qWarning() << "updateCard failed: deck name or card id is empty.";
        return false;
    }
    if (cleanedFront.isEmpty() || cleanedBack.isEmpty()) {
        qWarning() << "updateCard failed: front or back text is empty after trimming.";
        return false;
    }

    Model::Deck* deck = findDeckByName(cleanedDeckName);
    if (!deck) {
        qWarning() << "updateCard failed: deck does not exist:" << cleanedDeckName;
        return false;
    }
    if (deck->deckId.isEmpty()) {
        qWarning() << "updateCard failed: deck id is empty.";
        return false;
    }

    auto cardIterator = std::find_if(deck->cards.begin(), deck->cards.end(),
        [&cleanedCardId](const std::unique_ptr<Model::Card>& cardPtr) {
            return cardPtr && cardPtr->id == cleanedCardId;
        });
    if (cardIterator == deck->cards.end()) {
        qWarning() << "updateCard failed: card does not exist:" << cleanedCardId;
        return false;
    }

    Model::Card* card = cardIterator->get();
    const QString oldFront = card->front;
    const QString oldBack  = card->back;
    card->front = cleanedFront;
    card->back  = cleanedBack;

    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(*deck, deckFilePath(deck->deckId), &error)) {
        card->front = oldFront;
        card->back  = oldBack;
        qWarning() << "updateCard failed: unable to save deck file:" << error.message;
        return false;
    }

    emit signal_deckListChanged();
    return true;
}

bool DeckController::importDeckFromFile(const QString& sourceFilePath) {
    const QFileInfo inInfo(sourceFilePath);
    if (!inInfo.exists()) {
        qWarning() << "importDeckFromFile failed: .in file not found:" << sourceFilePath;
        return false;
    }

    const QString outPath = inInfo.dir().filePath(inInfo.completeBaseName() + ".out");
    if (!QFile::exists(outPath)) {
        qWarning() << "importDeckFromFile failed: matching .out file not found:" << outPath;
        return false;
    }

    QFile inFile(sourceFilePath);
    QFile outFile(outPath);
    if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text) ||
        !outFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "importDeckFromFile failed: unable to open file pair";
        return false;
    }

    QTextStream inStream(&inFile);
    QTextStream outStream(&outFile);
    inStream.setEncoding(QStringConverter::Utf8);
    outStream.setEncoding(QStringConverter::Utf8);

    QStringList fronts, backs;
    while (!inStream.atEnd()) fronts.append(inStream.readLine());
    while (!outStream.atEnd()) backs.append(outStream.readLine());

    if (fronts.isEmpty() || backs.isEmpty()) {
        qWarning() << "importDeckFromFile failed: one or both files are empty";
        return false;
    }

    const QString baseName = inInfo.completeBaseName();
    QString uniqueName = baseName;
    for (int n = 1; deckNameExists(uniqueName); ++n)
        uniqueName = QString("%1 (%2)").arg(baseName).arg(n);

    Model::Deck deck(uniqueName);
    deck.deckId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const int count = qMin(fronts.size(), backs.size());
    for (int i = 0; i < count; ++i) {
        const QString front = fronts[i].trimmed();
        const QString back  = backs[i].trimmed();
        if (front.isEmpty() && back.isEmpty()) continue;
        deck.addCard(std::make_unique<Model::Card>(front, back));
    }

    if (deck.cards.empty()) {
        qWarning() << "importDeckFromFile failed: no valid card pairs after parsing";
        return false;
    }

    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(deck, deckFilePath(deck.deckId), &error)) {
        qWarning() << "importDeckFromFile failed: unable to save deck:" << error.message;
        return false;
    }

    decks.push_back(std::move(deck));
    emit signal_deckListChanged();
    return true;
}

} // namespace MindPalace::Controller
