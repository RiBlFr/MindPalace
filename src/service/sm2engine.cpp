//
// Created by Arian on 2026/5/20.
//

#include "sm2engine.h"
#include <algorithm>
#include <QDate>

namespace MindPalace::Service {

void SM2Engine::calculate(Model::Card* card, int quality) {
    if (!card) return;

    // Any scored review updates the last-reviewed date.
    card->lastReviewed = QDate::currentDate();

    if (quality < 3) {
        // Failed recall restarts the repetition streak and reviews again tomorrow.
        card->repetitions = 0;
        card->interval = 1.0f;
        card->nextReviewDate = card->lastReviewed.addDays(1);

        return;
    }

    card->repetitions += 1;

    // SM-2 ease-factor update.
    auto q = static_cast<float>(quality);
    const float oldEF = card->easeFactor;
    const float newEF = oldEF + (0.1f - (5.0f - q) * (0.08f + (5.0f - q) * 0.02f));

    // Keep difficult cards from collapsing to unusably small intervals.
    card->easeFactor = std::max(1.3f, newEF);

    if (card->repetitions == 1) {
        card->interval = 1.0f;
    } else if (card->repetitions == 2) {
        card->interval = 6.0f;
    } else {
        card->interval = card->interval * card->easeFactor;
    }

    // addDays takes an int; truncation keeps scheduling on whole days.
    const int daysToAdd = static_cast<int>(card->interval);
    card->nextReviewDate = card->lastReviewed.addDays(daysToAdd);
}

} // namespace MindPalace::Service
