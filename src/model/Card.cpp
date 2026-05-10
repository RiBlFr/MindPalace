//
// Created by Arian on 2026/5/10.
//

#include "Card.h"
#include <QUuid>

namespace MindPalace
{
    namespace Model
    {
        // 默认构造函数
        Card::Card() {
            // 自动生成全局唯一的 ID，去掉大括号和连字符，保持字符串干净
            id = QUuid::createUuid().toString(QUuid::WithoutBraces);

            // SM-2 算法的初始状态
            repetitions = 0;
            interval = 0.0f;
            easeFactor = 2.5f; // 2.5 是 SM-2 算法推荐的平滑起点

            // 新卡片默认今天就需要复习
            lastReviewed = QDate::currentDate();
            nextReviewDate = QDate::currentDate();
        }

        // 带参构造函数
        Card::Card(const QString& frontText, const QString& backText) : Card() {
            this->front = frontText;
            this->back = backText;
        }

        // 判断今天是否该复习
        bool Card::isDue() const {
            return QDate::currentDate() >= nextReviewDate;
        }

        // 序列化：将内存中的 C++ 对象转换为 JSON 格式，准备存入硬盘
        QJsonObject Card::toJson() const {
            QJsonObject json;
            json["id"] = id;
            json["front"] = front;
            json["back"] = back;
            json["repetitions"] = repetitions;
            // JSON 不支持单精度 float，自动转换为 double 存储
            json["interval"] = static_cast<double>(interval);
            json["easeFactor"] = static_cast<double>(easeFactor);

            // 日期对象不能直接存，必须按 ISO 8601 标准 (YYYY-MM-DD) 转成字符串
            json["lastReviewed"] = lastReviewed.toString(Qt::ISODate);
            json["nextDate"] = nextReviewDate.toString(Qt::ISODate);

            return json;
        }

        // 反序列化：从硬盘读取 JSON 数据，还原为 C++ 对象
        void Card::fromJson(const QJsonObject& json) {
            id = json["id"].toString();
            front = json["front"].toString();
            back = json["back"].toString();

            repetitions = json["repetitions"].toInt();
            interval = static_cast<float>(json["interval"].toDouble());
            easeFactor = static_cast<float>(json["easeFactor"].toDouble());

            // 将格式化的字符串重新解析回 QDate 对象
            lastReviewed = QDate::fromString(json["lastReviewed"].toString(), Qt::ISODate);
            nextReviewDate = QDate::fromString(json["nextDate"].toString(), Qt::ISODate);
        }
    } // Model
} // MindPalace