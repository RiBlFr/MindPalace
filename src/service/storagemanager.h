#ifndef MINDPALACE_STORAGEMANAGER_H
#define MINDPALACE_STORAGEMANAGER_H

#include "model/Deck.h"
#include <QString>

//debug进度：测试了1)能够实现牌堆里面有多张牌的情况

namespace MindPalace::Service {
//定义的储存错误类型，用于在写入和读出崩溃时返回出错的原因

    enum class StorageErrorType {
        None,
        OpenFailed,
        ReadFailed,
        WriteFailed,
        ParseFailed,
        InvalidJsonRoot,
        ReplaceFailed
    };
//在出错的时候可以利用此类反馈出错的原因，位置，信息
    struct StorageError {
        StorageErrorType type = StorageErrorType::None;
        QString context;
        QString message;
        qint64 offset = -1;
    };


    class StorageManager {
    public:
        StorageManager() = delete;
        //此类不能实例化，只能用StorageManager::saveDeck(...)来实现存入和读取

        static bool saveDeck(const Model::Deck& deck, const QString& filePath, StorageError* error);
        //上面的函数是为了服务static bool saveDeck(const Model::Deck& deck, const QString& filePath);
        //请使用static bool saveDeck(const Model::Deck& deck, const QString& filePath);用于数据写入文件

        static bool saveDeck(const Model::Deck& deck, const QString& filePath);
        /**注意：使用此函数实现文件写入
         *
         * @param filePath 写入文件名，一个.json文件
         * @param deck 给定的牌堆
         * @return true:成功存入； false: 失败，并会打印原因
         */
        static bool loadDeck(const QString& filePath, Model::Deck& deck, StorageError* error);
        static bool loadDeck(const QString& filePath, Model::Deck& deck);
        /**注意：使用此函数实现文件读取
        *
        * @param filePath 读取文件名，一个.json文件名
        * @param deck 要赋值的牌堆
        * @return true:成功读取； false: 失败，并会打印原因
        */
    };

} // namespace MindPalace::Service

#endif // MINDPALACE_STORAGEMANAGER_H
