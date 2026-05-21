#ifndef MINDPALACE_DECKCONTROLLER_H
#define MINDPALACE_DECKCONTROLLER_H

#include "model/Deck.h"

#include <QString>
#include <vector>

namespace MindPalace::Controller {

class DeckController {
public:
    /**
     * @brief 构造牌组控制器，并自动从磁盘加载已有牌组。
     * @param decksDirPath 牌组 JSON 文件所在目录，默认为 data/decks。
     */
    explicit DeckController(const QString& decksDirPath = "data/decks");

    /**
     * @brief 创建新牌组，并以 <deckId>.json 的形式持久化到磁盘。
     * @param name 用户可见的牌组名称，不能为空，且不能与已有牌组重名。
     * @return 创建内存对象并写入磁盘成功时返回 true，否则返回 false。
     */
    bool createDeck(const QString& name);

    /**
     * @brief 根据牌组名称删除牌组，并移除对应的 JSON 文件。
     * @param deckName 已存在的用户可见牌组名称。
     * @return 内存状态和磁盘状态均更新成功时返回 true，否则返回 false。
     */
    bool deleteDeck(const QString& deckName);

    /**
     * @brief 重命名已有牌组，保留原 deckId 和 JSON 文件路径。
     * @param deckName 当前用户可见牌组名称。
     * @param newName 新的用户可见牌组名称，不能为空，且不能与已有牌组重名。
     * @return 重命名结果成功写入磁盘时返回 true，否则返回 false。
     */
    bool renameDeck(const QString& deckName, const QString& newName);

    /**
     * @brief 获取当前已加载的全部牌组，供界面渲染使用。
     */
    const std::vector<Model::Deck>& getDecks() const;

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
