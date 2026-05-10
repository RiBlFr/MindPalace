//
// Created by Arian on 2026/5/10.
//

#include "Deck.h"
#include <algorithm>

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

    QJsonObject Deck::toJson() const {
        QJsonObject json;
        json["deckName"] = deckName;

        // 创建一个 JSON 数组来存放所有卡片
        QJsonArray cardsArray;
        for (const auto& cardPtr : cards) {
            if (cardPtr) {
                cardsArray.append(cardPtr->toJson());
            }
        }
        json["cards"] = cardsArray;

        return json;
    }

    void Deck::fromJson(const QJsonObject& json) {
        deckName = json["deckName"].toString();

        cards.clear();

        QJsonArray cardsArray = json["cards"].toArray();
        for (int i = 0; i < cardsArray.size(); ++i) {
            QJsonObject cardObj = cardsArray[i].toObject();

            auto card = std::make_unique<Card>();
            card->fromJson(cardObj);
            cards.push_back(std::move(card));
        }
    }

}