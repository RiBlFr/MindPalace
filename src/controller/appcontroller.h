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
