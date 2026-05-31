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
#include <QInputDialog>

#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>

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
        bool hasSelection = !text.isEmpty();
        resetDeckBtn->setEnabled(hasSelection);
        addCardBtn->setEnabled(hasSelection); // 【新增这一句】同步激活/禁用新增卡片按钮

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
    showAnswerBtn->show();
    feedbackRow->hide();
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
    showAnswerBtn->hide();
    feedbackRow->show();
}

void MainWindow::showFinishedSummaryPage() {
    if (auto *root = flashCardView->rootObject()) {
        root->setProperty("hasNextCard", false);
        root->setProperty("questionText", tr("今日复习全部完成！"));
        root->setProperty("answerText", tr("所有卡片已复习完毕，明日再来~"));
    }
    showAnswerBtn->hide();
    feedbackRow->hide();
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

    leftLayout->addWidget(deckListWidget, 1);

    addDeckBtn = new QPushButton(tr("+ 新增卡组"));
    addDeckBtn->setProperty("variant", "primary");
    addDeckBtn->setMinimumHeight(40);
    setButtonFont(addDeckBtn, 11);
    leftLayout->addWidget(addDeckBtn);

    resetDeckBtn = new QPushButton(tr("重置卡组进度"));
    resetDeckBtn->setProperty("variant", "warning");
    resetDeckBtn->setMinimumHeight(40);
    setButtonFont(resetDeckBtn, 11);
    resetDeckBtn->setEnabled(false);
    leftLayout->addWidget(resetDeckBtn);

    connect(addDeckBtn, &QPushButton::clicked, this, [this]() {
    bool ok;
    QString newDeckName = QInputDialog::getText(this, "新建牌组",
                                         "请输入新牌组的名称:", QLineEdit::Normal,
                                         "", &ok);
    // 如果用户点了确定，且名字不为空，就把信号发射给 AppController
    if (ok && !newDeckName.trimmed().isEmpty()) {
        emit signal_requestCreateDeck(newDeckName.trimmed());
    }
});
    // ==========================================
    // 【新增】实例化"新增卡片"按钮
    // ==========================================
    addCardBtn = new QPushButton(tr("+ 新增卡片"));
    addCardBtn->setProperty("variant", "primary"); // 保持与新增卡组统一的主色调
    addCardBtn->setMinimumHeight(40);
    setButtonFont(addCardBtn, 11);
    addCardBtn->setEnabled(false); // 默认禁用，必须先选中左侧某个卡组才能添加卡片
    leftLayout->addWidget(addCardBtn);

    // 绑定点击事件：弹出一个带有两个输入框的自定义小窗口
    connect(addCardBtn, &QPushButton::clicked, this, [this]() {
        // 安全检查：确认当前确实有选中的牌组
        auto *currentItem = deckListWidget->currentItem();
        if (!currentItem) return;
        QString currentDeckName = currentItem->text();

        // 临时构建一个弹窗 (Dialog)
        QDialog dialog(this);
        dialog.setWindowTitle(tr("新增卡片 - ") + currentDeckName);
        dialog.setMinimumWidth(300);

        auto *formLayout = new QFormLayout(&dialog);

        // 创建正反面输入框
        auto *frontEdit = new QLineEdit(&dialog);
        auto *backEdit = new QLineEdit(&dialog);
        formLayout->addRow(tr("正面 (问题):"), frontEdit);
        formLayout->addRow(tr("背面 (答案):"), backEdit);

        // 创建底部的确认和取消按钮
        auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        formLayout->addRow(btnBox);

        connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        // 阻塞等待用户操作。如果用户点击了"确定"：
        if (dialog.exec() == QDialog::Accepted) {
            QString frontText = frontEdit->text().trimmed();
            QString backText = backEdit->text().trimmed();

            // 确保两面都填了内容再发信号
            if (!frontText.isEmpty() && !backText.isEmpty()) {
                emit signal_requestAddCard(currentDeckName, frontText, backText);
            }
        }
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

    // 问题态：大的"显示答案"按钮，与评分行互斥显示
    showAnswerBtn = new QPushButton(tr("显示答案  （空格键）"));
    showAnswerBtn->setProperty("variant", "primary");
    showAnswerBtn->setMinimumHeight(62);
    setButtonFont(showAnswerBtn, 13);
    showAnswerBtn->hide();
    centerLayout->addWidget(showAnswerBtn);

    // 答案态：4 个评分按钮，包在一个容器里方便整体 show/hide
    feedbackRow = setupFeedbackButtons();
    feedbackRow->hide();
    centerLayout->addWidget(feedbackRow);
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

void MainWindow::setupStyles() {
    setBackground(this, Theme::WindowBg);

    QFile qssFile(":/styles/MainWindow.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to load QSS resource" << qssFile.fileName() << qssFile.errorString();
        return;
    }

    setStyleSheet(QString::fromUtf8(qssFile.readAll()));
}