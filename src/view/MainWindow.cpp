#include "MainWindow.h"
#include <QCheckBox>
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
#include <QTextBrowser>
#include <QUrl>
#include <QMetaObject>
#include <QSettings>
#include <QDialog>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QSignalBlocker>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QPropertyAnimation>
#include <QQmlContext>
#include <QQmlEngine>

#include "StyleUtils.h"
#include "ThemeRegistry.h"
#include "CardManagerDialog.h"
#include "StyledDialogs.h"
#include "ScheduleCalendarDialog.h"
#include "service/storagemanager.h"

#include <QSurfaceFormat>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent) {
    setWindowTitle(tr("记忆殿堂"));
    setWindowIcon(QIcon(":/icons/app_icon.png"));

    const QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    resize(screenGeometry.width() * 0.9, screenGeometry.height() * 0.85);

    // ==========================================
    // 1. 启动时读取用户的历史主题选择
    //    优先按稳定的 themeKey 还原；兼容旧版本仅存了 themeMode 整型索引的情况。
    // ==========================================
    QSettings settings("MindPalace", "Settings");
    if (settings.contains("themeKey")) {
        m_themeIndex = Theme::indexForKey(settings.value("themeKey").toString());
    } else {
        // 旧版本回退：themeMode 曾是 0=经典 / 1=极光，与注册表索引一致
        const int legacy = settings.value("themeMode", Theme::indexForKey("aurora")).toInt();
        m_themeIndex = (legacy >= 0 && legacy < Theme::themeCount()) ? legacy : 0;
    }

    initUI();

    // 绑定左侧牌组列表点击事件
    connect(deckListWidget, &QListWidget::currentTextChanged, this, [this](const QString& text) {
        const bool hasSelection = !text.isEmpty();
        resetDeckBtn->setEnabled(hasSelection);
        deleteDeckBtn->setEnabled(hasSelection);
        if (hasSelection) emit signal_requestStartReview(text);
    });

    // 绑定 4 个评分按钮
    const int qualityMapping[4] = {0, 3, 4, 5};
    for (int i = 0; i < 4; ++i) {
        connect(feedbackBtns[i], &QPushButton::clicked, this, [this, q = qualityMapping[i]]() {
            emit signal_requestSubmitFeedback(q);
        });
    }

    // 绑定 F1-F4 评分热键：仅在答案态（评分按钮可见）时触发对应按钮
    const Qt::Key feedbackKeys[4] = {Qt::Key_F1, Qt::Key_F2, Qt::Key_F3, Qt::Key_F4};
    for (int i = 0; i < 4; ++i) {
        auto *scoreKey = new QShortcut(feedbackKeys[i], this);
        connect(scoreKey, &QShortcut::activated, this, [this, i]() {
            if (feedbackRow->isVisible()) feedbackBtns[i]->click();
        });
    }

    // 绑定翻牌热键
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

    // 绑定重置按钮
    connect(resetDeckBtn, &QPushButton::clicked, this, [this]() {
        if (auto *item = deckListWidget->currentItem()) {
            emit signal_requestResetDeck(item->text());
        }
    });

    // ==========================================
    // 2. 统一渲染入口：打上根节点 ID，并首次应用主题
    // ==========================================
    this->setObjectName("mainWindowRoot");
    applyTheme(m_themeIndex);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    emit signal_appWillClose();
    event->accept();
}

