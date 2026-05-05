#include "MainWindow.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace {

    namespace Theme {
        const QColor WindowBg(0xf7f8fb);
        const QColor SideBg(0xf3f6fa);
        const QColor Surface(0xffffff);

        const QColor Text(0x1f2937);
        const QColor MutedText(0x64748b);

        const QColor Primary(0x2563eb);
        const QColor Success(0x16a34a);
    }

    void setBackground(QWidget *widget, const QColor &color) {
    QPalette pal = widget->palette();
    pal.setColor(QPalette::Window, color);
    widget->setAutoFillBackground(true);
    widget->setPalette(pal);
}

void setTextColor(QWidget *widget, const QColor &color) {
QPalette pal = widget->palette();
pal.setColor(QPalette::WindowText, color);
pal.setColor(QPalette::ButtonText, color);
widget->setPalette(pal);
}

void setLabelStyle(
        QLabel *label,
        int pointSize,
        QFont::Weight weight = QFont::Normal,
        const QColor &color = Theme::Text
) {
    QFont font = label->font();
    font.setPointSize(pointSize);
    font.setWeight(weight);
    label->setFont(font);
    setTextColor(label, color);
}

void addSoftShadow(QWidget *widget) {
    auto *shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(15, 23, 42, 28));
    widget->setGraphicsEffect(shadow);
}

void markSurface(QFrame *frame, bool shadow = true) {
    frame->setProperty("role", "surface");
    setBackground(frame, Theme::Surface);

    if (shadow) {
        addSoftShadow(frame);
    }
}

void setButtonFont(QPushButton *button, int pointSize = 11) {
    QFont font = button->font();
    font.setPointSize(pointSize);
    font.setWeight(QFont::DemiBold);
    button->setFont(font);
}

}

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent) {
    setWindowTitle(tr("记忆殿堂"));
    setWindowIcon(QIcon(":/icons/app_icon.png"));

    const QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    resize(screenGeometry.width() * 0.9, screenGeometry.height() * 0.85);

    initUI();
    setupStyles();
}

MainWindow::~MainWindow() = default;

void MainWindow::initUI() {
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    setBackground(centralWidget, Theme::WindowBg);

    auto *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupLeftPanel();
    setupCenterPanel();
    setupRightPanel();

    mainLayout->addWidget(leftPanel, 0);
    mainLayout->addWidget(centerPanel, 1);
    mainLayout->addWidget(rightPanel, 0);

    setupMenuBar();
}

void MainWindow::setupMenuBar() {
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("新建卡组"));
    fileMenu->addAction(tr("导入卡组"));
    fileMenu->addAction(tr("导出卡组"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出"));

    QMenu *editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    editMenu->addAction(tr("偏好设置"));

    QMenu *viewMenu = menuBar()->addMenu(tr("查看(&V)"));
    viewMenu->addAction(tr("刷新统计"));

    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(tr("关于"));
    helpMenu->addAction(tr("使用说明"));
}

void MainWindow::setupLeftPanel() {
    leftPanel = new QWidget;
    leftPanel->setProperty("panel", "side");
    leftPanel->setMaximumWidth(280);
    leftPanel->setMinimumWidth(220);
    setBackground(leftPanel, Theme::SideBg);

    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 18, 16, 18);
    leftLayout->setSpacing(12);

    auto *deckTitle = new QLabel(tr("学习卡组"));
    setLabelStyle(deckTitle, 13, QFont::DemiBold);
    leftLayout->addWidget(deckTitle);

    deckListWidget = new QListWidget;
    deckListWidget->setObjectName("deckList");
    deckListWidget->setFrameShape(QFrame::NoFrame);
    deckListWidget->setAlternatingRowColors(false);

    deckListWidget->addItem(tr("GRE 词汇"));
    deckListWidget->addItem(tr("日本語単語"));
    deckListWidget->addItem(tr("编程语言概念"));
    deckListWidget->addItem(tr("世界地理"));

    leftLayout->addWidget(deckListWidget, 1);

    addDeckBtn = new QPushButton(tr("+ 新增卡组"));
    addDeckBtn->setProperty("variant", "primary");
    addDeckBtn->setMinimumHeight(40);
    setButtonFont(addDeckBtn, 11);

    leftLayout->addWidget(addDeckBtn);
}

