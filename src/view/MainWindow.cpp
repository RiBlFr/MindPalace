#include "MainWindow.h"

#include <QApplication>
#include <QFile>
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
#include <QQuickWidget>
#include <QQuickItem>
#include <QStackedWidget>
#include <QUrl>
#include <QMetaObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <QQmlContext>
#include <QQmlEngine>

#include "StyleUtils.h"

#include <QCloseEvent>
#include <QShortcut>
#include <QFileDialog>
#include <QMessageBox>

#include "CardManagerDialog.h"
#include "StyledDialogs.h"
#include "ScheduleCalendarDialog.h"
#include "service/storagemanager.h"

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent) {
    setWindowTitle(tr("记忆殿堂"));
    setWindowIcon(QIcon(":/icons/app_icon.png"));

    const QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    resize(screenGeometry.width() * 0.9, screenGeometry.height() * 0.85);

    initUI();

    // 1. 左侧牌组列表点击 → 开始复习 + 激活重置与新增卡片按钮.
    connect(deckListWidget, &QListWidget::currentTextChanged, this, [this](const QString& text) {
        const bool hasSelection = !text.isEmpty();
        resetDeckBtn->setEnabled(hasSelection);
        deleteDeckBtn->setEnabled(hasSelection);

        if (hasSelection) {
            emit signal_requestStartReview(text);
        }
    });

    // 2. 4 个评分按钮，对应枚举值 Again=0, Hard=3, Good=4, Easy=5
    const int qualityMapping[4] = {0, 3, 4, 5};
    for (int i = 0; i < 4; ++i) {
        connect(feedbackBtns[i], &QPushButton::clicked, this, [this, q = qualityMapping[i]]() {
            emit signal_requestSubmitFeedback(q);
        });
    }

    // 3. "显示答案"按钮 + 空格键 → 翻牌（仅当问题态按钮可见时生效）
    auto flipCard = [this]() {
        if (showAnswerBtn->isVisible()) {
            if (auto *root = flashCardView->rootObject()) {
                root->setProperty("flipped", true);
            }
        }
    };
    connect(showAnswerBtn, &QPushButton::clicked, this, flipCard);
    auto *spaceKey = new QShortcut(Qt::Key_Space, this);
    connect(spaceKey, &QShortcut::activated, this, flipCard);

    // 4. 重置卡组按钮
    connect(resetDeckBtn, &QPushButton::clicked, this, [this]() {
        if (auto *item = deckListWidget->currentItem()) {
            emit signal_requestResetDeck(item->text());
        }
    });

    setupStyles();

    // ==========================================
    // 等全局样式全部加载完毕后，再为整个大窗口铺设极光渐变底色！
    // ==========================================
    this->setObjectName("mainWindowRoot");
    setGradientBackground(this);
}

// 3. 拦截窗口关闭事件，通知总控进行数据安全落盘
void MainWindow::closeEvent(QCloseEvent *event) {
    emit signal_appWillClose();
    event->accept();            // 允许窗口正常注销
}

void MainWindow::updateDeckListView(const std::vector<QString>& deckNames) const{
    deckListWidget->clear(); // 先清空旧数据
    for (const auto& name : deckNames) {
        deckListWidget->addItem(name); // 逐个画上新数据
    }
}

void MainWindow::renderQuestionLayout(const QString& frontText, bool hasNextCard) const {
    if (auto *root = flashCardView->rootObject()) {
        root->setProperty("hasNextCard", hasNextCard);
        root->setProperty("questionText", frontText);
        root->setProperty("answerText", QString());
    }
    buttonStack->setCurrentWidget(showAnswerBtn);
    buttonStack->show();
}

void MainWindow::preloadAnswerText(const QString& backText) {
    if (auto *root = flashCardView->rootObject()) {
        root->setProperty("answerText", backText);
    }
}

