#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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

private:
    // UI 初始化
    void initUI();
    void setupMenuBar();
    void setupLeftPanel();
    void setupCenterPanel();
    void setupRightPanel();
    void setupStyles();

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
