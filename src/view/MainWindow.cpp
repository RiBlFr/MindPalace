#include "MainWindow.h"
#include <QCheckBox>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QQuickWidget>
#include <QQuickItem>
#include <QStackedWidget>
#include <QStyle>
#include <QTextBrowser>
#include <QUrl>
#include <QMetaObject>
#include <QSettings>
#include <QSizePolicy>
#include <QDialog>
#include <QDir>
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
#include <QTimer>
#include <QQmlContext>
#include <QQmlEngine>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

#include "StyleUtils.h"
#include "ThemeRegistry.h"
#include "CardManagerDialog.h"
#include "DeckPreviewDialog.h"
#include "DesktopPetWidget.h"
#include "StyledDialogs.h"
#include "ScheduleCalendarDialog.h"
#include "service/aiassistantservice.h"
#include "service/storagemanager.h"

#include <QSurfaceFormat>

namespace {
class CheckInSuccessIcon final : public QWidget {
public:
    explicit CheckInSuccessIcon(QWidget *parent = nullptr)
            : QWidget(parent) {
        setFixedSize(92, 92);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const bool dark = property("theme").toString() == QStringLiteral("dark");
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF ringRect(11, 11, width() - 22, height() - 22);
        const QColor accent = dark ? QColor("#55ffe0") : QColor("#62d99d");
        const QColor glow = dark ? QColor(85, 255, 224, 58) : QColor(98, 217, 157, 56);

        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(ringRect.adjusted(-9, -9, 9, 9));

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(accent, 5.5, Qt::SolidLine, Qt::RoundCap));
        painter.drawEllipse(ringRect);

        QPen checkPen(accent, 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(checkPen);
        QPainterPath checkPath;
        checkPath.moveTo(33, 48);
        checkPath.lineTo(44, 59);
        checkPath.lineTo(63, 37);
        painter.drawPath(checkPath);

        painter.setPen(QPen(dark ? QColor(85, 255, 224, 180) : QColor(255, 255, 255, 230), 2));
        painter.drawLine(QPointF(70, 15), QPointF(78, 7));
        painter.drawLine(QPointF(78, 15), QPointF(70, 7));
        if (!dark) {
            painter.drawLine(QPointF(14, 30), QPointF(20, 24));
            painter.drawLine(QPointF(20, 30), QPointF(14, 24));
        }
    }
};

#ifdef Q_OS_WIN
void applyWindowFrame(QWidget *widget, bool dark) {
    if (!widget) return;

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    const BOOL darkMode = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));

    const COLORREF caption = dark ? RGB(5, 9, 16) : RGB(247, 248, 251);
    const COLORREF text = dark ? RGB(232, 238, 248) : RGB(31, 41, 55);
    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
}
#else
void applyWindowFrame(QWidget*, bool) {}
#endif