void MainWindow::updateDeckListView(const std::vector<QString>& deckNames) const{
    // 记录重建前选中的卡组。增删卡片 / 牌组都会触发列表重建，若不还原选区，
    // currentTextChanged 不会再次发出，右侧“总体统计/掌握率”就会丢失上下文而显示陈旧值。
    const QString previous =
            deckListWidget->currentItem() ? deckListWidget->currentItem()->text() : QString();

    {
        // 重建过程中屏蔽信号，避免 clear()/addItem() 触发的中间态反复重启复习会话与统计刷新
        const QSignalBlocker blocker(deckListWidget);
        deckListWidget->clear();
        for (const auto& name : deckNames) {
            deckListWidget->addItem(name);
        }
        deckListWidget->setCurrentRow(-1);
    }

    // 选区策略：优先还原之前选中的卡组；若它已被删除则回退到第一个，
    // 保证主界面始终有一个选中的卡组，掌握率等统计随之实时刷新。
    int rowToSelect = -1;
    if (!previous.isEmpty()) {
        const auto matches = deckListWidget->findItems(previous, Qt::MatchExactly);
        if (!matches.isEmpty()) {
            rowToSelect = deckListWidget->row(matches.first());
        }
    }
    if (rowToSelect < 0 && deckListWidget->count() > 0) {
        rowToSelect = 0;
    }
    if (rowToSelect >= 0) {
        // 解除屏蔽后单次设置选区，触发一次 currentTextChanged → 刷新掌握率等统计
        deckListWidget->setCurrentRow(rowToSelect);
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
    if (auto *root = flashCardView->rootObject()) root->setProperty("answerText", backText);
}

void MainWindow::renderAnswerLayout(const QString& backText) {
    if (auto *root = flashCardView->rootObject()) root->setProperty("answerText", backText);
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
    todayStudyLabel->setText(tr("已复习: %1 张\n待复习: %2 张").arg(done).arg(total - done));
}

MainWindow::~MainWindow() = default;

void MainWindow::initUI() {
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    centralWidget->setObjectName("centerPanel"); // 交由 QSS 管理背景

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

    auto *newDeckAction = fileMenu->addAction(tr("新建卡组"));
    newDeckAction->setShortcut(QKeySequence::New);
    connect(newDeckAction, &QAction::triggered, addDeckBtn, &QPushButton::click);

    auto *importAction = fileMenu->addAction(tr("导入卡组(.in/.out)"));
    connect(importAction, &QAction::triggered, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(this, tr("选择 .in 文件"), {}, tr("题库文件 (*.in)"));
        if (!filePath.isEmpty()) emit signal_requestImportDeck(filePath);
    });

    auto *manageAction = fileMenu->addAction(tr("管理当前卡组的卡片"));
    connect(manageAction, &QAction::triggered, this, [this]() {
        if (auto *item = deckListWidget->currentItem()) emit signal_requestManageCards(item->text());
        else StyledDialogs::info(this, tr("提示"), tr("请先在左侧选择一个卡组"));
    });

    auto *refreshAction = fileMenu->addAction(tr("刷新当前卡组"));
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, [this]() {
        if (auto *item = deckListWidget->currentItem()) emit signal_requestRefreshDeck(item->text());
    });

    fileMenu->addSeparator();

    auto *quitAction = fileMenu->addAction(tr("退出"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *settingsMenu = menuBar()->addMenu(tr("设置(&S)"));
    auto *prefAction = settingsMenu->addAction(tr("偏好设置"));
    connect(prefAction, &QAction::triggered, this, &MainWindow::showPreferencesDialog);

    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    auto *howToUseAction = helpMenu->addAction(tr("使用说明"));
    connect(howToUseAction, &QAction::triggered, this, [this]() {
        showMarkdownDialog(tr("使用说明"), QStringLiteral(":/docs/HowToUse.md"));
    });
    auto *aboutAction = helpMenu->addAction(tr("关于"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        showMarkdownDialog(tr("关于"), QStringLiteral(":/docs/AboutUs.md"));
    });
}

void MainWindow::showMarkdownDialog(const QString& title, const QString& resourcePath) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        StyledDialogs::info(this, title, tr("无法加载文档：%1").arg(resourcePath));
        return;
    }
    const QString markdown = QString::fromUtf8(file.readAll());

    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(640, 560);
    StyledDialogs::applyStyle(&dialog);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(14);

    auto *browser = new QTextBrowser(&dialog);
    browser->setOpenExternalLinks(true);
    browser->setMarkdown(markdown);
    layout->addWidget(browser, 1);

    auto *footer = new QHBoxLayout;
    footer->addStretch();
    auto *okBtn = new QPushButton(tr("关闭"), &dialog);
    okBtn->setObjectName("dialogPrimary");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    footer->addWidget(okBtn);
    layout->addLayout(footer);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void MainWindow::setupLeftPanel() {
    leftPanel = new QWidget;
    leftPanel->setProperty("panel", "side");
    leftPanel->setMaximumWidth(280);
    leftPanel->setMinimumWidth(220);

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

    addDeckBtn = new QPushButton(tr("+ 新增卡组"));
    addDeckBtn->setObjectName("btnAddDeck");
    addDeckBtn->setMinimumHeight(40);
    setButtonFont(addDeckBtn, 11);
    leftLayout->addWidget(addDeckBtn);

    leftLayout->addSpacing(16);

    calendarBtn = new QPushButton(tr("📅 复习日历"));
    calendarBtn->setObjectName("btnCalendar");
    calendarBtn->setMinimumHeight(44);
    setButtonFont(calendarBtn, 12);
    leftLayout->addWidget(calendarBtn);

    deleteDeckBtn = new QPushButton(tr("删除卡组"));
    deleteDeckBtn->setObjectName("btnDeleteDeck");
    deleteDeckBtn->setMinimumHeight(40);
    setButtonFont(deleteDeckBtn, 11);
    deleteDeckBtn->setEnabled(false);
    leftLayout->addWidget(deleteDeckBtn);

    resetDeckBtn = new QPushButton(tr("重置卡组进度"));
    resetDeckBtn->setObjectName("btnResetDeck");
    resetDeckBtn->setMinimumHeight(40);
    setButtonFont(resetDeckBtn, 11);
    resetDeckBtn->setEnabled(false);
    leftLayout->addWidget(resetDeckBtn);

    connect(addDeckBtn, &QPushButton::clicked, this, [this]() {
        auto name = StyledDialogs::getText(this, tr("新建卡组"), tr("请输入新卡组的名称"), tr("如：英语单词 / 高数公式"));
        if (name) emit signal_requestCreateDeck(*name);
    });

    connect(deleteDeckBtn, &QPushButton::clicked, this, [this]() {
        auto *item = deckListWidget->currentItem();
        if (!item) return;
        const QString name = item->text();
        const bool ok = StyledDialogs::confirm(this, tr("删除卡组"), tr("确定要删除卡组 [%1] 吗？").arg(name), true);
        if (ok) emit signal_requestDeleteDeck(name);
    });

    connect(calendarBtn, &QPushButton::clicked, this, [this]() {
        QStringList allDecks;
        for(int i = 0; i < deckListWidget->count(); ++i) allDecks << deckListWidget->item(i)->text();
        ScheduleCalendarDialog dialog(allDecks, this);
        connect(&dialog, &ScheduleCalendarDialog::signal_requestCalendarData, this, &MainWindow::signal_requestCalendarData);
        connect(&dialog, &ScheduleCalendarDialog::signal_requestUpdateSchedule, this, &MainWindow::signal_requestUpdateSchedule);
        dialog.exec();
    });
}

