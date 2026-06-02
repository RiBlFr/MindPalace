#ifndef MINDPALACE_DECKCONTROLLER_H
#define MINDPALACE_DECKCONTROLLER_H

#include "model/Deck.h"

#include <QString>
#include <vector>
#include <QObject>

namespace MindPalace::Controller {

class DeckController : public QObject {
    Q_OBJECT         // 【新增】Qt 元对象宏，告诉编译器这个类有信号和槽

public:
    /**
     * @brief 构造牌组控制器，并自动从磁盘加载已有牌组。
     * @param decksDirPath 牌组 JSON 文件所在目录，默认为 data/decks。
     * @param parent 【新增】Qt 的父对象指针，用于挂载到对象树上，防止内存泄漏
     */
    explicit DeckController(const QString& decksDirPath = "data/decks"
        , QObject* parent = nullptr);

    /**
     * @brief 创建新牌组，并以 <deckId>.json 的形式持久化到磁盘。
     * 创建时去除名称首尾空白，拒绝空名称和重名，并生成独立 deckId 作为文件名。
     * @param name 用户可见的牌组名称，处理后不能为空，且不能与已有牌组重名。
     * @return 创建内存对象并写入磁盘成功时返回 true，否则返回 false。
     */
    bool createDeck(const QString& name);

    /**
     * @brief 根据牌组名称删除牌组，并移除对应的 JSON 文件。
     * 删除时去除输入名称首尾空白，并在删除磁盘文件成功后再移除内存对象。
     * 已经更改为：若对应磁盘文件已不存在，也会移除内存中的牌组残留并视为删除成功。
     * @param deckName 已存在的用户可见牌组名称。
     * @return 内存状态和磁盘状态均更新成功时返回 true，否则返回 false。
     */
    bool deleteDeck(const QString& deckName);

    /**
     * @brief 重命名已有牌组，保留原 deckId 和 JSON 文件路径。
     * 重命名时去除两个输入名称的首尾空白；新名称与原名称相同时直接视为成功。
     * 写入磁盘失败时恢复原名称，确保内存状态不提前改变。
     * @param deckName 当前用户可见牌组名称。
     * @param newName 新的用户可见牌组名称，处理后不能为空，且不能与已有牌组重名。
     * @return 重命名结果成功写入磁盘时返回 true，否则返回 false。
     */
    bool renameDeck(const QString& deckName, const QString& newName);

    /**
     * @brief 获取当前已加载的全部牌组，供界面渲染使用。
     */
    const std::vector<Model::Deck>& getDecks() const;

    /**
     * @brief 将指定牌组的所有卡片 SM-2 参数重置为初始状态（全部今日到期）。
     * @param deckName 要重置的牌组名称。
     * @return 重置并写盘成功时返回 true，否则返回 false。
     */
    bool resetDeck(const QString& deckName);

    /**
     * @brief 向指定牌组添加一张新卡片并立刻落盘
     */
    bool addCardToDeck(const QString& deckName, const QString& front, const QString& back);

    /**
     * @brief 从指定牌组删除一张卡片并立刻落盘。
     * @param deckName 目标牌组名称。
     * @param cardId 要删除的卡片唯一 ID。
     * @return 删除并写盘成功时返回 true，否则返回 false。
     */
    bool deleteCardFromDeck(const QString& deckName, const QString& cardId);

    /**
     * @brief 从外部文件导入一个完整牌组。
     * 具体导入格式、字段兼容策略、重名处理策略待后续确认。
     * @param sourceFilePath 外部牌组文件路径。
     * @return 导入并写入本地牌组目录成功时返回 true，否则返回 false。
     */
    bool importDeckFromFile(const QString& sourceFilePath);

signals:
    // ==========================================
    // 【新增区】对外广播的信号器官
    // ==========================================

    /**
     * @brief 当底层牌组数据发生增、删、改时，触发此信号。
     * AppController 监听到后，会立即通知 MainWindow 重新拉取数据并刷新 UI。
     */
    void signal_deckListChanged();

private:
    QString decksDirPath;
    std::vector<Model::Deck> decks;

    void loadDecks();
    Model::Deck* findDeckByName(const QString& deckName);
    const Model::Deck* findDeckByName(const QString& deckName) const;
    bool deckNameExists(const QString& deckName) const;
    QString deckFilePath(const QString& deckId) const;
};

} // namespace MindPalace::Controller

#endif // MINDPALACE_DECKCONTROLLER_H
