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
#include <QSettings>
#include <QDialog>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QCloseEvent>
#include <QQmlContext>
#include <QQmlEngine>

#include "StyleUtils.h"
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
    // ==========================================
    QSettings settings("MindPalace", "Settings");
    m_themeMode = static_cast<ThemeMode>(settings.value("themeMode", static_cast<int>(ThemeMode::Aurora)).toInt());

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
    applyTheme(m_themeMode);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    emit signal_appWillClose();
    event->accept();
}

void MainWindow::updateDeckListView(const std::vector<QString>& deckNames) const{
    deckListWidget->clear();
    for (const auto& name : deckNames) {
        deckListWidget->addItem(name);
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
        else QMessageBox::information(this, tr("提示"), tr("请先在左侧选择一个卡组"));
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

    QMenu *editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    auto *prefAction = editMenu->addAction(tr("偏好设置"));
    connect(prefAction, &QAction::triggered, this, &MainWindow::showPreferencesDialog);

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
}

void MainWindow::initFlashCardView() {
    flashCardView = new QQuickWidget(centerPanel);

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    flashCardView->setFormat(format);

    flashCardView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    flashCardView->setClearColor(Qt::transparent);
    flashCardView->setAttribute(Qt::WA_TranslucentBackground, true);
    flashCardView->setAttribute(Qt::WA_OpaquePaintEvent, false);

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
    // 动态根据枚举加载对应的 QSS，再也不会迷路了！
    QString qssPath = (m_themeMode == ThemeMode::Classic) ? ":/styles/theme_classic.qss" : ":/styles/theme_aurora.qss";
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

void MainWindow::showPreferencesDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("偏好设置"));
    dialog.resize(320, 160);

    auto *layout = new QVBoxLayout(&dialog);
    auto *group = new QGroupBox(tr("外观主题"), &dialog);
    auto *groupLayout = new QVBoxLayout(group);

    auto *radioClassic = new QRadioButton(tr("经典扁平 (Classic)"), group);
    auto *radioAurora = new QRadioButton(tr("极光拟态 (Aurora)"), group);

    if (m_themeMode == ThemeMode::Classic) radioClassic->setChecked(true);
    else radioAurora->setChecked(true);

    groupLayout->addWidget(radioClassic);
    groupLayout->addWidget(radioAurora);
    layout->addWidget(group);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        ThemeMode selectedMode = radioClassic->isChecked() ? ThemeMode::Classic : ThemeMode::Aurora;
        if (selectedMode != m_themeMode) {
            applyTheme(selectedMode);
        }
    }
}

void MainWindow::applyTheme(ThemeMode mode) {
    m_themeMode = mode;

    QSettings settings("MindPalace", "Settings");
    settings.setValue("themeMode", static_cast<int>(m_themeMode));

    // 1. 加载对应的 QSS
    setupStyles();

    // 2. 动态清理或附加边框阴影效果
    bool isAurora = (mode == ThemeMode::Aurora);
    for (auto *frame : this->findChildren<QFrame*>()) {
        if (frame->property("role") == "surface") {
            if (isAurora) {
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
        flashCardView->setClearColor(Qt::transparent);
    }

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