//
// Created by Arian on 2026/5/10.
//

#ifndef MINDPALACE_DECK_H
#define MINDPALACE_DECK_H

#include "Card.h"
#include <vector>
#include <memory>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>


namespace MindPalace::Model {

    class Deck {
    public:
        // ==========================================
        // 1. 构造函数
        // ==========================================
        Deck() = default;
        explicit Deck(const QString& name);

        // ==========================================
        // 2. 基础属性
        // ==========================================
        QString deckName;

        std::vector<std::unique_ptr<Card>> cards;

        // ==========================================
        // 3. 核心管理接口
        // ==========================================
        /**
         * @brief 向牌组添加卡片
         * @param card 必须使用 std::move 转移所有权
         */
        void addCard(std::unique_ptr<Card> card);

        /**
         * @brief 获取当前牌组中今日待复习的卡片总数
         * 用于主界面的环形进度条和数据摘要展示
         */
        int getDueCount() const;

        // ==========================================
        // 4. 持久化接口
        // ==========================================
        QJsonObject toJson() const; // 将整个牌组（含所有卡片）转为 JSON
        void fromJson(const QJsonObject& json);
    };

}

#endif //MINDPALACE_DECK_H