bool syncReviewReminderStartup(bool enabled) {
#ifdef Q_OS_WIN
    QSettings runKey(
            "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            QSettings::NativeFormat);
    if (enabled) {
        const QString appPath = QDir::toNativeSeparators(QApplication::applicationFilePath());
        runKey.setValue("MindPalace", QString("\"%1\" --review-reminder-startup").arg(appPath));
    } else {
        runKey.remove("MindPalace");
    }
    runKey.sync();
    return runKey.status() == QSettings::NoError;
#else
    Q_UNUSED(enabled);
    return true;
#endif
}

void applyBusyDialogStyle(QProgressDialog& dialog) {
    // QProgressDialog does not inherit the app's dark theme cleanly on Windows;
    // pin the body text to a high-contrast palette so loading states stay readable.
    dialog.setStyleSheet(QStringLiteral(R"(
        QProgressDialog {
            background-color: #ffffff;
            color: #172033;
            border: 1px solid #cbd5e1;
            border-radius: 8px;
        }
        QProgressDialog QLabel {
            color: #172033;
            font-size: 14px;
            font-weight: 700;
        }
        QProgressBar {
            min-height: 8px;
            border: 1px solid #a8b3c2;
            border-radius: 4px;
            background-color: #eef2f7;
            text-align: center;
            color: #172033;
        }
        QProgressBar::chunk {
            border-radius: 4px;
            background-color: #2f7f95;
        }
    )"));
}
}

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent) {
    setWindowTitle(tr("记忆殿堂"));
    setWindowIcon(QIcon(":/icons/app_icon.png"));

    const QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    resize(screenGeometry.width() * 0.9, screenGeometry.height() * 0.85);

    // 1. 启动时读取用户的历史主题选择
    //    优先按稳定的 themeKey 还原；兼容旧版本仅存了 themeMode 整型索引的情况。
    QSettings settings("MindPalace", "Settings");
    syncReviewReminderStartup(settings.value("reviewReminderEnabled", false).toBool());
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
    connect(deckListWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item) emit signal_requestPreviewDeck(item->text());
    });

    // 绑定 4 个评分按钮
    const int qualityMapping[4] = {0, 3, 4, 5};
    for (int i = 0; i < 4; ++i) {
        connect(feedbackBtns[i], &QPushButton::clicked, this, [this, q = qualityMapping[i]]() {
            emit signal_requestSubmitFeedback(q);
        });
    }

    // F1-F4 only work while the answer-side feedback buttons are visible.
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

    this->setObjectName("mainWindowRoot");
    applyTheme(m_themeIndex);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    emit signal_appWillClose();
    event->accept();
}

