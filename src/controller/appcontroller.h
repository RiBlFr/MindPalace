//
// Created by Arian on 2026/5/25.
//

#ifndef MINDPALACE_APPCONTROLLER_H
#define MINDPALACE_APPCONTROLLER_H

#include <QObject>
#include <memory>
#include <QDate>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

class MainWindow;
namespace MindPalace::Controller {
    class DeckController;
    class ReviewController;
}

namespace MindPalace::Controller {

    /**
     * @class AppController
     * @brief 管理应用生命周期与主要事件路由。
     * 负责创建控制器和主窗口，并集中连接跨模块信号。
     */
    class AppController : public QObject {
        Q_OBJECT

    public:
        explicit AppController(QObject *parent = nullptr);
        ~AppController() override;

        // AppController owns the application-level objects; copying it would
        // duplicate that ownership.
        AppController(const AppController&) = delete;
        AppController& operator=(const AppController&) = delete;

        /**
         * @brief 启动应用程序并展示主窗口。
         */
        void start();
        bool showStartupReviewReminder();

        // 日历看板专用查询接口
        /**
         * @brief 获取指定月份的总体复习计划分布
         * @return 映射表：具体的某一天 -> 那天需要复习的牌组名称列表
         */
        QMap<QDate, QStringList> getCalendarData(int year, int month);

    public slots:
        /**
         * @brief 修改指定牌组在某天的复习状态（软覆盖算法）
         * @param deckName 牌组名称
         * @param date 目标日期
         * @param status 1代表强制复习，-1代表休假，0代表清除手动计划（恢复算法）
         */
        void handleUpdateDeckSchedule(const QString& deckName, const QDate& date, int status);

    private:
        // Owned application components.
        std::unique_ptr<DeckController> m_deckController;
        std::unique_ptr<ReviewController> m_reviewController;
        std::unique_ptr<MainWindow> m_mainWindow;
        QString m_decksDirPath;

        // 用于在复习期间记忆当前牌组的名称
        QString m_currentReviewingDeckName;

        // 本次会话开始前，当前牌组今日已复习的卡片数（来自持久化的 lastReviewed）。
        // 作为“今日复习”进度的基线，保证关闭重开应用后已完成的进度不会被清零。
        int m_todayReviewedBaseline = 0;

        void initializeControllers();
        void initializeViews();

        /**
         * @brief Connects view signals to controller actions.
         */
        void setupGlobalConnections();

    private slots:
        void handleStartReview(const QString& deckName);
        void handleSubmitFeedback(int quality);
        void handleResetDeck(const QString& deckName);
        void handleAppQuit();

    private:
        void refreshDeckList();
        bool maybeShowReviewReminder(QWidget* parentForDialog = nullptr);
    };


} // namespace MindPalace::Controller

#endif // MINDPALACE_APPCONTROLLER_H
