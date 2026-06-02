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

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent) {
    setWindowTitle(tr("记忆殿堂"));
    setWindowIcon(QIcon(":/icons/app_icon.png"));

    const QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    resize(screenGeometry.width() * 0.9, screenGeometry.height() * 0.85);

    initUI();

    // 1. 左侧牌组列表点击 → 开始复习 + 激活重置与新增卡片按钮
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

    addDeckBtn = new QPushButton(tr("+ 新增卡组"));
    addDeckBtn->setProperty("variant", "primary");
    addDeckBtn->setMinimumHeight(40);
    setButtonFont(addDeckBtn, 11);
    leftLayout->addWidget(addDeckBtn);

    deleteDeckBtn = new QPushButton(tr("删除卡组"));
    deleteDeckBtn->setProperty("variant", "danger");
    deleteDeckBtn->setMinimumHeight(40);
    setButtonFont(deleteDeckBtn, 11);
    deleteDeckBtn->setEnabled(false);
    leftLayout->addWidget(deleteDeckBtn);

    resetDeckBtn = new QPushButton(tr("重置卡组进度"));
    resetDeckBtn->setProperty("variant", "warning");
    resetDeckBtn->setMinimumHeight(40);
    setButtonFont(resetDeckBtn, 11);
    resetDeckBtn->setEnabled(false);
    leftLayout->addWidget(resetDeckBtn);

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
}

void MainWindow::showCardManagerDialog(const QString& deckName,
                                       const std::vector<CardDisplayInfo>& cards) {
    CardManagerDialog dialog(deckName, cards, this);
    // 把对话框内部的增删请求转发为 MainWindow 已有的信号，AppController 不感知 dialog 类型
    connect(&dialog, &CardManagerDialog::signal_requestDeleteCard,
            this, &MainWindow::signal_requestDeleteCard);
    connect(&dialog, &CardManagerDialog::signal_requestAddCard,
            this, &MainWindow::signal_requestAddCard);
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