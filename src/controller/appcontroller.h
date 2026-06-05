//
// Created by Arian on 2026/5/25.
//

#ifndef MINDPALACE_APPCONTROLLER_H
#define MINDPALACE_APPCONTROLLER_H

#include <QObject>
#include <memory>
#include <QString>

class MainWindow;
namespace MindPalace::Controller {
    class DeckController;
    class ReviewController;
}

namespace MindPalace::Controller {

    /**
     * @class AppController
     * @brief 全局生命周期与事件路由中枢
     * 负责实例化并管理 MVC 架构中的核心控制器与主视图，实现顶层模块间的物理隔离与信号缝合。
     */
    class AppController : public QObject {
        Q_OBJECT

    public:
        explicit AppController(QObject *parent = nullptr);
        ~AppController() override;

        // 禁用拷贝，保证单例中枢
        AppController(const AppController&) = delete;
        AppController& operator=(const AppController&) = delete;

        /**
         * @brief 点火函数：启动应用程序，展示主窗口
         */
        void start();

        // 日历看板专用查询接口
        // ==========================================
        /**
         * @brief 获取指定月份的总体复习计划分布
         * @return 映射表：具体的某一天 -> 那天需要复习的牌组名称列表
         */
        QMap<QDate, QStringList> getCalendarData(int year, int month);

    public slots: // 注意：可以放在 public slots 下，方便未来与 UI 信号绑定
        // ==========================================
        // 【新增】日历看板专用修改接口
        // ==========================================
        /**
         * @brief 修改指定牌组在某天的复习状态（软覆盖算法）
         * @param deckName 牌组名称
         * @param date 目标日期
         * @param status 1代表强制复习，-1代表休假，0代表清除手动计划（恢复算法）
         */
        void handleUpdateDeckSchedule(const QString& deckName, const QDate& date, int status);

    private:
        // ==========================================
        // 1. 核心组件托管区 (RAII).
        // ==========================================
        std::unique_ptr<DeckController> m_deckController;
        std::unique_ptr<ReviewController> m_reviewController;
        std::unique_ptr<MainWindow> m_mainWindow;
        QString m_decksDirPath;

        // 用于在复习期间记忆当前牌组的名称
        QString m_currentReviewingDeckName;

        // 本次会话开始前，当前牌组今日已复习的卡片数（来自持久化的 lastReviewed）。
        // 作为“今日复习”进度的基线，保证关闭重开应用后已完成的进度不会被清零。
        int m_todayReviewedBaseline = 0;

        // ==========================================
        // 2. 初始化流水线
        // ==========================================
        void initializeControllers();
        void initializeViews();

        /**
         * @brief 全局神经枢纽：负责将 View 和各个子 Controller 的信号/槽缝合在一起
         */
        void setupGlobalConnections();

    private slots:
        void handleStartReview(const QString& deckName);
        void handleSubmitFeedback(int quality);
        void handleResetDeck(const QString& deckName);
        void handleAppQuit();

    private:
        void refreshDeckList();
    };


} // namespace MindPalace::Controller

#endif // MINDPALACE_APPCONTROLLER_H
