#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMainWindow>
#include <QString>

class QWidget;
class QListWidget;
class QLabel;
class QPushButton;
class QProgressBar;
class QFrame;
class QQuickWidget;
/**
 * @class MainWindow
 * @brief 记忆殿堂主窗口
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // ==========================================
    // 【新增】对外暴露的被动渲染接口 (供 AppController 调用)
    // ==========================================

    /**
     * @brief 刷新左侧的牌组列表
     * @param deckNames 所有可用牌组的名字清单
     */
    void updateDeckListView(const std::vector<QString>& deckNames) const;

    /**
     * @brief 渲染卡片正面（提问态）
     */
    void renderQuestionLayout(const QString& frontText) const;

    /**
     * @brief 渲染卡片正反面与分隔线（答案态）
     */
    void renderAnswerLayout(const QString& backText);

    /**
     * @brief 切换至今日复习完成的结算页面
     */
    void showFinishedSummaryPage();

private:
    // UI 初始化
    void initUI();
    void setupMenuBar();
    void setupLeftPanel();
    void setupCenterPanel();
    void setupRightPanel();
    void setupStyles();

    void initFlashCardView();
    class QHBoxLayout* setupFeedbackButtons();

signals:
    // 用户点击了某张牌组的“进入学习”
    void signal_requestStartReview(const QString& deckName);

    // 用户在复习界面点击了 1~4 评分按钮 (对应生疏到简单)
    void signal_requestSubmitFeedback(int quality);

    // 用户点击了窗口红叉准备退出
    void signal_appWillClose();

protected:
    // 重写关闭事件，用于拦截右上角的红叉
    void closeEvent(QCloseEvent *event) override;

private slots:
    void showNextFlashCard();

private:
    // ===== 左侧导航面板 =====
    QWidget *leftPanel{};
    QListWidget *deckListWidget{};
    QPushButton *addDeckBtn{};

    // ===== 中央看板区 =====
    QWidget *centerPanel{};
    QQuickWidget *flashCardView{};
    QPushButton *feedbackBtns[4]{};  // 生疏、困难、良好、简单
    int currentFlashCardIndex{0};
    int flashCardCount{0};

    // ===== 右侧统计面板 =====
    QWidget *rightPanel{};
    QLabel *progressLabel{};
    QLabel *todayStudyLabel{};
    QLabel *reviewStatsLabel{};
    QProgressBar *dailyProgressBar{};
};

#endif // MAINWINDOW_H
