#include "reviewcontroller.h"
#include "service/sm2engine.h"
#include "service/storagemanager.h"

#include <QDebug>

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

    activeDeck = deck;
    activeDeckFilePath = deckFilePath;

    // 【修改点】向下传递强制复习指令
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

    for (const auto& card : activeDeck->cards) {
        if (card && (forceReview || card->isDue())) {
            reviewQueue.push(card.get());
        }
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

    // 核心一致性保护：SM2Engine 会直接改写卡片，必须先留快照，确保落盘失败时可以完整回滚。
    const Model::Card backup = *card;
    Service::SM2Engine::calculate(card, static_cast<int>(feedback));

    Service::StorageError error;
    if (!Service::StorageManager::saveDeck(*activeDeck, activeDeckFilePath, &error)) {
        *card = backup;
        qWarning() << "submitFeedback failed: unable to save deck file:" << error.message;
        return false;
    }

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

// =========================================================================
// 3. 复习会话收尾与进度查询
// =========================================================================

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
