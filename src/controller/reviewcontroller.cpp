#include "reviewcontroller.h"

namespace MindPalace::Controller {

// =========================================================================
// 1. 构造与只读状态查询
// =========================================================================

ReviewController::ReviewController(QObject* parent)
    : QObject(parent) {
}

ReviewController::ReviewState ReviewController::currentState() const {
    return state;
}

Model::Card* ReviewController::currentCard() const {
    // 架构安全规范：FinishedState 代表复习状态机已经停止对外暴露卡片，
    // 即使内部队列残留数据，也不能让 View 层继续读取旧卡片。
    if (state == ReviewState::FinishedState || reviewQueue.empty()) {
        return nullptr;
    }

    return reviewQueue.front();
}

// =========================================================================
// 2. 复习会话启动与队列构建
// =========================================================================

bool ReviewController::startReview(Model::Deck* deck, const QString& deckFilePath) {
    if (!deck || deckFilePath.trimmed().isEmpty()) {
        return false;
    }

    clearQueue();
    totalReviewCount = 0;
    state = ReviewState::FinishedState;

    activeDeck = deck;
    activeDeckFilePath = deckFilePath;
    buildReviewQueue();
    totalReviewCount = static_cast<int>(reviewQueue.size());

    if (reviewQueue.empty()) {
        return false;
    }

    state = ReviewState::QuestionState;
    return true;
}

void ReviewController::buildReviewQueue() {
    clearQueue();

    if (!activeDeck) {
        return;
    }

    for (const auto& card : activeDeck->cards) {
        if (card && card->isDue()) {
            reviewQueue.push(card.get());
        }
    }
}

void ReviewController::clearQueue() {
    while (!reviewQueue.empty()) {
        reviewQueue.pop();
    }
}

} // namespace MindPalace::Controller
