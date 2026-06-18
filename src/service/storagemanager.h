#ifndef MINDPALACE_STORAGEMANAGER_H
#define MINDPALACE_STORAGEMANAGER_H

#include "model/Deck.h"
#include <QDate>
#include <QSet>
#include <QString>

namespace MindPalace::Service {
// Error categories used by deck load/save operations.

    enum class StorageErrorType {
        None,
        OpenFailed,
        ReadFailed,
        WriteFailed,
        ParseFailed,
        InvalidJsonRoot,
        ReplaceFailed
    };
// Optional detail returned to callers when storage work fails.
    struct StorageError {
        StorageErrorType type = StorageErrorType::None;
        QString context;
        QString message;
        qint64 offset = -1;
    };


    class StorageManager {
    public:
        StorageManager() = delete;

        static bool saveDeck(const Model::Deck& deck, const QString& filePath, StorageError* error);
        static bool saveDeck(const Model::Deck& deck, const QString& filePath);
        /**
         * @brief 保存牌组到 JSON 文件。
         *
         * @param filePath 写入文件名，一个.json文件
         * @param deck 给定的牌堆
         * @return true:成功存入； false: 失败，并会打印原因
         */
        static bool loadDeck(const QString& filePath, Model::Deck& deck, StorageError* error);
        static bool loadDeck(const QString& filePath, Model::Deck& deck);
        /**
        * @brief 从 JSON 文件读取牌组。
        *
        * @param filePath 读取文件名，一个.json文件名
        * @param deck 要赋值的牌堆
        * @return true:成功读取； false: 失败，并会打印原因
        */

        /**
         * @brief 给今天的复习数量 +1，并自动落盘
         */
        static void incrementDailyReviewCount();

        /**
         * @brief 获取过去 7 天的复习数据，用于 UI 图表渲染
         * @param outData 输出的 7 天数据数组 (例如 [12, 0, 5, ...])
         * @param outLabels 输出的 7 天横坐标标签 (例如 ["周四", "周五", ... "今"])
         */
        static void getWeeklyReviewData(std::vector<int>& outData, QStringList& outLabels);

        /**
         * @brief Mark today as signed in. Returns false when today was already signed in.
         */
        static bool markTodaySignedIn();

        /**
         * @brief Whether today already has a sign-in record.
         */
        static bool isTodaySignedIn();

        /**
         * @brief Read all signed-in days for a given month.
         */
        static QSet<QDate> getMonthlyCheckInDates(int year, int month);
    };

} // namespace MindPalace::Service
/*附：实现的json表示：

{
    "cards": [
        {
            "back": "back text 1",
            "easeFactor": 2.5,
            "front": "front text 1",
            "id": "ae3f03b8-1365-443d-b157-d987b1481bb8",
            "interval": 0,
            "lastReviewed": "2026-05-21",
            "nextDate": "2026-05-21",
            "repetitions": 0
        },
        {
            "back": "back text 2",
            "easeFactor": 2.5,
            "front": "front text 2",
            "id": "092369df-68b4-4c71-b6fe-aa89b922c896",
            "interval": 0,
            "lastReviewed": "2026-05-21",
            "nextDate": "2026-05-21",
            "repetitions": 0
        },
        {
            "back": "back text 3",
            "easeFactor": 2.5,
            "front": "front text 3",
            "id": "d5c1b358-0a24-4214-9ce5-93c893228bfd",
            "interval": 0,
            "lastReviewed": "2026-05-21",
            "nextDate": "2026-05-21",
            "repetitions": 0
        }
    ],
    "deckName": "DebugDeck"
}
 *
 */

#endif // MINDPALACE_STORAGEMANAGER_H