void MainWindow::updateDeckListView(const std::vector<QString>& deckNames) const{
    // Preserve selection across list rebuilds so stats stay tied to the same deck.
    const QString previous =
            deckListWidget->currentItem() ? deckListWidget->currentItem()->text() : QString();

    {
        // Avoid review restarts while the list is temporarily empty.
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
        // Re-select once after unblocking signals to refresh stats.
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
    centralWidget->setObjectName("mainWindowRoot");

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

    auto *aiImportAction = fileMenu->addAction(QStringLiteral("AI导入"));
    connect(aiImportAction, &QAction::triggered, this, &MainWindow::showAiImportDialog);

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
    auto *aiAssistantAction = settingsMenu->addAction(QStringLiteral("AI助手"));
    connect(aiAssistantAction, &QAction::triggered, this, &MainWindow::showAiAssistantDialog);
    auto *reviewReminderAction = settingsMenu->addAction(tr("复习提醒"));
    connect(reviewReminderAction, &QAction::triggered, this, &MainWindow::showReviewReminderDialog);
    auto *desktopPetAction = settingsMenu->addAction(QStringLiteral("桌面宠物"));
    desktopPetAction->setText(QStringLiteral("桌面宠物"));
    connect(desktopPetAction, &QAction::triggered, this, &MainWindow::showDesktopPetDialog);

    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    auto *howToUseAction = helpMenu->addAction(tr("使用说明"));
    connect(howToUseAction, &QAction::triggered, this, [this]() {
        showMarkdownDialog(tr("使用说明"), QStringLiteral(":/docs/HowToUse.md"));
    });
    auto *aboutAction = helpMenu->addAction(tr("关于"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        showMarkdownDialog(tr("关于"), QStringLiteral(":/docs/AboutUs.md"));
    });
    checkInBtn = new QPushButton(QStringLiteral("签到"), menuBar());
    checkInBtn->setObjectName("btnCheckIn");
    checkInBtn->setCursor(Qt::PointingHandCursor);
    checkInBtn->setFixedSize(96, 32);
    setButtonFont(checkInBtn, 11);
    menuBar()->setMinimumHeight(checkInBtn->height() + 8);
    connect(checkInBtn, &QPushButton::clicked, this, [this]() {
        emit signal_requestCheckIn();
    });
    repositionCheckInButton();
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
    setButtonFont(addDeckBtn, 11);
    leftLayout->addWidget(addDeckBtn);

    leftLayout->addSpacing(16);

    calendarBtn = new QPushButton(tr("📅 复习日历"));
    calendarBtn->setObjectName("btnCalendar");
    setButtonFont(calendarBtn, 12);
    leftLayout->addWidget(calendarBtn);

    deleteDeckBtn = new QPushButton(tr("删除卡组"));
    deleteDeckBtn->setObjectName("btnDeleteDeck");
    setButtonFont(deleteDeckBtn, 11);
    deleteDeckBtn->setEnabled(false);
    leftLayout->addWidget(deleteDeckBtn);

    resetDeckBtn = new QPushButton(tr("重置卡组进度"));
    resetDeckBtn->setObjectName("btnResetDeck");
    setButtonFont(resetDeckBtn, 11);
    resetDeckBtn->setEnabled(false);
    leftLayout->addWidget(resetDeckBtn);

    normalizeActionButtonMetrics();

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
        connect(&dialog, &ScheduleCalendarDialog::signal_requestCheckInDates, this, &MainWindow::signal_requestCheckInDates);
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
    showAnswerBtn->setObjectName("btnShowAnswer");
    setButtonFont(showAnswerBtn, 13);

    feedbackRow = setupFeedbackButtons();

    buttonStack = new QStackedWidget;
    buttonStack->addWidget(showAnswerBtn);
    buttonStack->addWidget(feedbackRow);
    buttonStack->hide();
    centerLayout->addWidget(buttonStack);
    normalizeActionButtonMetrics();

    // 悬浮徽章必须最后创建：它不进布局，而是叠加在闪卡看板之上
    setupStreakBadge();
    setupCheckInToast();
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

void MainWindow::setupCheckInToast() {
    checkInToast = new QFrame(centerPanel);
    checkInToast->setObjectName("checkInToast");
    checkInToast->setFixedSize(560, 300);
    checkInToast->setAttribute(Qt::WA_TransparentForMouseEvents);
    checkInToast->hide();

    auto *layout = new QVBoxLayout(checkInToast);
    layout->setContentsMargins(34, 28, 34, 26);
    layout->setSpacing(10);
    layout->addStretch(1);

    checkInToastIcon = new CheckInSuccessIcon(checkInToast);
    layout->addWidget(checkInToastIcon, 0, Qt::AlignHCenter);

    checkInToastTitle = new QLabel(QStringLiteral("签到成功"), checkInToast);
    checkInToastTitle->setObjectName("checkInToastTitle");
    checkInToastTitle->setAlignment(Qt::AlignCenter);
    setLabelStyle(checkInToastTitle, 27, QFont::Black);
    layout->addWidget(checkInToastTitle);

    checkInToastSubtitle = new QLabel(QStringLiteral("今日签到已完成"), checkInToast);
    checkInToastSubtitle->setObjectName("checkInToastSubtitle");
    checkInToastSubtitle->setAlignment(Qt::AlignCenter);
    setLabelStyle(checkInToastSubtitle, 13, QFont::Medium);
    layout->addWidget(checkInToastSubtitle);

    checkInToastHint = new QLabel(QStringLiteral("弹窗将自动关闭"), checkInToast);
    checkInToastHint->setObjectName("checkInToastHint");
    checkInToastHint->setAlignment(Qt::AlignCenter);
    setLabelStyle(checkInToastHint, 10, QFont::Normal);
    layout->addSpacing(18);
    layout->addWidget(checkInToastHint);
    layout->addStretch(1);

    checkInToastOpacity = new QGraphicsOpacityEffect(checkInToast);
    checkInToastOpacity->setOpacity(0.0);
    checkInToast->setGraphicsEffect(checkInToastOpacity);

    checkInToastFade = new QPropertyAnimation(checkInToastOpacity, "opacity", this);
    checkInToastFade->setEasingCurve(QEasingCurve::OutCubic);
    connect(checkInToastFade, &QPropertyAnimation::finished, this, [this]() {
        if (checkInToastOpacity && qFuzzyIsNull(checkInToastOpacity->opacity()) && checkInToast) {
            checkInToast->hide();
        }
    });

    checkInToastMove = new QPropertyAnimation(checkInToast, "geometry", this);
    checkInToastMove->setEasingCurve(QEasingCurve::OutCubic);

    styleCheckInToast();
    repositionCheckInToast();
}

void MainWindow::styleCheckInToast() {
    if (!checkInToast) return;

    const bool dark = frostedCard();
    checkInToast->setProperty("theme", dark ? "dark" : "classic");
    if (checkInToastIcon) {
        checkInToastIcon->setProperty("theme", dark ? "dark" : "classic");
        checkInToastIcon->update();
    }

    if (dark) {
        checkInToast->setStyleSheet(QStringLiteral(
                "QFrame#checkInToast {"
                "  background-color: rgba(8, 19, 34, 225);"
                "  border: 2px solid rgba(125, 210, 255, 225);"
                "  border-radius: 18px;"
                "}"
                "QLabel#checkInToastTitle { color: #f3fbff; font-size: 38px; font-weight: 900; }"
                "QLabel#checkInToastSubtitle { color: #d5e7ff; font-size: 20px; font-weight: 500; }"
                "QLabel#checkInToastHint { color: rgba(177, 204, 235, 0); font-size: 14px; }"));
    } else {
        checkInToast->setStyleSheet(QStringLiteral(
                "QFrame#checkInToast {"
                "  background-color: rgba(246, 252, 255, 238);"
                "  border: 1px solid rgba(198, 223, 241, 210);"
                "  border-radius: 18px;"
                "}"
                "QLabel#checkInToastTitle { color: #13233c; font-size: 38px; font-weight: 900; }"
                "QLabel#checkInToastSubtitle { color: #3c506d; font-size: 20px; font-weight: 500; }"
                "QLabel#checkInToastHint { color: #6d7f96; font-size: 14px; }"));
    }
}

void MainWindow::repositionCheckInToast() {
    if (!checkInToast || !centerPanel) return;
    const int x = (centerPanel->width() - checkInToast->width()) / 2;
    const int y = (centerPanel->height() - checkInToast->height()) / 2 - 8;
    checkInToast->move(qMax(0, x), qMax(0, y));
}

void MainWindow::showCheckInSuccessToast() {
    if (!checkInToast || !checkInToastOpacity || !checkInToastFade || !checkInToastMove) return;

    styleCheckInToast();
    repositionCheckInToast();

    const QRect endGeometry = checkInToast->geometry();
    const QRect startGeometry = endGeometry.translated(0, 24);

    checkInToastFade->stop();
    checkInToastMove->stop();
    checkInToastOpacity->setOpacity(0.0);
    checkInToast->setGeometry(startGeometry);
    checkInToast->show();
    checkInToast->raise();

    checkInToastFade->setDuration(170);
    checkInToastFade->setStartValue(0.0);
    checkInToastFade->setEndValue(1.0);
    checkInToastFade->start();

    checkInToastMove->setDuration(220);
    checkInToastMove->setStartValue(startGeometry);
    checkInToastMove->setEndValue(endGeometry);
    checkInToastMove->start();

    QTimer::singleShot(1000, this, [this, endGeometry]() {
        if (!checkInToast || !checkInToast->isVisible()) return;
        checkInToastFade->stop();
        checkInToastMove->stop();
        checkInToastFade->setDuration(180);
        checkInToastFade->setStartValue(checkInToastOpacity->opacity());
        checkInToastFade->setEndValue(0.0);
        checkInToastFade->start();

        checkInToastMove->setDuration(180);
        checkInToastMove->setStartValue(checkInToast->geometry());
        checkInToastMove->setEndValue(endGeometry.translated(0, -10));
        checkInToastMove->start();
    });
}

void MainWindow::initFlashCardView() {
    flashCardView = new QQuickWidget(centerPanel);
    flashCardView->setObjectName("flashCardView");

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    flashCardView->setFormat(format);

    flashCardView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    // Transparent clear color lets the themed container show through behind QML.
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
    const QStringList objNames = { "btnForget", "btnHard", "btnNormal", "btnEasy" };

    for (int i = 0; i < 4; ++i) {
        feedbackBtns[i] = new QPushButton(feedbackTexts[i]);
        feedbackBtns[i]->setObjectName(objNames[i]);
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

void MainWindow::showDeckPreviewDialog(const QString& deckName, const std::vector<CardDisplayInfo>& cards) {
    DeckPreviewDialog dialog(deckName, cards, frostedCard(), this);
    if (dialog.exec() == QDialog::Accepted) {
        emit signal_requestStartReview(deckName);
    }
}

void MainWindow::setupStyles() {
    // ThemeRegistry owns the active QSS path.
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

void MainWindow::setTodayCheckInState(bool checkedIn) {
    if (!checkInBtn) return;

    checkInBtn->setText(checkedIn ? QStringLiteral("已签到") : QStringLiteral("签到"));
    checkInBtn->setEnabled(!checkedIn);
    checkInBtn->setProperty("checkedIn", checkedIn);
    checkInBtn->style()->unpolish(checkInBtn);
    checkInBtn->style()->polish(checkInBtn);
    checkInBtn->update();
    repositionCheckInButton();
}

void MainWindow::normalizeActionButtonMetrics() {
    auto pinHeight = [](QPushButton *button, int height) {
        if (!button) return;
        button->setMinimumHeight(height);
        button->setMaximumHeight(height);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->updateGeometry();
    };

    pinHeight(addDeckBtn, 56);
    pinHeight(calendarBtn, 60);
    pinHeight(deleteDeckBtn, 56);
    pinHeight(resetDeckBtn, 56);
    pinHeight(showAnswerBtn, 62);

    for (auto *button : feedbackBtns) {
        pinHeight(button, 64);
    }

    if (feedbackRow) {
        feedbackRow->setMinimumHeight(64);
        feedbackRow->setMaximumHeight(64);
        feedbackRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        feedbackRow->updateGeometry();
    }
    if (buttonStack) {
        buttonStack->setMinimumHeight(64);
        buttonStack->setMaximumHeight(64);
        buttonStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        buttonStack->updateGeometry();
    }
    if (checkInBtn) {
        checkInBtn->setFixedSize(96, 32);
        checkInBtn->updateGeometry();
    }
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

void MainWindow::repositionCheckInButton() {
    if (!checkInBtn || !menuBar()) return;

    const int x = (menuBar()->width() - checkInBtn->width()) / 2;
    const int y = (menuBar()->height() - checkInBtn->height()) / 2;
    checkInBtn->move(qMax(0, x), qMax(0, y));
    checkInBtn->raise();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    repositionStreakBadge();
    repositionCheckInToast();
    repositionCheckInButton();
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

    auto *line = new QFrame(&dialog);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: rgba(150, 150, 150, 50);");
    layout->addWidget(line);

    QSettings settings("MindPalace", "Settings");
    auto *shuffleCheck = new QCheckBox(tr("随机打乱复习顺序"), &dialog);
    shuffleCheck->setChecked(settings.value("shuffleReview", false).toBool());
    layout->addWidget(shuffleCheck);
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

void MainWindow::showAiAssistantDialog() {
    using MindPalace::Service::AiAssistantService;
    using MindPalace::Service::AiAssistantSettings;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("AI助手"));
    dialog.setMinimumWidth(520);
    StyledDialogs::applyStyle(&dialog);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);

    auto *promptLabel = new QLabel(QStringLiteral("配置用于 AI 导入 Markdown 文档的模型服务。API 密钥只保存在本机设置中。"), &dialog);
    promptLabel->setObjectName("dialogPrompt");
    promptLabel->setWordWrap(true);
    layout->addWidget(promptLabel);

    AiAssistantSettings saved = AiAssistantService::loadSettings();

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    auto *providerCombo = new QComboBox(&dialog);
    providerCombo->setObjectName("dialogInput");
    providerCombo->addItem(QStringLiteral("OpenAI"), QStringLiteral("openai-compatible"));
    providerCombo->addItem(QStringLiteral("DeepSeek"), QStringLiteral("deepseek"));
    providerCombo->addItem(QStringLiteral("Gemini"), QStringLiteral("gemini"));
    providerCombo->addItem(QStringLiteral("OpenAI兼容接口"), QStringLiteral("openai-compatible"));
    const int providerIndex = providerCombo->findData(saved.provider);
    providerCombo->setCurrentIndex(providerIndex >= 0 ? providerIndex : 0);

    auto *apiKeyEdit = new QLineEdit(&dialog);
    apiKeyEdit->setObjectName("dialogInput");
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiKeyEdit->setPlaceholderText(QStringLiteral("sk-... / AI Studio key"));
    apiKeyEdit->setText(saved.apiKey);

    auto *baseUrlEdit = new QLineEdit(&dialog);
    baseUrlEdit->setObjectName("dialogInput");
    baseUrlEdit->setText(saved.baseUrl);

    auto *modelEdit = new QLineEdit(&dialog);
    modelEdit->setObjectName("dialogInput");
    modelEdit->setText(saved.model);

    form->addRow(QStringLiteral("服务商"), providerCombo);
    form->addRow(QStringLiteral("API密钥"), apiKeyEdit);
    form->addRow(QStringLiteral("Base URL"), baseUrlEdit);
    form->addRow(QStringLiteral("模型"), modelEdit);
    layout->addLayout(form);

    auto collectSettings = [&]() {
        AiAssistantSettings settings;
        settings.provider = providerCombo->currentData().toString();
        settings.apiKey = apiKeyEdit->text().trimmed();
        settings.baseUrl = baseUrlEdit->text().trimmed();
        settings.model = modelEdit->text().trimmed();
        return settings;
    };

    connect(providerCombo, &QComboBox::currentIndexChanged, this, [providerCombo, baseUrlEdit, modelEdit]() {
        const QString provider = providerCombo->currentData().toString();
        baseUrlEdit->setText(MindPalace::Service::AiAssistantService::defaultBaseUrlForProvider(provider));
        modelEdit->setText(MindPalace::Service::AiAssistantService::defaultModelForProvider(provider));
    });

    auto *footer = new QHBoxLayout;
    footer->setSpacing(10);
    auto *testBtn = new QPushButton(QStringLiteral("测试连接"), &dialog);
    testBtn->setObjectName("dialogSecondary");
    testBtn->setCursor(Qt::PointingHandCursor);
    footer->addWidget(testBtn);
    footer->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), &dialog);
    cancelBtn->setObjectName("dialogSecondary");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *saveBtn = new QPushButton(QStringLiteral("保存"), &dialog);
    saveBtn->setObjectName("dialogPrimary");
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setDefault(true);
    footer->addWidget(cancelBtn);
    footer->addWidget(saveBtn);
    layout->addLayout(footer);

    auto testConnection = [&]() {
        const AiAssistantSettings settings = collectSettings();
        QString errorMessage;
        QProgressDialog progress(QStringLiteral("正在测试 API 密钥..."), QString(), 0, 0, &dialog);
        progress.setWindowModality(Qt::WindowModal);
        progress.setCancelButton(nullptr);
        progress.setMinimumDuration(0);
        applyBusyDialogStyle(progress);
        progress.show();
        QApplication::processEvents();
        const bool ok = AiAssistantService::testConnection(settings, &errorMessage);
        progress.close();
        if (!ok) {
            StyledDialogs::info(&dialog, QStringLiteral("测试失败"), errorMessage);
            return false;
        }
        StyledDialogs::info(&dialog, QStringLiteral("测试成功"), QStringLiteral("API 密钥和模型配置可用。"));
        return true;
    };

    connect(testBtn, &QPushButton::clicked, &dialog, [&]() {
        testConnection();
    });
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, &dialog, [&]() {
        const AiAssistantSettings settings = collectSettings();
        QString errorMessage;
        if (!AiAssistantService::validateSettings(settings, &errorMessage)) {
            StyledDialogs::info(&dialog, QStringLiteral("配置不完整"), errorMessage);
            return;
        }
        if (!testConnection()) {
            return;
        }
        AiAssistantService::saveSettings(settings);
        dialog.accept();
    });

    dialog.exec();
}

void MainWindow::showAiImportDialog() {
    using MindPalace::Service::AiAssistantService;
    using MindPalace::Service::AiAssistantSettings;
    using MindPalace::Service::AiImportResult;

    // Keep the expensive AI path behind a verified configuration; otherwise
    // users would only discover a bad key after selecting a document.
    AiAssistantSettings settings = AiAssistantService::loadSettings();
    QString settingsError;
    if (!AiAssistantService::validateSettings(settings, &settingsError)) {
        StyledDialogs::info(this,
                            QStringLiteral("需要配置 AI 助手"),
                            QStringLiteral("请先在“设置 > AI助手”中填写并测试 API 密钥。"));
        showAiAssistantDialog();
        settings = AiAssistantService::loadSettings();
        if (!AiAssistantService::validateSettings(settings, &settingsError)) {
            return;
        }
    }

    // The extractor still has broader legacy support, but the product copy now
    // guides users to Markdown because it produces the most reliable cards.
    const QString filePath = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("选择要 AI 导入的 Markdown 文档"),
            {},
            QStringLiteral("Markdown 文档（AI助手当前仅支持） (*.md *.markdown);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    AiImportResult result;
    QString errorMessage;
    QProgressDialog progress(QStringLiteral("AI 正在阅读 Markdown 并生成卡片..."), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    applyBusyDialogStyle(progress);
    progress.show();
    QApplication::processEvents();

    // Generation returns an editable draft. Nothing is written to disk until
    // the user reviews the preview dialog and presses "创建".
    const bool ok = AiAssistantService::generateDeckFromDocument(filePath, settings, &result, &errorMessage);
    progress.close();
    if (!ok) {
        StyledDialogs::info(this, QStringLiteral("AI导入失败"), errorMessage);
        return;
    }

    DeckPreviewDialog::ImportMetadata metadata;
    metadata.sourceFileName = result.sourceFileName;
    metadata.summary = result.summary;

    DeckPreviewDialog dialog(result.deckName,
                             result.cards,
                             frostedCard(),
                             DeckPreviewDialog::Mode::AiImportPreview,
                             metadata,
                             this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    emit signal_requestCreateDeckFromCards(dialog.deckName(), dialog.cards());
}

void MainWindow::showReviewReminderDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("复习提醒"));
    dialog.setMinimumWidth(420);
    StyledDialogs::applyStyle(&dialog);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);

    auto *promptLabel = new QLabel(tr("复习提醒"), &dialog);
    promptLabel->setObjectName("dialogPrompt");
    layout->addWidget(promptLabel);

    auto *messageLabel = new QLabel(
            tr("开启后，程序会在 Windows 每天首次登录时检查复习日历；即使不打开主界面，也会提醒今日需要复习的牌组和卡片数量。"),
            &dialog);
    messageLabel->setObjectName("dialogMessage");
    messageLabel->setWordWrap(true);
    layout->addWidget(messageLabel);

    QSettings settings("MindPalace", "Settings");
    auto *enabledCheck = new QCheckBox(tr("开启复习提醒"), &dialog);
    enabledCheck->setChecked(settings.value("reviewReminderEnabled", false).toBool());
    layout->addWidget(enabledCheck);

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
        const bool enabled = enabledCheck->isChecked();
        if (syncReviewReminderStartup(enabled)) {
            settings.setValue("reviewReminderEnabled", enabled);
        } else {
            StyledDialogs::info(
                    this,
                    tr("复习提醒"),
                    tr("无法更新 Windows 登录启动项，请检查系统权限或安全软件拦截。"));
        }
    }
}

void MainWindow::showDesktopPetDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("桌面宠物"));
    dialog.setMinimumWidth(420);
    StyledDialogs::applyStyle(&dialog);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);

    auto *promptLabel = new QLabel(QStringLiteral("桌面宠物"), &dialog);
    promptLabel->setObjectName("dialogPrompt");
    layout->addWidget(promptLabel);

    auto *messageLabel = new QLabel(
            QStringLiteral("开启后，桌面宠物会常驻桌面并随 Windows 登录启动；取消勾选即可关闭桌宠并移除自启动。"),
            &dialog);
    messageLabel->setObjectName("dialogMessage");
    messageLabel->setWordWrap(true);
    layout->addWidget(messageLabel);

    auto *enabledCheck = new QCheckBox(QStringLiteral("开启桌面宠物"), &dialog);
    enabledCheck->setChecked(MindPalace::DesktopPet::isEnabled());
    layout->addWidget(enabledCheck);

    auto *footer = new QHBoxLayout;
    footer->setSpacing(10);
    footer->addStretch();
    auto *cancelBtn = new QPushButton(tr("取消"), &dialog);
    cancelBtn->setObjectName("dialogSecondary");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *okBtn = new QPushButton(QStringLiteral("保存"), &dialog);
    okBtn->setObjectName("dialogPrimary");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    footer->addWidget(cancelBtn);
    footer->addWidget(okBtn);
    layout->addLayout(footer);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() == QDialog::Accepted) {
        MindPalace::DesktopPet::setEnabled(enabledCheck->isChecked());
    }
}