void MainWindow::renderAnswerLayout(const QString& backText) {
    if (auto *root = flashCardView->rootObject()) {
        root->setProperty("answerText", backText);
    }
    buttonStack->setCurrentWidget(feedbackRow);
    buttonStack->show();
}

void MainWindow::showFinishedSummaryPage() {
    if (auto *root = flashCardView->rootObject()) {
        root->setProperty("hasNextCard", false);
        root->setProperty("questionText", tr("今日复习全部完成！"));
        root->setProperty("answerText", tr("所有卡片已复习完毕，明日再来~"));
    }
    buttonStack->hide();
}

void MainWindow::updateProgressView(int done, int total) {
    progressLabel->setText(QString("%1 / %2").arg(done).arg(total));
    dailyProgressBar->setMaximum(total > 0 ? total : 1);
    dailyProgressBar->setValue(done);
    todayStudyLabel->setText(
        tr("已复习: %1 张\n待复习: %2 张").arg(done).arg(total - done)
    );
}

MainWindow::~MainWindow() = default;

void MainWindow::initUI() {
    // ==========================================
    // 1. 让整个大窗口（包含菜单栏的底部）变成渐变色
    // ==========================================

    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 2. 中央面板设为全透明，直接透出后面的渐变色
    centralWidget->setStyleSheet("background: transparent;");

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
    // 菜单栏高级玻璃质感样式
    menuBar()->setStyleSheet(
        "QMenuBar {"
        "   background-color: transparent;" // 让菜单栏全透明，直接透出后面的极光渐变
        "   border-bottom: 1px solid rgba(255, 255, 255, 80);" // 极细的白色高光分割线
        "}"
        "QMenuBar::item {"
        "   background: transparent;"
        "   padding: 8px 12px;"
        "   margin: 4px 2px;"
        "   border-radius: 6px;" // 圆角反馈
        "}"
        "QMenuBar::item:selected {"
        "   background: rgba(255, 255, 255, 100);" // 鼠标悬停时的半透明高光
        "}"
        "QMenu {"
        "   background-color: rgba(255, 255, 255, 220);" // 下拉菜单的毛玻璃质感
        "   border: 1px solid rgba(255, 255, 255, 200);"
        "   border-radius: 8px;"
        "   padding: 4px;"
        "}"
        "QMenu::item {"
        "   padding: 6px 24px 6px 12px;"
        "   border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: rgba(37, 99, 235, 40);" // 淡淡的主题蓝选中态
        "   color: #2563eb;"
        "}"
    );

    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));

    // 新建卡组：直接复用左侧栏按钮的点击逻辑，避免重复维护两份弹窗代码
    auto *newDeckAction = fileMenu->addAction(tr("新建卡组"));
    newDeckAction->setShortcut(QKeySequence::New);
    connect(newDeckAction, &QAction::triggered, addDeckBtn, &QPushButton::click);

    auto *importAction = fileMenu->addAction(tr("导入卡组(.in/.out)"));
    connect(importAction, &QAction::triggered, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(
            this, tr("选择 .in 文件"), {}, tr("题库文件 (*.in)"));
        if (!filePath.isEmpty())
            emit signal_requestImportDeck(filePath);
    });

    auto *manageAction = fileMenu->addAction(tr("管理当前卡组的卡片"));
    connect(manageAction, &QAction::triggered, this, [this]() {
        if (auto *item = deckListWidget->currentItem())
            emit signal_requestManageCards(item->text());
        else
            QMessageBox::information(this, tr("提示"), tr("请先在左侧选择一个卡组"));
    });

    auto *refreshAction = fileMenu->addAction(tr("刷新当前卡组"));
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, [this]() {
        if (auto *item = deckListWidget->currentItem())
            emit signal_requestRefreshDeck(item->text());
    });

    fileMenu->addSeparator();

    // 退出：触发 close() 走 closeEvent，让 signal_appWillClose 被正常发出
    auto *quitAction = fileMenu->addAction(tr("退出"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

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
    leftLayout->addWidget(deckListWidget, 1);

    // ==========================================
    // 1. 新增卡组按钮 (核心高亮，使用主题长春花蓝)
    // ==========================================
    addDeckBtn = new QPushButton(tr("+ 新增卡组"));
    addDeckBtn->setProperty("variant", "primary");
    addDeckBtn->setMinimumHeight(40);
    setButtonFont(addDeckBtn, 11);
    addDeckBtn->setStyleSheet(
        "QPushButton { "
        "   /* 1. 微凸面受光：使用由亮到暗的微渐变，拒绝死板纯色 */"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #9B88FA, stop:1 #8B75FA);"
        "   color: white; "
        "   border-radius: 8px; "
        "   border: 1px solid #7A61F9; /* 基础轮廓线 */"
        "   /* 2. 灵魂高光：模拟顶部环境光的折射 */"
        "   border-top: 1px solid rgba(255, 255, 255, 0.45); "
        "   /* 3. 底部背光：增加按钮的厚度与抓地力 */"
        "   border-bottom: 1px solid rgba(0, 0, 0, 0.15); "
        "   font-weight: bold; "
        "}"
        "QPushButton:hover { "
        "   /* 悬停时整体变亮，模拟光源拉近 */"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #AD9DFB, stop:1 #9D8AFB);"
        "}"
        "QPushButton:pressed { "
        "   /* 4. 按下时反光面消失，彻底变为平面的暗色，配合内边距下沉 */"
        "   background: #7A61F9;"
        "   border-top: 1px solid transparent;"
        "   padding-top: 2px; "
        "}"
    );
    leftLayout->addWidget(addDeckBtn);

    leftLayout->addSpacing(16);

    // ==========================================
    // 2. 日历看板按钮 (次级高亮，使用温柔浅蓝)
    // ==========================================
    calendarBtn = new QPushButton(tr("📅 复习日历"));
    calendarBtn->setProperty("variant", "primary");
    calendarBtn->setMinimumHeight(44);
    setButtonFont(calendarBtn, 12);
    calendarBtn->setStyleSheet(
        "QPushButton { "
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7D9FFB, stop:1 #638BFA);"
        "   color: white; "
        "   border-radius: 8px; "
        "   border: 1px solid #4A76F9; "
        "   border-top: 1px solid rgba(255, 255, 255, 0.45); "
        "   border-bottom: 1px solid rgba(0, 0, 0, 0.15); "
        "   font-weight: bold; "
        "}"
        "QPushButton:hover { "
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #95B2FC, stop:1 #7D9FFB);"
        "}"
        "QPushButton:pressed { "
        "   background: #4A76F9;"
        "   border-top: 1px solid transparent;"
        "   padding-top: 2px; "
        "}"
    );
    leftLayout->addWidget(calendarBtn);

    // 绑定信号：点击按钮打开日历弹窗
    connect(calendarBtn, &QPushButton::clicked, this, [this]() {
        QStringList allDecks;
        for(int i = 0; i < deckListWidget->count(); ++i) {
            allDecks << deckListWidget->item(i)->text();
        }

        ScheduleCalendarDialog dialog(allDecks, this);

        connect(&dialog, &ScheduleCalendarDialog::signal_requestCalendarData,
                this, &MainWindow::signal_requestCalendarData);
        connect(&dialog, &ScheduleCalendarDialog::signal_requestUpdateSchedule,
                this, &MainWindow::signal_requestUpdateSchedule);

        dialog.exec();
    });

    // ==========================================
    // 3. 删除卡组按钮 (危险操作，半透明毛玻璃底 + 红字)
    // ==========================================
    deleteDeckBtn = new QPushButton(tr("删除卡组"));
    deleteDeckBtn->setProperty("variant", "danger");
    deleteDeckBtn->setMinimumHeight(40);
    setButtonFont(deleteDeckBtn, 11);
    deleteDeckBtn->setEnabled(false);
    deleteDeckBtn->setStyleSheet(
        "QPushButton { background-color: rgba(255, 255, 255, 180); color: #EF4444; border-radius: 8px; border: 1px solid #FECACA; font-weight: bold; }"
        "QPushButton:hover { background-color: #FEE2E2; }"
        "QPushButton:pressed { padding-top: 2px; }" // <--- 增加物理下沉感
        "QPushButton:disabled { color: #FCA5A5; border-color: #FEF2F2; background-color: rgba(255, 255, 255, 50); }"
    );
    leftLayout->addWidget(deleteDeckBtn);

    // ==========================================
    // 4. 重置进度按钮 (警告操作，半透明毛玻璃底 + 橙字)
    // ==========================================
    resetDeckBtn = new QPushButton(tr("重置卡组进度"));
    resetDeckBtn->setProperty("variant", "warning");
    resetDeckBtn->setMinimumHeight(40);
    setButtonFont(resetDeckBtn, 11);
    resetDeckBtn->setEnabled(false);
    resetDeckBtn->setStyleSheet(
        "QPushButton { background-color: rgba(255, 255, 255, 180); color: #F59E0B; border-radius: 8px; border: 1px solid #FDE68A; font-weight: bold; }"
        "QPushButton:hover { background-color: #FEF3C7; }"
        "QPushButton:pressed { padding-top: 2px; }" // <--- 增加物理下沉感
        "QPushButton:disabled { color: #FCD34D; border-color: #FFFBEB; background-color: rgba(255, 255, 255, 50); }"
    );
    leftLayout->addWidget(resetDeckBtn);

    // 绑定基础操作的弹窗信号
    connect(addDeckBtn, &QPushButton::clicked, this, [this]() {
        auto name = StyledDialogs::getText(
            this,
            tr("新建卡组"),
            tr("请输入新卡组的名称"),
            tr("如：英语单词 / 高数公式"));
        if (name) emit signal_requestCreateDeck(*name);
    });

    connect(deleteDeckBtn, &QPushButton::clicked, this, [this]() {
        auto *item = deckListWidget->currentItem();
        if (!item) return;
        const QString name = item->text();
        const bool ok = StyledDialogs::confirm(
            this,
            tr("删除卡组"),
            tr("确定要删除卡组 [%1] 吗？\n该卡组的全部卡片和复习进度都会被永久删除，且无法恢复。").arg(name),
            /*dangerAction=*/true);
        if (ok) emit signal_requestDeleteDeck(name);
    });
}

void MainWindow::setupCenterPanel() {
    centerPanel = new QWidget;
    setBackground(centerPanel, Theme::WindowBg);

    auto *centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(24, 24, 24, 24);
    centerLayout->setSpacing(18);

    initFlashCardView();
    centerLayout->addWidget(flashCardView, 1);

    // 问题态：大的"显示答案"按钮
    showAnswerBtn = new QPushButton(tr("显示答案  （空格键）"));
    showAnswerBtn->setProperty("variant", "primary");
    showAnswerBtn->setMinimumHeight(62);
    setButtonFont(showAnswerBtn, 13);

    // 答案态：4 个评分按钮容器
    feedbackRow = setupFeedbackButtons();

    // 两者放入 QStackedWidget，始终占据相同高度，切换时布局不跳动
    buttonStack = new QStackedWidget;
    buttonStack->addWidget(showAnswerBtn);
    buttonStack->addWidget(feedbackRow);
    buttonStack->hide();
    centerLayout->addWidget(buttonStack);
}

void MainWindow::initFlashCardView() {
    flashCardView = new QQuickWidget(centerPanel);
    flashCardView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    flashCardView->setClearColor(Qt::transparent);
    flashCardView->setMinimumHeight(420);

    // Expose this MainWindow to QML as "_reviewBridge" so FlashCardStack can
    // call notifyCardFlipped() directly. This must happen before setSource()
    // so the property is available when the QML component initialises.
    flashCardView->engine()->rootContext()->setContextProperty(
        QStringLiteral("_reviewBridge"), this);

    flashCardView->setSource(QUrl(QStringLiteral("qrc:/styles/FlashCardStack.qml")));

    if (auto *root = flashCardView->rootObject()) {
        root->setProperty("questionText", tr("从左侧选择卡组开始学习"));
    } else {
        qWarning() << "FlashCardStack rootObject is null — QML failed to load";
    }
}

void MainWindow::onQmlFlippedPropertyChanged() {
    // Kept as a fallback slot; actual notification goes through notifyCardFlipped().
}

void MainWindow::notifyCardFlipped(bool flipped) {
    if (flipped) emit signal_requestShowAnswer();
}

QWidget* MainWindow::setupFeedbackButtons() {
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    const QStringList feedbackTexts = {
            tr("忘记\n(F1)"),
            tr("困难\n(F2)"),
            tr("普通\n(F3)"),
            tr("熟悉\n(F4)")
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
        layout->addWidget(feedbackBtns[i]);
    }

    return row;
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

    // ==========================================
    // 近七日趋势图表区
    // ==========================================
    auto *chartTitle = new QLabel(tr("近七日复习趋势"));
    setLabelStyle(chartTitle, 12, QFont::DemiBold);
    rightLayout->addWidget(chartTitle);

    auto *chartFrame = new QFrame;
    markSurface(chartFrame); // 使用你封装好的卡片样式工具
    auto *chartFrameLayout = new QVBoxLayout(chartFrame);
    chartFrameLayout->setContentsMargins(10, 10, 10, 10);

    weeklyChartWidget = new WeeklyChartWidget(this);

    // 【修改】初始化时读取一次真实数据
    std::vector<int> initData;
    QStringList initLabels;
    // 提示：你需要在 MainWindow.cpp 顶部引入 #include "service/storagemanager.h"
    MindPalace::Service::StorageManager::getWeeklyReviewData(initData, initLabels);
    weeklyChartWidget->setData(initData, initLabels);

    chartFrameLayout->addWidget(weeklyChartWidget);
    rightLayout->addWidget(chartFrame);

    rightLayout->addStretch();
}

void MainWindow::showCardManagerDialog(const QString& deckName,
                                       const std::vector<CardDisplayInfo>& cards) {
    CardManagerDialog dialog(deckName, cards, this);
    // 把对话框内部的增/删/改请求转发为 MainWindow 已有的信号，AppController 不感知 dialog 类型
    connect(&dialog, &CardManagerDialog::signal_requestDeleteCard,
            this, &MainWindow::signal_requestDeleteCard);
    connect(&dialog, &CardManagerDialog::signal_requestAddCard,
            this, &MainWindow::signal_requestAddCard);
    connect(&dialog, &CardManagerDialog::signal_requestUpdateCard,
            this, &MainWindow::signal_requestUpdateCard);
    dialog.exec();
}

void MainWindow::setupStyles() {
    setBackground(this, Theme::WindowBg);

    QFile qssFile(":/styles/MainWindow.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to load QSS resource" << qssFile.fileName() << qssFile.errorString();
        return;
    }

    setStyleSheet(QString::fromUtf8(qssFile.readAll()));
}

void MainWindow::updateSummaryStats(int totalCards, double masteryRate, int totalReviews) {
    // 将小数转换为百分比显示，保留一位小数
    QString rateStr = QString::number(masteryRate * 100.0, 'f', 1) + "%";

    reviewStatsLabel->setText(
        tr("总卡片数: %1\n掌握率: %2\n已复习卡片数: %3")
        .arg(totalCards)
        .arg(rateStr)
        .arg(totalReviews)
    );
}

void MainWindow::updateWeeklyChart(const std::vector<int>& data, const QStringList& labels) {
    if (weeklyChartWidget) {
        weeklyChartWidget->setData(data, labels);
    }
}