void MainWindow::setupCenterPanel() {
    centerPanel = new QWidget;
    setBackground(centerPanel, Theme::WindowBg);

    auto *centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(24, 24, 24, 24);
    centerLayout->setSpacing(18);

    cardFrame = new QFrame;
    markSurface(cardFrame);
    cardFrame->setMinimumHeight(360);

    auto *cardLayout = new QVBoxLayout(cardFrame);
    cardLayout->setContentsMargins(36, 36, 36, 36);
    cardLayout->setSpacing(20);

    cardFrontLabel = new QLabel(tr("正面\n\n点击卡片查看答案"));
    cardFrontLabel->setAlignment(Qt::AlignCenter);
    cardFrontLabel->setWordWrap(true);
    cardFrontLabel->setMinimumHeight(280);
    cardFrontLabel->setCursor(Qt::PointingHandCursor);
    setLabelStyle(cardFrontLabel, 26, QFont::Bold, Theme::Text);

    cardLayout->addWidget(cardFrontLabel, 1);

    cardBackLabel = new QLabel(tr("反面\n\n隐藏中..."));
    cardBackLabel->setAlignment(Qt::AlignCenter);
    cardBackLabel->setWordWrap(true);
    setLabelStyle(cardBackLabel, 23, QFont::Bold, Theme::Success);
    cardBackLabel->hide();

    cardLayout->addWidget(cardBackLabel, 1);

    centerLayout->addWidget(cardFrame, 1);

    auto *feedbackLayout = new QHBoxLayout;
    feedbackLayout->setSpacing(12);

    const QStringList feedbackTexts = {
            tr("生疏\n(F1)"),
            tr("困难\n(F2)"),
            tr("良好\n(F3)"),
            tr("简单\n(F4)")
    };

    const QStringList variants = {
            "danger",
            "warning",
            "normal",
            "success"
    };

    for (int i = 0; i < 4; ++i) {
        feedbackBtns[i] = new QPushButton(feedbackTexts[i]);
        feedbackBtns[i]->setProperty("variant", variants[i]);
        feedbackBtns[i]->setMinimumHeight(62);
        setButtonFont(feedbackBtns[i], 11);

        feedbackLayout->addWidget(feedbackBtns[i]);
    }

    centerLayout->addLayout(feedbackLayout);
}

void MainWindow::setupRightPanel() {
    rightPanel = new QWidget;
    rightPanel->setProperty("panel", "side");
    rightPanel->setMaximumWidth(300);
    rightPanel->setMinimumWidth(250);
    setBackground(rightPanel, Theme::SideBg);

    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 18, 16, 18);
    rightLayout->setSpacing(12);

    auto *statsTitle = new QLabel(tr("学习进度"));
    setLabelStyle(statsTitle, 13, QFont::DemiBold);
    rightLayout->addWidget(statsTitle);

    auto *progressFrame = new QFrame;
    markSurface(progressFrame);

    auto *progressLayout = new QVBoxLayout(progressFrame);
    progressLayout->setContentsMargins(16, 16, 16, 16);
    progressLayout->setSpacing(12);

    progressLabel = new QLabel(tr("0 / 0"));
    progressLabel->setAlignment(Qt::AlignCenter);
    setLabelStyle(progressLabel, 18, QFont::Bold, Theme::Primary);
    progressLayout->addWidget(progressLabel);

    dailyProgressBar = new QProgressBar;
    dailyProgressBar->setObjectName("dailyProgress");
    dailyProgressBar->setRange(0, 100);
    dailyProgressBar->setValue(0);
    dailyProgressBar->setTextVisible(true);
    progressLayout->addWidget(dailyProgressBar);

    rightLayout->addWidget(progressFrame);

    auto *todayTitle = new QLabel(tr("今日复习"));
    setLabelStyle(todayTitle, 12, QFont::DemiBold);
    rightLayout->addWidget(todayTitle);

    auto *todayFrame = new QFrame;
    markSurface(todayFrame);

    auto *todayLayout = new QVBoxLayout(todayFrame);
    todayLayout->setContentsMargins(14, 14, 14, 14);
    todayLayout->setSpacing(8);

    todayStudyLabel = new QLabel(
            tr("已复习: 0 张\n"
               "待复习: 0 张")
    );
    todayStudyLabel->setWordWrap(true);
    setLabelStyle(todayStudyLabel, 11, QFont::Normal, Theme::MutedText);
    todayLayout->addWidget(todayStudyLabel);

    rightLayout->addWidget(todayFrame);

    auto *summaryTitle = new QLabel(tr("总体统计"));
    setLabelStyle(summaryTitle, 12, QFont::DemiBold);
    rightLayout->addWidget(summaryTitle);

    auto *summaryFrame = new QFrame;
    markSurface(summaryFrame);

    auto *summaryLayout = new QVBoxLayout(summaryFrame);
    summaryLayout->setContentsMargins(14, 14, 14, 14);
    summaryLayout->setSpacing(8);

    reviewStatsLabel = new QLabel(
            tr("总卡片数: 0\n"
               "掌握率: 0%\n"
               "复习次数: 0")
    );
    reviewStatsLabel->setWordWrap(true);
    setLabelStyle(reviewStatsLabel, 11, QFont::Normal, Theme::MutedText);
    summaryLayout->addWidget(reviewStatsLabel);

    rightLayout->addWidget(summaryFrame);
    rightLayout->addStretch();
}