void MainWindow::setupCenterPanel() {
    centerPanel = new QWidget;
    centerPanel->setObjectName("centerPanel");

    auto *centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(24, 24, 24, 24);
    centerLayout->setSpacing(18);

    initFlashCardView();
    centerLayout->addWidget(flashCardView, 1);

    showAnswerBtn = new QPushButton(tr("显示答案  （空格键）"));
    showAnswerBtn->setObjectName("btnShowAnswer"); // 极其关键的 ID
    showAnswerBtn->setMinimumHeight(62);
    setButtonFont(showAnswerBtn, 13);

    feedbackRow = setupFeedbackButtons();

    buttonStack = new QStackedWidget;
    buttonStack->addWidget(showAnswerBtn);
    buttonStack->addWidget(feedbackRow);
    buttonStack->hide();
    centerLayout->addWidget(buttonStack);

    // 悬浮徽章必须最后创建：它不进布局，而是叠加在闪卡看板之上
    setupStreakBadge();
}

void MainWindow::setupStreakBadge() {
    // 徽章是 centerPanel 的子控件但不加入布局，靠手动 move() 悬浮在闪卡顶部居中。
    streakBadge = new QLabel(centerPanel);
    streakBadge->setObjectName("streakBadge");
    streakBadge->setAlignment(Qt::AlignCenter);
    streakBadge->setAttribute(Qt::WA_TransparentForMouseEvents); // 不挡住下方翻牌/点击
    streakBadge->setTextInteractionFlags(Qt::NoTextInteraction);
    streakBadge->hide();

    // 彩色光晕：档位越高颜色越炽烈，连胜增长时还会脉冲一下。
    streakGlow = new QGraphicsDropShadowEffect(streakBadge);
    streakGlow->setOffset(0, 0);
    streakGlow->setBlurRadius(24);
    streakGlow->setColor(QColor(255, 140, 0, 220));
    streakBadge->setGraphicsEffect(streakGlow);

    streakPulse = new QPropertyAnimation(streakGlow, "blurRadius", this);
    streakPulse->setDuration(460);
}

