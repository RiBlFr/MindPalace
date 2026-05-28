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
     * @param hasNextCard 是否还有更多待复习卡片（用于控制背景叠层阴影卡的显示）
     */
    void renderQuestionLayout(const QString& frontText, bool hasNextCard = false) const;

    /**
     * @brief 渲染卡片正反面与分隔线（答案态）
     */
    void renderAnswerLayout(const QString& backText);

    /**
     * @brief 切换至今日复习完成的结算页面
     */
    void showFinishedSummaryPage();

    /**
     * @brief 更新右侧进度面板
     * @param done  本次会话已复习张数
     * @param total 本次会话总到期张数
     */
    void updateProgressView(int done, int total);

    /**
     * @brief 预加载当前卡片的背面文字，使翻牌时无需等待信号链完成即可显示答案
     */
    void preloadAnswerText(const QString& backText);

    /**
     * @brief QML 直接调用的翻牌回调（通过 _reviewBridge context property）
     * 必须是 Q_INVOKABLE 才能被 QML JS 调用
     */
    Q_INVOKABLE void notifyCardFlipped(bool flipped);

private:
    // UI 初始化
    void initUI();
    void setupMenuBar();
    void setupLeftPanel();
    void setupCenterPanel();
    void setupRightPanel();
    void setupStyles();

    void initFlashCardView();
    QWidget* setupFeedbackButtons();

signals:
    void signal_requestStartReview(const QString& deckName);
    void signal_requestSubmitFeedback(int quality);
    void signal_requestShowAnswer();
    void signal_requestResetDeck(const QString& deckName);
    void signal_appWillClose();

private slots:
    // Monitors QML's "flipped" property change to emit signal_requestShowAnswer.
    // Property-change signals (flippedChanged) are guaranteed in Qt's meta-object
    // system, unlike user-defined QML signals which can fail with the SIGNAL macro.
    void onQmlFlippedPropertyChanged();

protected:
    // 重写关闭事件，用于拦截右上角的红叉
    void closeEvent(QCloseEvent *event) override;

private:
    // ===== 左侧导航面板 =====
    QWidget *leftPanel{};
    QListWidget *deckListWidget{};
    QPushButton *addDeckBtn{};
    QPushButton *resetDeckBtn{};

    // ===== 中央看板区 =====
    QWidget *centerPanel{};
    QQuickWidget *flashCardView{};
    QPushButton *showAnswerBtn{};    // 问题态：大的"显示答案"按钮
    QWidget    *feedbackRow{};       // 答案态：4 个评分按钮的容器
    QPushButton *feedbackBtns[4]{};  // 生疏、困难、良好、简单（在 feedbackRow 内）

    // ===== 右侧统计面板 =====
    QWidget *rightPanel{};
    QLabel *progressLabel{};
    QLabel *todayStudyLabel{};
    QLabel *reviewStatsLabel{};
    QProgressBar *dailyProgressBar{};
};

#endif // MAINWINDOW_H
