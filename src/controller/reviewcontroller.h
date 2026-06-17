#ifndef MINDPALACE_REVIEWCONTROLLER_H
#define MINDPALACE_REVIEWCONTROLLER_H

#include "model/Card.h"
#include "model/Deck.h"

#include <QString>
#include <queue>

#include <QObject>

namespace MindPalace::Controller {

// QObject is used so the review state machine can notify the UI layer.
class ReviewController : public QObject {
    Q_OBJECT

public:
    enum class ReviewState {
        QuestionState,//显示卡片未回答
        AnswerState,//已经回答
        FinishedState//结束询问
    };

    enum class ReviewFeedback {
        Again = 0,
        Hard = 3,
        Good = 4,
        Easy = 5
    };

    explicit ReviewController(QObject* parent = nullptr);

    /**
     * @brief 开始一次牌组复习，并筛选出今日到期的卡片。
     * @param deck 待复习的牌组。复习过程中会直接修改其中的卡片状态。
     * @param deckFilePath 该牌组对应的 JSON 文件路径，用于评分后立即保存。
     * @param forceReview 为 true 时忽略到期日期，复习该牌组内所有卡片。
     * @return 存在可复习卡片时返回 true；没有到期卡片或参数无效时返回 false。
     * 成功进入提问态时，会立即发出 signal_showQuestion()。
     * 注意：需要deckFilePath是因为想把deckcontroller 和 reviewcontroller分开
     * 可以从reviewcontroller.h得到filepath
     */
    bool startReview(Model::Deck* deck, const QString& deckFilePath, bool forceReview = false);

    /**
     * @brief 获取当前复习状态。
     */
    ReviewState currentState() const;

    /**
     * @brief 获取当前正在复习的卡片。
     * @return 当前卡片指针；复习结束或尚未开始时返回 nullptr。
     */
    Model::Card* currentCard() const;

    /**
     * @brief 从问题展示状态切换到答案展示状态。
     * @return 状态切换成功时返回 true，否则返回 false。
     */
    bool showAnswer();

    /**
     * @brief 提交当前卡片的复习反馈，更新 SM-2 参数并保存牌组。
     * @param feedback 用户选择的复习反馈。
     * @return 评分处理并保存成功时返回 true；保存失败或当前状态不可评分时返回 false。（此时会回滚card状态）
     * 保存成功后若仍有下一张卡片，会切回提问态并发出 signal_showQuestion()；
     * 若本次队列完成，会切入 FinishedState 并发出 signal_reviewFinished()。
     */
    bool submitFeedback(ReviewFeedback feedback);

    /**
     * @brief 主动结束当前复习流程，并清空复习队列。
     */
    void finishReview();

    /**
     * @brief 获取当前队列中剩余待复习卡片数量。
     */
    int remainingCount() const;

    /**
     * @brief 获取本次复习开始时的到期卡片总数。
     */
    int totalCount() const;

signals:
    // Signals describe state-machine events; the UI decides how to render them.

    /**
     * @brief 状态机切入“提问态”时发出。
     * 附带当前卡片的正面文字；外部协调者可据此驱动 View 渲染提问态。
     */
    void signal_showQuestion(const QString& frontText);

    /**
     * @brief 状态机切入“答案态”时发出。
     * 附带当前卡片的背面文字；外部协调者可据此驱动 View 渲染答案态。
     */
    void signal_showAnswer(const QString& backText);

    /**
     * @brief 复习队列清空或复习流程结束时发出。
     * 外部协调者可据此驱动 View 渲染复习完成状态。
     */
    void signal_reviewFinished();

    /**
     * @brief “连续选择简单”的连胜计数发生变化时发出。
     * @param easyStreak 当前在本牌组会话中连续评定为 [熟悉] 的张数；
     * 任意一次非简单评分都会立刻清零。新会话开始时也会归零（携带 0 广播）。
     * 外部协调者可据此驱动 View 显示/隐藏“火热连胜”徽章并分档施加特效。
     */
    void signal_streakChanged(int easyStreak);

private:
    Model::Deck* activeDeck = nullptr;
    QString activeDeckFilePath;
    std::queue<Model::Card*> reviewQueue;
    int totalReviewCount = 0;
    ReviewState state = ReviewState::FinishedState;

    // 本牌组会话内“连续选择简单”的累计张数；非简单评分立即清零，换牌组时归零。
    int easyStreak = 0;

    /**
     * @brief 构建复习队列
     * @param forceReview 是否忽略到期日期，复习该牌组内全部卡片
     */
    void buildReviewQueue(bool forceReview = false);
    void clearQueue();
};

} // namespace MindPalace::Controller

#endif // MINDPALACE_REVIEWCONTROLLER_H