void MainWindow::initFlashCardView() {
    flashCardView = new QQuickWidget(centerPanel);

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    flashCardView->setFormat(format);

    flashCardView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    // 卡片四周/下方的留白统一刷白：之前依赖 QQuickWidget 透明合成，
    // 在部分 GPU 上会把未覆盖区域渲染成黑色。改用不透明白色底，彻底消除黑边。
    flashCardView->setClearColor(Qt::white);
    flashCardView->setAttribute(Qt::WA_TranslucentBackground, false);
    flashCardView->setAttribute(Qt::WA_OpaquePaintEvent, true);

    flashCardView->setMinimumHeight(420);

    flashCardView->engine()->rootContext()->setContextProperty(QStringLiteral("_reviewBridge"), this);
    flashCardView->setSource(QUrl(QStringLiteral("qrc:/styles/FlashCardStack.qml")));

    if (auto *root = flashCardView->rootObject()) {
        root->setProperty("questionText", tr("从左侧选择卡组开始学习"));
    }
}

void MainWindow::onQmlFlippedPropertyChanged() {}

void MainWindow::notifyCardFlipped(bool flipped) {
    if (flipped) emit signal_requestShowAnswer();
}

QWidget* MainWindow::setupFeedbackButtons() {
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    const QStringList feedbackTexts = { tr("忘记\n(F1)"), tr("困难\n(F2)"), tr("普通\n(F3)"), tr("熟悉\n(F4)") };
    const QStringList objNames = { "btnForget", "btnHard", "btnNormal", "btnEasy" }; // 极其关键的 ID

    for (int i = 0; i < 4; ++i) {
        feedbackBtns[i] = new QPushButton(feedbackTexts[i]);
        feedbackBtns[i]->setObjectName(objNames[i]);
        feedbackBtns[i]->setMinimumHeight(64);
        setButtonFont(feedbackBtns[i], 12);
        layout->addWidget(feedbackBtns[i]);
    }
    return row;
}

void MainWindow::setupRightPanel() {
    rightPanel = new QWidget;
    rightPanel->setProperty("panel", "side");
    rightPanel->setMaximumWidth(300);
    rightPanel->setMinimumWidth(250);

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

    todayStudyLabel = new QLabel(tr("已复习: 0 张\n待复习: 0 张"));
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

    reviewStatsLabel = new QLabel(tr("总卡片数: 0\n掌握率: 0%\n复习次数: 0"));
    reviewStatsLabel->setWordWrap(true);
    setLabelStyle(reviewStatsLabel, 11, QFont::Normal, Theme::MutedText);
    summaryLayout->addWidget(reviewStatsLabel);

    rightLayout->addWidget(summaryFrame);
    rightLayout->addStretch();

    auto *chartTitle = new QLabel(tr("近七日复习趋势"));
    setLabelStyle(chartTitle, 12, QFont::DemiBold);
    rightLayout->addWidget(chartTitle);

    auto *chartFrame = new QFrame;
    markSurface(chartFrame);
    auto *chartFrameLayout = new QVBoxLayout(chartFrame);
    chartFrameLayout->setContentsMargins(10, 10, 10, 10);

    weeklyChartWidget = new WeeklyChartWidget(this);

    std::vector<int> initData;
    QStringList initLabels;
    MindPalace::Service::StorageManager::getWeeklyReviewData(initData, initLabels);
    weeklyChartWidget->setData(initData, initLabels);

    chartFrameLayout->addWidget(weeklyChartWidget);
    rightLayout->addWidget(chartFrame);
    rightLayout->addStretch();
}

void MainWindow::showCardManagerDialog(const QString& deckName, const std::vector<CardDisplayInfo>& cards) {
    CardManagerDialog dialog(deckName, cards, this);
    connect(&dialog, &CardManagerDialog::signal_requestDeleteCard, this, &MainWindow::signal_requestDeleteCard);
    connect(&dialog, &CardManagerDialog::signal_requestAddCard, this, &MainWindow::signal_requestAddCard);
    connect(&dialog, &CardManagerDialog::signal_requestUpdateCard, this, &MainWindow::signal_requestUpdateCard);
    dialog.exec();
}

