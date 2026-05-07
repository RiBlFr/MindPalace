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

    flashCardView = new QQuickWidget(centerPanel);
    flashCardView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    flashCardView->setClearColor(Qt::transparent);
    flashCardView->setMinimumHeight(420);
    QJsonArray cards;

    auto appendCard = [&cards](const QString &front, const QString &back) {
        QJsonObject card;
        card.insert(QStringLiteral("front"), front);
        card.insert(QStringLiteral("back"), back);
        cards.append(card);
    };

    appendCard(
            tr("C++ RAII 是什么？"),
            tr("Resource Acquisition Is Initialization：把资源生命周期绑定到对象生命周期，构造获取，析构释放。")
    );

    appendCard(
            tr("SM-2 算法中的 EF 代表什么？"),
            tr("Easiness Factor，表示卡片对学习者的难易权重，会影响下一次复习间隔。")
    );

    appendCard(
            tr("Qt 信号槽适合解决什么问题？"),
            tr("用于对象之间的松耦合通信，发送方不需要知道接收方的具体实现。")
    );

    appendCard(
            tr("被动视图 Passive View 的核心思想是什么？"),
            tr("View 只负责展示与上报事件，业务状态和流程由 Controller / Presenter 管理。")
    );

    flashCardCount = cards.size();
    currentFlashCardIndex = 0;

    const QString cardsJson = QString::fromUtf8(
            QJsonDocument(cards).toJson(QJsonDocument::Compact)
    );

    qWarning() << "C++ cardsJson =" << cardsJson;

    flashCardView->rootContext()->setContextProperty(QStringLiteral("cardsJson"), cardsJson);
    flashCardView->setSource(QUrl(QStringLiteral("qrc:/styles/FlashCardStack.qml")));

    if (auto *root = flashCardView->rootObject()) {
        qWarning() << "QML root loaded:" << root << root->metaObject()->className();

        // 把 cardsJson 写入 QML 内部属性，并主动触发一次 reload。
        // 注意：QML 里不能声明和 context property 同名的属性，否则会遮蔽 context property。
        root->setProperty("cardsJsonInternal", cardsJson);
        QMetaObject::invokeMethod(root, "loadCardsFromJson", Qt::DirectConnection);

        root->setProperty("currentIndex", currentFlashCardIndex);
        root->setProperty("requestedIndex", currentFlashCardIndex);
        root->setProperty("nextRequest", 0);
        root->setProperty("flipped", false);

        qWarning() << "cardCount after load =" << root->property("cardCount").toInt();
    } else {
        qWarning() << "FlashCardStack rootObject is null";
    }



    centerLayout->addWidget(flashCardView, 1);

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

        connect(feedbackBtns[i], &QPushButton::clicked,
                this, &MainWindow::showNextFlashCard);

        feedbackLayout->addWidget(feedbackBtns[i]);
    }


    centerLayout->addLayout(feedbackLayout);
}

void MainWindow::showNextFlashCard() {
    if (!flashCardView) {
        return;
    }

    QObject *root = flashCardView->rootObject();
    if (!root) {
        qWarning() << "Flash card QML root object is null.";
        return;
    }

    if (root->property("switching").toBool()) {
        return;
    }

    const int currentIndex = root->property("currentIndex").toInt();
    const int cardCount = root->property("cardCount").toInt();

//    if (cardCount <= 1) {
//        qWarning() << "Card count is too small:" << cardCount;
//        return;
//    }

    const int nextIndex = currentIndex + 1;
    qWarning() << "Switch flash card:" << currentIndex << "->" << nextIndex;

    root->setProperty("requestedIndex", nextIndex);
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
