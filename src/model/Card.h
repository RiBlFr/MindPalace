//
// Created by Arian on 2026/5/10.
//

#ifndef MINDPALACE_CARD_H
#define MINDPALACE_CARD_H

#include <QString>
#include <QDate>
#include <QJsonObject>


namespace MindPalace::Model {

class Card {
public:
    // ==========================================
    // 1. 构造函数
    // ==========================================
    Card();
    Card(const QString& frontText, const QString& backText);

    // ==========================================
    // 2. 基础属性
    // ==========================================
    QString id;             // 唯一识别码
    QString front;          // 正面内容 (问题)
    QString back;           // 背面内容 (答案)

    // ==========================================
    // 3. SM-2 算法核心参数
    // ==========================================
    int repetitions;        // 已成功复习的连续次数 (n)
    float interval;         // 当前复习间隔，单位：天 (I)
    float easeFactor;       // 难度系数 (E-Factor)，默认初始值为 2.5

    // ==========================================
    // 4. 时间状态属性
    // ==========================================
    QDate lastReviewed;     // 上次复习的具体日期
    QDate nextReviewDate;   // 下次应该复习的具体日期

    // ==========================================
    // 5. 核心业务接口
    // ==========================================
    /**
     * @brief 判断当前卡片在今天是否已经到期，需要被复习
     * @return 如果今天大于或等于 nextReviewDate，返回 true
     */
    bool isDue() const;

    // ==========================================
    // 6. 序列化与反序列化接口 (StorageManager 使用)
    // ==========================================
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};

} // Model
// MindPalace

#endif //MINDPALACE_CARD_H
