#include "reviewcontroller.h"
#include "service/sm2engine.h"
#include "service/storagemanager.h"

namespace MindPalace::Controller {

ReviewController::ReviewController(QObject* parent)
    : QObject(parent) {}

bool ReviewController::startReview(Model::Deck* deck, const QString& deckFilePath) {
    if (!deck) {
        return false;
    }

    clearQueue();
    activeDeck = deck;
    activeDeckFilePath = deckFilePath;

    buildReviewQueue();
    totalReviewCount = static_cast<int>(reviewQueue.size());

    if (reviewQueue.empty()) {
        state = ReviewState::FinishedState;
        return false;
    }

    state = ReviewState::QuestionState;
    emit signal_showQuestion(reviewQueue.front()->front);
    return true;
}

ReviewController::ReviewState ReviewController::currentState() const {
    return state;
}

Model::Card* ReviewController::currentCard() const {
    if (state == ReviewState::FinishedState || reviewQueue.empty()) {
        return nullptr;
    }
    return reviewQueue.front();
}

bool ReviewController::showAnswer() {
    if (state != ReviewState::QuestionState) {
        return false;
    }

    state = ReviewState::AnswerState;
    emit signal_showAnswer(reviewQueue.front()->back);
    return true;
}

bool ReviewController::submitFeedback(ReviewFeedback feedback) {
    if (state != ReviewState::AnswerState || reviewQueue.empty()) {
        return false;
    }

    Model::Card* card = reviewQueue.front();

    // Snapshot card state for rollback on save failure
    const QDate oldLastReviewed   = card->lastReviewed;
    const QDate oldNextReviewDate = card->nextReviewDate;
    const int   oldRepetitions    = card->repetitions;
    const float oldInterval       = card->interval;
    const float oldEaseFactor     = card->easeFactor;

    Service::SM2Engine::calculate(card, static_cast<int>(feedback));

    if (!Service::StorageManager::saveDeck(*activeDeck, activeDeckFilePath)) {
        card->lastReviewed   = oldLastReviewed;
        card->nextReviewDate = oldNextReviewDate;
        card->repetitions    = oldRepetitions;
        card->interval       = oldInterval;
        card->easeFactor     = oldEaseFactor;
        return false;
    }

    reviewQueue.pop();

    if (!reviewQueue.empty()) {
        state = ReviewState::QuestionState;
        emit signal_showQuestion(reviewQueue.front()->front);
    } else {
        finishReview();
    }

    return true;
}

void ReviewController::finishReview() {
    clearQueue();
    activeDeck = nullptr;
    activeDeckFilePath.clear();
    state = ReviewState::FinishedState;
    emit signal_reviewFinished();
}

int ReviewController::remainingCount() const {
    return static_cast<int>(reviewQueue.size());
}

int ReviewController::totalCount() const {
    return totalReviewCount;
}

void ReviewController::buildReviewQueue() {
    if (!activeDeck) {
        return;
    }

    for (const auto& cardPtr : activeDeck->cards) {
        if (cardPtr && cardPtr->isDue()) {
            reviewQueue.push(cardPtr.get());
        }
    }
}

void ReviewController::clearQueue() {
    while (!reviewQueue.empty()) {
        reviewQueue.pop();
    }
}

} // namespace MindPalace::Controller