void MainWindow::applyTheme(int themeIndex) {
    // 防御性夹取，并落到注册表里真实存在的主题
    if (themeIndex < 0 || themeIndex >= Theme::themeCount()) {
        themeIndex = 0;
    }
    m_themeIndex = themeIndex;

    const Theme::ThemeDef& theme = Theme::themeAt(m_themeIndex);
    applyWindowFrame(this, theme.frostedSurface);

    QSettings settings("MindPalace", "Settings");
    settings.setValue("themeKey", theme.key);          // 主存：稳定 key
    settings.setValue("themeMode", m_themeIndex);      // 兼容旧字段，便于回退读取

    // 1. 加载对应的 QSS
    setupStyles();
    normalizeActionButtonMetrics();
    styleCheckInToast();
    if (checkInBtn) {
        menuBar()->setMinimumHeight(checkInBtn->height() + 8);
        repositionCheckInButton();
    }

    // 2. 按主题特性决定卡片表面是磨砂玻璃 + 柔和阴影，还是扁平交还给 QSS
    const bool frosted = theme.frostedSurface;
    for (auto *frame : this->findChildren<QFrame*>()) {
        if (frame->property("role") == "surface") {
            if (frosted) {
                frame->setStyleSheet(QString("background-color: rgba(%1, %2, %3, %4); border: 1px solid rgba(72, 96, 126, 150); border-radius: 12px;")
                                             .arg(Theme::Surface.red()).arg(Theme::Surface.green()).arg(Theme::Surface.blue()).arg(Theme::Surface.alpha()));
                addSoftShadow(frame);
            } else {
                frame->setStyleSheet(""); // 让 QSS 重新接管
                frame->setGraphicsEffect(nullptr); // 剥离阴影
            }
        }
    }

    if (flashCardView) {
        flashCardView->setClearColor(frosted ? Qt::transparent : Qt::white);
        flashCardView->setAttribute(Qt::WA_TranslucentBackground, frosted);
        flashCardView->setAttribute(Qt::WA_OpaquePaintEvent, !frosted);
    }

    emit themeModeChanged();
}

bool MainWindow::frostedCard() const {
    return Theme::themeAt(m_themeIndex).frostedSurface;
}
