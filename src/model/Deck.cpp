//
// Created by Arian on 2026/5/10.
//

#include "Deck.h"
#include <algorithm>
#include <QDate>

namespace MindPalace::Model {

    Deck::Deck(const QString& name) : deckName(name) {}

    void Deck::addCard(std::unique_ptr<Card> card) {
        if (card) {
            cards.push_back(std::move(card));
        }
    }

    int Deck::getDueCount() const {
        return std::count_if(cards.begin(), cards.end(), [](const std::unique_ptr<Card>& c) {
            return c && c->isDue();
        });
    }

    int Deck::getReviewedTodayCount() const {
        const QDate today = QDate::currentDate();
        return std::count_if(cards.begin(), cards.end(), [&today](const std::unique_ptr<Card>& c) {
            return c && c->lastReviewed == today;
        });
    }

    QJsonObject Deck::toJson() const {
        QJsonObject json;
        json["deckName"] = deckName;
        json["deckId"] = deckId;

        QJsonArray cardsArray;
        for (const auto& cardPtr : cards) {
            if (cardPtr) {
                cardsArray.append(cardPtr->toJson());
            }
        }
        json["cards"] = cardsArray;

        QJsonObject scheduleObj;
        for (auto it = manualSchedule.constBegin(); it != manualSchedule.constEnd(); ++it) {
            scheduleObj[it.key()] = it.value();
        }
        json["manualSchedule"] = scheduleObj;

        return json;
    }

    void Deck::fromJson(const QJsonObject& json) {
        deckName = json["deckName"].toString();
        deckId = json["deckId"].toString();

        cards.clear();

        QJsonArray cardsArray = json["cards"].toArray();
        for (int i = 0; i < cardsArray.size(); ++i) {
            QJsonObject cardObj = cardsArray[i].toObject();

            auto card = std::make_unique<Card>();
            card->fromJson(cardObj);
            cards.push_back(std::move(card));
        }

        manualSchedule.clear();
        if (json.contains("manualSchedule")) {
            QJsonObject scheduleObj = json["manualSchedule"].toObject();
            for (const QString& key : scheduleObj.keys()) {
                manualSchedule[key] = scheduleObj.value(key).toInt();
            }
        }
    }

}