void MainWindow::setupStyles() {
    setBackground(this, Theme::WindowBg);

    setStyleSheet(R"(
        QMainWindow {
            background: #f7f8fb;
        }

        QMenuBar {
            background: #ffffff;
            border-bottom: 1px solid #e5e7eb;
            padding: 2px;
        }

        QMenuBar::item {
            padding: 6px 10px;
            border-radius: 6px;
            color: #1f2937;
        }

        QMenuBar::item:selected {
            background: #eef2ff;
            color: #2563eb;
        }

        QMenu {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            padding: 6px;
        }

        QMenu::item {
            padding: 7px 28px 7px 18px;
            border-radius: 6px;
            color: #1f2937;
        }

        QMenu::item:selected {
            background: #eff6ff;
            color: #2563eb;
        }

        QWidget[panel="side"] {
            background: #f3f6fa;
        }

        QFrame[role="surface"] {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 14px;
        }

        QListWidget#deckList {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
            padding: 6px;
            outline: none;
        }

        QListWidget#deckList::item {
            padding: 10px 12px;
            margin: 2px;
            border-radius: 8px;
            color: #334155;
        }

        QListWidget#deckList::item:hover {
            background: #eff6ff;
            color: #1d4ed8;
        }

        QListWidget#deckList::item:selected {
            background: #2563eb;
            color: #ffffff;
        }

        QPushButton {
            min-height: 34px;
            padding: 8px 16px;
            border: none;
            border-radius: 8px;
            font-weight: 600;
        }

        QPushButton[variant="primary"] {
            background: #2563eb;
            color: #ffffff;
        }

        QPushButton[variant="primary"]:hover {
            background: #1d4ed8;
        }

        QPushButton[variant="danger"] {
            background: #ef4444;
            color: #ffffff;
        }

        QPushButton[variant="danger"]:hover {
            background: #dc2626;
        }

        QPushButton[variant="warning"] {
            background: #f97316;
            color: #ffffff;
        }

        QPushButton[variant="warning"]:hover {
            background: #ea580c;
        }

        QPushButton[variant="normal"] {
            background: #f59e0b;
            color: #ffffff;
        }

        QPushButton[variant="normal"]:hover {
            background: #d97706;
        }

        QPushButton[variant="success"] {
            background: #16a34a;
            color: #ffffff;
        }

        QPushButton[variant="success"]:hover {
            background: #15803d;
        }

        QPushButton:pressed {
            padding-top: 10px;
            padding-bottom: 6px;
        }

        QProgressBar#dailyProgress {
            height: 18px;
            border: none;
            border-radius: 9px;
            background: #e5e7eb;
            text-align: center;
            color: #334155;
            font-size: 11px;
        }

        QProgressBar#dailyProgress::chunk {
            border-radius: 9px;
            background: #2563eb;
        }
    )");
}
