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
#include <QMap>


namespace MindPalace::Model {

    class Deck {
    public:
        Deck() = default;
        explicit Deck(const QString& name);

        QString deckId;
        QString deckName;

        std::vector<std::unique_ptr<Card>> cards;

        // Key: 日期字符串 (格式如 "2026-06-02")
        // Value: 操作指令 (1 = 强制复习, -1 = 强制休假, 其他值/不存在则遵循算法)
        QMap<QString, int> manualSchedule;

        /**
         * @brief 向牌组添加卡片
         * @param card 使用 std::move 转移所有权
         */
        void addCard(std::unique_ptr<Card> card);

        /**
         * @brief 获取当前牌组中今日待复习的卡片总数
         * 用于主界面的环形进度条和数据摘要展示
         */
        int getDueCount() const;

        /**
         * @brief 获取当前牌组中今日已经复习过的卡片数量
         * 依据每张卡片持久化的 lastReviewed 字段统计（== 今天即视为今日已复习）。
         * 由于评分后卡片的 nextReviewDate 一定被推迟到明天及以后，今日已复习的卡片
         * 不会再被计入今日待复习队列，因此该统计与“待复习”天然互斥、不会重复计数。
         * 用于在关闭应用后仍能正确还原“今日复习”进度。
         */
        int getReviewedTodayCount() const;

        QJsonObject toJson() const;
        void fromJson(const QJsonObject& json);
    };

}

#endif //MINDPALACE_DECK_H