void MainWindow::setupStyles() {
    // 从主题注册表取当前主题的 QSS 路径，新增主题无需改动此处
    const QString qssPath = Theme::themeAt(m_themeIndex).qssPath;
    QFile qssFile(qssPath);
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to load QSS resource" << qssFile.fileName();
        return;
    }
    setStyleSheet(QString::fromUtf8(qssFile.readAll()));
}

void MainWindow::updateSummaryStats(int totalCards, double masteryRate, int totalReviews) {
    QString rateStr = QString::number(masteryRate * 100.0, 'f', 1) + "%";
    reviewStatsLabel->setText(tr("总卡片数: %1\n掌握率: %2\n已复习卡片数: %3").arg(totalCards).arg(rateStr).arg(totalReviews));
}

void MainWindow::updateWeeklyChart(const std::vector<int>& data, const QStringList& labels) {
    if (weeklyChartWidget) weeklyChartWidget->setData(data, labels);
}

namespace {
// “火热连胜”分档：阈值递增，每档有独立文案、配色与光晕颜色。
    struct StreakTier {
        int threshold;        // 达到该连胜张数即进入此档
        QString name;         // 档位名称（含 emoji）
        QString bg1, bg2;     // 徽章渐变背景起止色
        QString textColor;    // 文字颜色
        QColor glow;          // 光晕颜色
        int fontPx;           // 字号
    };

// 注意：必须按阈值从高到低排列，便于线性向下匹配。
    const StreakTier* tierForStreak(int streak) {
        static const StreakTier tiers[] = {
                {15, QStringLiteral("👑 GODLIKE 神之连胜"), "#00e5ff", "#7c4dff", "#ffffff", QColor( 0, 229, 255, 235), 24},
                {10, QStringLiteral("⭐ LEGENDARY 传说"),    "#ffd54f", "#ff7043", "#4a2600", QColor(255, 193,  7, 235), 23},
                { 7, QStringLiteral("⚡ RAMPAGE 暴走"),       "#b388ff", "#7c4dff", "#ffffff", QColor(149, 117, 205, 230), 21},
                { 4, QStringLiteral("🔥 火热连胜 ON FIRE"),   "#ff8a65", "#f4511e", "#ffffff", QColor(255,  87, 34, 225), 20},
                { 2, QStringLiteral("✨ 连胜"),               "#ffe082", "#ffb300", "#5d4037", QColor(255, 179,  0, 210), 18},
        };
        for (const auto& t : tiers) {
            if (streak >= t.threshold) return &t;
        }
        return nullptr;
    }
} // namespace

void MainWindow::updateStreakBadge(int easyStreak) {
    if (!streakBadge) return;

    const StreakTier* tier = tierForStreak(easyStreak);
    if (!tier) {
        // 连胜中断或不足 2 连：收起徽章并停掉残留动画。
        if (streakPulse) streakPulse->stop();
        streakBadge->hide();
        return;
    }

    const bool wasVisible = streakBadge->isVisible();

    streakBadge->setText(QStringLiteral("%1  ×%2").arg(tier->name).arg(easyStreak));
    streakBadge->setStyleSheet(QStringLiteral(
                                       "QLabel#streakBadge {"
                                       "  color: %1;"
                                       "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %2, stop:1 %3);"
                                       "  border: 2px solid rgba(255,255,255,180);"
                                       "  border-radius: 18px;"
                                       "  padding: 8px 22px;"
                                       "  font-size: %4px;"
                                       "  font-weight: 800;"
                                       "}").arg(tier->textColor, tier->bg1, tier->bg2).arg(tier->fontPx));

    if (streakGlow) streakGlow->setColor(tier->glow);

    streakBadge->adjustSize();
    streakBadge->show();
    streakBadge->raise();
    repositionStreakBadge(); // 必须在 show() 之后，否则 isHidden() 守卫会让定位提前返回

    // 每次连胜增长都来一发光晕脉冲，作为升档/续命的即时正反馈特效。
    if (streakGlow && streakPulse) {
        const qreal base = wasVisible ? 24.0 : 16.0;
        streakPulse->stop();
        streakPulse->setStartValue(base);
        streakPulse->setKeyValueAt(0.5, 56.0);
        streakPulse->setEndValue(28.0);
        streakPulse->start();
    }
}

