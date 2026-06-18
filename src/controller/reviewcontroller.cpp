#include "reviewcontroller.h"
#include "service/sm2engine.h"
#include "service/storagemanager.h"
#include <QSettings>
#include <random>
#include <algorithm>
#include <vector>
#include <QDebug>

namespace MindPalace::Controller {

// State access

ReviewController::ReviewController(QObject* parent)
    : QObject(parent) {
}

ReviewController::ReviewState ReviewController::currentState() const {
    return state;
}

Model::Card* ReviewController::currentCard() const {
    // Finished sessions should not expose a stale card to the view.
    if (state == ReviewState::FinishedState || reviewQueue.empty()) {
        return nullptr;
    }

    return reviewQueue.front();
}

// Review session

bool ReviewController::startReview(Model::Deck* deck, const QString& deckFilePath, bool forceReview) {
    // 参数无效：没有有效牌组时不能启动复习会话。
    if (!deck) {
        qWarning() << "startReview failed: deck is null.";
        return false;
    }

    // 参数无效：没有文件路径时后续评分无法安全保存。
    if (deckFilePath.trimmed().isEmpty()) {
        qWarning() << "startReview failed: deck file path is empty.";
        return false;
    }

    clearQueue();
    totalReviewCount = 0;
    state = ReviewState::FinishedState;

    // 进入新牌组会话：清零连胜并通知 View 收起上一牌组残留的“火热连胜”徽章。
    easyStreak = 0;
    emit signal_streakChanged(easyStreak);

    activeDeck = deck;
    activeDeckFilePath = deckFilePath;

    buildReviewQueue(forceReview);

    totalReviewCount = static_cast<int>(reviewQueue.size());

    if (reviewQueue.empty()) {
        qWarning() << "startReview notice: no due cards.";
        return false;
    }

    state = ReviewState::QuestionState;
    emit signal_showQuestion(reviewQueue.front()->front);
    return true;
}

    void ReviewController::buildReviewQueue(bool forceReview) {
    clearQueue();

    if (!activeDeck) {
        return;
    }

    // Collect candidates in a vector first so optional shuffling is simple.
    std::vector<Model::Card*> tempCards;

    for (const auto& card : activeDeck->cards) {
        if (card && (forceReview || card->isDue())) {
            tempCards.push_back(card.get());
        }
    }

    // Default to the deck's stored order unless the user enabled shuffling.
    QSettings settings("MindPalace", "Settings");
    bool isRandomShuffle = settings.value("shuffleReview", false).toBool();

    if (isRandomShuffle) {
        std::random_device rd;
        std::mt19937 generator(rd());
        std::shuffle(tempCards.begin(), tempCards.end(), generator);
    }

    // Queue keeps the rest of the state machine simple.
    for (Model::Card* c : tempCards) {
        reviewQueue.push(c);
    }
}

bool ReviewController::showAnswer() {
    // 幂等保护：答案已揭示时来回翻牌只是视觉动画，重复请求直接静默返回，
    // 既不重复发信号，也不应视为错误。
    if (state == ReviewState::AnswerState) {
        return false;
    }

    Model::Card* card = currentCard();
    // 状态无效：只有提问态允许切换到答案态。
    if (state != ReviewState::QuestionState) {
        qWarning() << "showAnswer failed: current state is not QuestionState.";
        return false;
    }

    // 数据无效：没有当前卡片时无法展示答案。
    if (!card) {
        qWarning() << "showAnswer failed: current card is null.";
        return false;
    }

    // 状态机职责：只宣布已经进入答案态，并把背面文本交给外部协调者处理。
    state = ReviewState::AnswerState;
    emit signal_showAnswer(card->back);
    return true;
}

bool ReviewController::submitFeedback(ReviewFeedback feedback) {
    Model::Card* card = currentCard();
    // 状态无效：只有答案态允许提交评分。
    if (state != ReviewState::AnswerState) {
        qWarning() << "submitFeedback failed: current state is not AnswerState.";
        return false;
    }

    // 数据无效：没有活动牌组时无法保存评分结果。
    if (!activeDeck) {
        qWarning() << "submitFeedback failed: active deck is null.";
        return false;
    }

    // 参数无效：没有文件路径时无法持久化复习进度。
    if (activeDeckFilePath.trimmed().isEmpty()) {
        qWarning() << "submitFeedback failed: deck file path is empty.";
        return false;
    }

    // 数据无效：没有当前卡片时无法提交评分。
    if (!card) {
        qWarning() << "submitFeedback failed: current card is null.";
        return false;
    }

    // SM2Engine mutates the card in place; keep a copy so save failures can roll back.
    const Model::Card backup = *card;
    Service::SM2Engine::calculate(card, static_cast<int>(feedback));

    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(*activeDeck, activeDeckFilePath, &error)) {
        *card = backup;
        qWarning() << "submitFeedback failed: unable to save deck file:" << error.message;
        return false;
    }

    // 连胜统计：仅“熟悉”累加，其余评分立即清零；落盘成功后才计入，保证与持久化一致。
    if (feedback == ReviewFeedback::Easy) {
        ++easyStreak;
    } else {
        easyStreak = 0;
    }
    emit signal_streakChanged(easyStreak);

    reviewQueue.pop();
    if (reviewQueue.empty()) {
        state = ReviewState::FinishedState;
        emit signal_reviewFinished();
        return true;
    }

    state = ReviewState::QuestionState;
    emit signal_showQuestion(reviewQueue.front()->front);
    return true;
}

// Session cleanup

void ReviewController::finishReview() {
    const bool hadActiveSession =
        state != ReviewState::FinishedState || activeDeck || !reviewQueue.empty();

    clearQueue();
    activeDeck = nullptr;
    activeDeckFilePath.clear();
    totalReviewCount = 0;
    state = ReviewState::FinishedState;

    if (hadActiveSession) {
        emit signal_reviewFinished();
    }
}

int ReviewController::remainingCount() const {
    return static_cast<int>(reviewQueue.size());
}

int ReviewController::totalCount() const {
    return totalReviewCount;
}

void ReviewController::clearQueue() {
    while (!reviewQueue.empty()) {
        reviewQueue.pop();
    }
}

} // namespace MindPalace::Controller
