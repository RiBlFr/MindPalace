#ifndef MINDPALACE_REVIEWCONTROLLER_H
#define MINDPALACE_REVIEWCONTROLLER_H

#include "model/Card.h"
#include "model/Deck.h"

#include <QString>
#include <queue>

#include <QObject>

namespace MindPalace::Controller {

// 【改造】继承自 QObject，赋予该类发送和接收信号的超能力
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

    // 【改造】符合 Qt 标准规范的构造函数，带有可选的 parent 指针用于自动内存管理
    explicit ReviewController(QObject* parent = nullptr);

    /**
     * @brief 开始一次牌组复习，并筛选出今日到期的卡片。
     * @param deck 待复习的牌组。复习过程中会直接修改其中的卡片状态。
     * @param deckFilePath 该牌组对应的 JSON 文件路径，用于评分后立即保存。
     * @return 存在可复习卡片时返回 true；没有到期卡片或参数无效时返回 false。
     * 注意：需要deckFilePath是因为想把deckcontroller 和 reviewcontroller分开
     * 可以从reviewcontroller.h得到filepath（待商议）
     */
    bool startReview(Model::Deck* deck, const QString& deckFilePath);

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
    // ==========================================
    // 【新增区】状态机向外界（总指挥）广播的核心事件
    // ==========================================

    /**
     * @brief 当状态机切入“提问态”时触发
     * 附带当前卡片的正面文字，通知外部进行 UI 渲染
     */
    void signal_showQuestion(const QString& frontText);

    /**
     * @brief 当状态机切入“答案态”时触发
     * 附带当前卡片的背面文字，通知外部揭晓答案并显示评分按钮
     */
    void signal_showAnswer(const QString& backText);

    /**
     * @brief 当复习队列被彻底清空时触发
     * 通知外部今日学习任务圆满结束，可切换至结算大图章页面
     */
    void signal_reviewFinished();

private:
    Model::Deck* activeDeck = nullptr;
    QString activeDeckFilePath;
    std::queue<Model::Card*> reviewQueue;
    int totalReviewCount = 0;
    ReviewState state = ReviewState::FinishedState;

    void buildReviewQueue();
    void clearQueue();
};

} // namespace MindPalace::Controller

#endif // MINDPALACE_REVIEWCONTROLLER_H