void MainWindow::repositionStreakBadge() {
    if (!streakBadge || !centerPanel || streakBadge->isHidden()) return;
    streakBadge->adjustSize();
    const int x = (centerPanel->width() - streakBadge->width()) / 2;
    const int y = 40; // 紧贴中央看板顶部，悬浮在闪卡上方
    streakBadge->move(qMax(0, x), y);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    repositionStreakBadge();
}

void MainWindow::showPreferencesDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("偏好设置"));
    dialog.setMinimumWidth(360);
    StyledDialogs::applyStyle(&dialog);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);

    auto *promptLabel = new QLabel(tr("外观主题"), &dialog);
    promptLabel->setObjectName("dialogPrompt");
    layout->addWidget(promptLabel);

    // 按注册表动态生成单选项
    std::vector<QRadioButton*> themeRadios;
    const auto& themes = Theme::availableThemes();
    themeRadios.reserve(themes.size());
    for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
        auto *radio = new QRadioButton(themes[i].displayName, &dialog);
        radio->setChecked(i == m_themeIndex);
        layout->addWidget(radio);
        themeRadios.push_back(radio);
    }

    // ==========================================
    // 【新增区】添加分割线与随机打乱复习选项
    // ==========================================
    auto *line = new QFrame(&dialog);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: rgba(150, 150, 150, 50);");
    layout->addWidget(line);

    QSettings settings("MindPalace", "Settings");
    auto *shuffleCheck = new QCheckBox(tr("随机打乱复习顺序"), &dialog);
    shuffleCheck->setChecked(settings.value("shuffleReview", false).toBool());
    layout->addWidget(shuffleCheck);
    // ==========================================

    auto *footer = new QHBoxLayout;
    footer->setSpacing(10);
    footer->addStretch();
    auto *cancelBtn = new QPushButton(tr("取消"), &dialog);
    cancelBtn->setObjectName("dialogSecondary");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *okBtn = new QPushButton(tr("确定"), &dialog);
    okBtn->setObjectName("dialogPrimary");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    footer->addWidget(cancelBtn);
    footer->addWidget(okBtn);
    layout->addLayout(footer);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() == QDialog::Accepted) {
        // 【新增区】用户点击确定后，保存打乱顺序的设置
        settings.setValue("shuffleReview", shuffleCheck->isChecked());

        // 处理主题切换
        int selectedIndex = m_themeIndex;
        for (int i = 0; i < static_cast<int>(themeRadios.size()); ++i) {
            if (themeRadios[i]->isChecked()) {
                selectedIndex = i;
                break;
            }
        }
        if (selectedIndex != m_themeIndex) {
            applyTheme(selectedIndex);
        }
    }
}

void MainWindow::applyTheme(int themeIndex) {
    // 防御性夹取，并落到注册表里真实存在的主题
    if (themeIndex < 0 || themeIndex >= Theme::themeCount()) {
        themeIndex = 0;
    }
    m_themeIndex = themeIndex;

    const Theme::ThemeDef& theme = Theme::themeAt(m_themeIndex);

    QSettings settings("MindPalace", "Settings");
    settings.setValue("themeKey", theme.key);          // 主存：稳定 key
    settings.setValue("themeMode", m_themeIndex);      // 兼容旧字段，便于回退读取

    // 1. 加载对应的 QSS
    setupStyles();

    // 2. 按主题特性决定卡片表面是磨砂玻璃 + 柔和阴影，还是扁平交还给 QSS
    const bool frosted = theme.frostedSurface;
    for (auto *frame : this->findChildren<QFrame*>()) {
        if (frame->property("role") == "surface") {
            if (frosted) {
                frame->setStyleSheet(QString("background-color: rgba(%1, %2, %3, %4); border: 1px solid rgba(255, 255, 255, 220); border-radius: 16px;")
                                             .arg(Theme::Surface.red()).arg(Theme::Surface.green()).arg(Theme::Surface.blue()).arg(Theme::Surface.alpha()));
                addSoftShadow(frame);
            } else {
                frame->setStyleSheet(""); // 让 QSS 重新接管
                frame->setGraphicsEffect(nullptr); // 剥离阴影
            }
        }
    }

    if (flashCardView) {
        flashCardView->setClearColor(Qt::white);
    }

    emit themeModeChanged();
}

bool MainWindow::frostedCard() const {
    return Theme::themeAt(m_themeIndex).frostedSurface;
}