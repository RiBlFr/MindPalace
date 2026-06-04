#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <vector>
#include <QDate>
#include <QMap>
#include <QStringList>

#include "CardManagerDialog.h"
#include "WeeklyChartWidget.h"

class QWidget;
class QListWidget;
class QLabel;
class QPushButton;
class QProgressBar;
class QFrame;
class QQuickWidget;
class QStackedWidget;
/**
 * @class MainWindow
 * @brief 记忆殿堂主窗口
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

    // themeMode 暴露当前主题在注册表（Theme::availableThemes()）中的索引；
    // frostedCard 让 QML 无需关心具体主题，只问“当前是否磨砂玻璃卡片”。
    Q_PROPERTY(int themeMode READ themeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool frostedCard READ frostedCard NOTIFY themeModeChanged)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 当前主题在 Theme::availableThemes() 中的索引
    int themeMode() const { return m_themeIndex; }

    // 当前主题是否使用磨砂玻璃卡片效果（供 QML 绑定）
    bool frostedCard() const;

    // 应用主题：themeIndex 为 Theme::availableThemes() 中的索引
    void applyTheme(int themeIndex);

    // ==========================================
    // 对外暴露的被动渲染接口 (供 AppController 调用).
    // ==========================================

    /**
     * @brief 刷新左侧的牌组列表
     * @param deckNames 所有可用牌组的名字清单
     */
    void updateDeckListView(const std::vector<QString>& deckNames) const;

    /**
     * @brief 更新右下角的“总体统计”面板
     */
    void updateSummaryStats(int totalCards, double masteryRate, int totalReviews);

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
     * @brief 弹出卡片管理对话框，展示当前牌组的所有卡片并支持逐张删除
     */
    void showCardManagerDialog(const QString& deckName,
                               const std::vector<CardDisplayInfo>& cards);

    /**
     * @brief QML 直接调用的翻牌回调（通过 _reviewBridge context property）
     * 必须是 Q_INVOKABLE 才能被 QML JS 调用
     */
    Q_INVOKABLE void notifyCardFlipped(bool flipped);

    /**
     * @brief 接收大脑传回的日历数据，并驱动日历面板重新绘制
     * @param calendarData 映射表：具体的某一天 -> 那天需要复习的牌组名称列表
     */
    void updateCalendarBoard(const QMap<QDate, QStringList>& calendarData);

    /**
     * @brief 更新右下角的周复习趋势图表
     */
    void updateWeeklyChart(const std::vector<int>& data, const QStringList& labels);

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
    void themeModeChanged();

    void signal_requestStartReview(const QString& deckName);
    void signal_requestSubmitFeedback(int quality);
    void signal_requestShowAnswer();
    void signal_requestResetDeck(const QString& deckName);
    void signal_appWillClose();
    void signal_requestCreateDeck(const QString& deckName);
    void signal_requestDeleteDeck(const QString& deckName);
    void signal_requestAddCard(const QString& deckName, const QString& front, const QString& back);
    void signal_requestManageCards(const QString& deckName);
    void signal_requestDeleteCard(const QString& deckName, const QString& cardId);
    void signal_requestUpdateCard(const QString& deckName, const QString& cardId,
                                  const QString& newFront, const QString& newBack);
    void signal_requestImportDeck(const QString& filePath);
    void signal_requestRefreshDeck(const QString& deckName);
    /**
     * @brief 请求获取指定月份的日历排期数据
     * @param year 年份
     * @param month 月份
     */
    void signal_requestCalendarData(int year, int month, QMap<QDate, QStringList>& outData);

    /**
     * @brief 用户在日历看板上发起了强制调期请求
     * @param deckName 目标牌组名称
     * @param date 用户点击的日期
     * @param status 1(强制复习), -1(强制休假), 0(清除自定义计划)
     */
    void signal_requestUpdateSchedule(const QString& deckName, const QDate& date, int status);

private slots:
    // Monitors QML's "flipped" property change to emit signal_requestShowAnswer.
    // Property-change signals (flippedChanged) are guaranteed in Qt's meta-object
    // system, unlike user-defined QML signals which can fail with the SIGNAL macro.
    void onQmlFlippedPropertyChanged();

    // 弹出偏好设置对话框的槽函数
    void showPreferencesDialog();

protected:
    // 重写关闭事件，用于拦截右上角的红叉
    void closeEvent(QCloseEvent *event) override;

private:
    int m_themeIndex = 0; // 当前主题在 Theme::availableThemes() 中的索引（构造时由配置/默认值覆盖）

    // ===== 左侧导航面板 =====
    QWidget *leftPanel{};
    QListWidget *deckListWidget{};
    QPushButton *addDeckBtn{};
    QPushButton *deleteDeckBtn{};
    QPushButton *resetDeckBtn{};

    // 打开日历看板的入口按钮
    QPushButton *calendarBtn{};

    // ===== 中央看板区 =====
    QWidget *centerPanel{};
    QQuickWidget *flashCardView{};
    QStackedWidget *buttonStack{};   // 问题态与答案态按钮区共占一块空间，消除切换跳动
    QPushButton *showAnswerBtn{};    // 问题态：大的"显示答案"按钮
    QWidget    *feedbackRow{};       // 答案态：4 个评分按钮的容器
    QPushButton *feedbackBtns[4]{};  // 生疏、困难、良好、简单（在 feedbackRow 内）

    // ===== 右侧统计面板 =====
    QWidget *rightPanel{};
    QLabel *progressLabel{};
    QLabel *todayStudyLabel{};
    QLabel *reviewStatsLabel{};
    QProgressBar *dailyProgressBar{};

    WeeklyChartWidget *weeklyChartWidget{};
};

#endif // MAINWINDOW_H
