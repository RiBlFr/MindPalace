//
// Created by Arian on 2026/5/25.
//

#include "appcontroller.h"
#include "view/MainWindow.h"
#include "view/DesktopPetWidget.h"
#include "view/StyledDialogs.h"
#include "deckcontroller.h"
#include "reviewcontroller.h"
#include "service/storagemanager.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>

namespace MindPalace::Controller {

// Lifecycle

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    qDebug() << "AppController: initializing...";

    // Controllers must exist before the view starts wiring signals to them.
    initializeControllers();
    initializeViews();
    setupGlobalConnections();

    qDebug() << "AppController: ready.";
}

/**
 * Keep the destructor out-of-line because this class owns unique_ptrs to
 * forward-declared types.
 */
AppController::~AppController() = default;

void AppController::start() {
    qDebug() << "AppController: showing main window...";
    DesktopPet::ensureRunningIfEnabled();
    if (m_mainWindow) {
        m_mainWindow->show();
    }
}

bool AppController::showStartupReviewReminder() {
    return maybeShowReviewReminder(nullptr);
}

// Startup wiring

void AppController::initializeControllers() {
    // 优先用可执行文件所在目录（部署和 IDE 运行均适用）；
    // 若不存在则回退到当前工作目录（方便从项目根目录直接运行时调试）
    const QString relPath = QStringLiteral("data/decks");
    const QString appDirPath = QCoreApplication::applicationDirPath() + "/" + relPath;
    m_decksDirPath = QDir(appDirPath).exists() ? appDirPath : relPath;

    qDebug() << "AppController: 牌组目录 ->" << m_decksDirPath;

    m_deckController = std::make_unique<DeckController>(m_decksDirPath);
    m_reviewController = std::make_unique<ReviewController>();
}

void AppController::initializeViews() {
    // 实例化最外层的主窗口视图
    m_mainWindow = std::make_unique<MainWindow>();
}

// Signal wiring
void AppController::setupGlobalConnections() {
    if (!m_mainWindow || !m_deckController || !m_reviewController) return;

    // View -> controller routes.
    connect(m_mainWindow.get(), &MainWindow::signal_requestStartReview,
            this, &AppController::handleStartReview);

    connect(m_mainWindow.get(), &MainWindow::signal_requestSubmitFeedback,
            this, &AppController::handleSubmitFeedback);

    connect(m_mainWindow.get(), &MainWindow::signal_appWillClose,
            this, &AppController::handleAppQuit);

    // 用户点击"重置卡组进度"按钮
    connect(m_mainWindow.get(), &MainWindow::signal_requestResetDeck,
            this, &AppController::handleResetDeck);


    // 用户将卡片翻面 → 驱动 ReviewController 进入答案态（state = AnswerState）
    connect(m_mainWindow.get(), &MainWindow::signal_requestShowAnswer,
            this, [this]() {
                m_reviewController->showAnswer();
            });

    // ReviewController 进入提问态 → 刷新卡片正面 + 预加载背面 + 更新进度
    connect(m_reviewController.get(), &ReviewController::signal_showQuestion,
            this, [this](const QString& frontText) {
                const int remaining = m_reviewController->remainingCount();
                const bool hasNextCard = remaining > 1;
                m_mainWindow->renderQuestionLayout(frontText, hasNextCard);
                // Pre-load back text so it's visible the moment the user flips,
                // without waiting for the showAnswer() signal chain to complete.
                if (Model::Card* card = m_reviewController->currentCard()) {
                    m_mainWindow->preloadAnswerText(card->back);
                }
                // 叠加今日已复习基线：进度 = 历史已完成 + 本会话已完成，
                // 总量 = 历史已完成 + 本会话待复习，确保关闭重开后进度不归零。
                const int total = m_reviewController->totalCount();
                const int sessionDone = total - remaining;
                m_mainWindow->updateProgressView(m_todayReviewedBaseline + sessionDone,
                                                 m_todayReviewedBaseline + total);
            });

    // ReviewController 进入答案态 → 揭示背面、弹出评分按钮
    connect(m_reviewController.get(), &ReviewController::signal_showAnswer,
            this, [this](const QString& backText) {
                m_mainWindow->renderAnswerLayout(backText);
            });

    // “连续选择简单”连胜变化，驱动中央看板的“火热连胜”徽章分档显示特效
    connect(m_reviewController.get(), &ReviewController::signal_streakChanged,
            this, [this](int easyStreak) {
                m_mainWindow->updateStreakBadge(easyStreak);
            });

    // 复习队列清空 → 跳转结算页
    connect(m_reviewController.get(), &ReviewController::signal_reviewFinished,
            this, [this]() {
                m_mainWindow->showFinishedSummaryPage();
                const int dayTotal = m_todayReviewedBaseline + m_reviewController->totalCount();
                m_mainWindow->updateProgressView(dayTotal, dayTotal);
            });

    // 牌组增删改 → 刷新左侧列表
    connect(m_deckController.get(), &DeckController::signal_deckListChanged,
            this, [this]() { refreshDeckList(); });

    // 初始化时立即填充一次牌组列表
    refreshDeckList();
    m_mainWindow->setTodayCheckInState(Service::StorageManager::isTodaySignedIn());

    // Deck management actions.

    // 当用户在主界面输入新牌组名称并点击确认时，路由到 DeckController 执行底层创建逻辑
    connect(m_mainWindow.get(), &MainWindow::signal_requestCreateDeck,
            this, [this](const QString& deckName) {
                qDebug() << "AppController request: create deck ->" << deckName;

                // 驱动底层模型去执行真正的磁盘写入和排重逻辑
                bool success = m_deckController->createDeck(deckName);

                if (success) {
                    qDebug() << "AppController: 新牌组创建成功！已落盘保存。";
                    // 底层 createDeck 成功后会自动发出 signal_deckListChanged，
                    // UI 会因为我们之前写好的刷新逻辑自动更新，这里无需额外写代码。
                } else {
                    qWarning() << "AppController 异常拦截: 牌组创建失败 (可能因重名或非法字符) ->" << deckName;
                }
            });

    connect(m_mainWindow.get(), &MainWindow::signal_requestCreateDeckFromCards,
            this, [this](const QString& deckName, const std::vector<CardDisplayInfo>& cards) {
                std::vector<std::pair<QString, QString>> cardPairs;
                cardPairs.reserve(cards.size());
                for (const CardDisplayInfo& card : cards) {
                    cardPairs.emplace_back(card.front, card.back);
                }

                if (!m_deckController->createDeckFromCards(deckName, cardPairs)) {
                    StyledDialogs::info(m_mainWindow.get(),
                                        QStringLiteral("创建失败"),
                                        QStringLiteral("无法创建牌组，请检查名称是否重复，且卡片正反面不能为空。"));
                    return;
                }

                StyledDialogs::info(m_mainWindow.get(),
                                    QStringLiteral("创建成功"),
                                    QStringLiteral("AI 牌组已创建，可以开始学习了。"));
                handleStartReview(deckName);
            });

    // 当用户在微型表单弹窗中点击确认时，路由到底层执行添加逻辑
    connect(m_mainWindow.get(), &MainWindow::signal_requestAddCard,
            this, [this](const QString& deckName, const QString& front, const QString& back) {
                if (!m_deckController->addCardToDeck(deckName, front, back))
                    qWarning() << "AppController: addCardToDeck failed";
            });

    // 打开卡片管理对话框：从 DeckController 取出卡片列表传给 MainWindow
    connect(m_mainWindow.get(), &MainWindow::signal_requestManageCards,
            this, [this](const QString& deckName) {
                std::vector<CardDisplayInfo> cards;
                for (const auto& deck : m_deckController->getDecks()) {
                    if (deck.deckName != deckName) continue;
                    for (const auto& cardPtr : deck.cards) {
                        if (cardPtr)
                            cards.push_back({cardPtr->id, cardPtr->front, cardPtr->back});
                    }
                    break;
                }
                m_mainWindow->showCardManagerDialog(deckName, cards);
            });

    connect(m_mainWindow.get(), &MainWindow::signal_requestPreviewDeck,
            this, [this](const QString& deckName) {
                std::vector<CardDisplayInfo> cards;
                for (const auto& deck : m_deckController->getDecks()) {
                    if (deck.deckName != deckName) continue;
                    for (const auto& cardPtr : deck.cards) {
                        if (cardPtr)
                            cards.push_back({cardPtr->id, cardPtr->front, cardPtr->back});
                    }
                    break;
                }
                m_mainWindow->showDeckPreviewDialog(deckName, cards);
            });

    // 删除单张卡片：先终止当前复习会话，否则 reviewQueue 中的裸指针会悬空
    connect(m_mainWindow.get(), &MainWindow::signal_requestDeleteCard,
            this, [this](const QString& deckName, const QString& cardId) {
                m_reviewController->finishReview();
                if (!m_deckController->deleteCardFromDeck(deckName, cardId))
                    qWarning() << "AppController: deleteCardFromDeck failed for" << cardId;
            });

    // 修改单张卡片：只改文本，不影响 SM-2 参数；不需要中止复习
    connect(m_mainWindow.get(), &MainWindow::signal_requestUpdateCard,
            this, [this](const QString& deckName, const QString& cardId,
                         const QString& newFront, const QString& newBack) {
                if (!m_deckController->updateCard(deckName, cardId, newFront, newBack))
                    qWarning() << "AppController: updateCard failed for" << cardId;
            });

    // 从 .in/.out 文件对导入牌组
    connect(m_mainWindow.get(), &MainWindow::signal_requestImportDeck,
            this, [this](const QString& filePath) {
                if (!m_deckController->importDeckFromFile(filePath))
                    qWarning() << "AppController: importDeckFromFile failed for" << filePath;
            });

    // 用户主动刷新当前卡组（增删卡片后用来重启复习队列）
    connect(m_mainWindow.get(), &MainWindow::signal_requestRefreshDeck,
            this, &AppController::handleStartReview);

    // 删除整个卡组：先中止复习会话以避免 reviewQueue 指针悬空，再让 DeckController 落盘删除
    connect(m_mainWindow.get(), &MainWindow::signal_requestDeleteDeck,
            this, [this](const QString& deckName) {
                m_reviewController->finishReview();
                if (!m_deckController->deleteDeck(deckName))
                    qWarning() << "AppController: deleteDeck failed for" << deckName;
            });

    // 路由 1：数据查询 (直接利用 Qt::DirectConnection 特性，瞬间填满引用的 outData)
    connect(m_mainWindow.get(), &MainWindow::signal_requestCalendarData,
            this, [this](int year, int month, QMap<QDate, QStringList>& outData) {
                outData = getCalendarData(year, month);
            }, Qt::DirectConnection);

    connect(m_mainWindow.get(), &MainWindow::signal_requestCheckInDates,
            this, [](int year, int month, QSet<QDate>& outDates) {
                outDates = Service::StorageManager::getMonthlyCheckInDates(year, month);
            }, Qt::DirectConnection);

    connect(m_mainWindow.get(), &MainWindow::signal_requestCheckIn,
            this, [this]() {
                const bool signedInNow = Service::StorageManager::markTodaySignedIn();
                m_mainWindow->setTodayCheckInState(true);
                if (signedInNow) {
                    m_mainWindow->showCheckInSuccessToast();
                }
            });

    // 路由 2：修改硬盘计划
    connect(m_mainWindow.get(), &MainWindow::signal_requestUpdateSchedule,
            this, &AppController::handleUpdateDeckSchedule);
}

void AppController::refreshDeckList() {
    std::vector<QString> names;
    for (const auto& deck : m_deckController->getDecks()) {
        names.push_back(deck.deckName);
    }
    m_mainWindow->updateDeckListView(names);
}

bool AppController::maybeShowReviewReminder(QWidget* parentForDialog) {
    if (!m_deckController) return false;

    QSettings settings("MindPalace", "Settings");
    if (!settings.value("reviewReminderEnabled", false).toBool()) {
        return false;
    }

    const QDate today = QDate::currentDate();
    const QString todayKey = today.toString("yyyy-MM-dd");
    if (settings.value("lastReviewReminderDate").toString() == todayKey) {
        return false;
    }

    QStringList deckLines;
    int totalCards = 0;

    for (const auto& deck : m_deckController->getDecks()) {
        const int manualStatus = deck.manualSchedule.value(todayKey, 0);
        if (manualStatus == -1) {
            continue;
        }

        const int dueCount = manualStatus == 1
                ? static_cast<int>(deck.cards.size())
                : deck.getDueCount();
        if (dueCount <= 0) {
            continue;
        }

        totalCards += dueCount;
        deckLines << tr("%1：%2 张").arg(deck.deckName).arg(dueCount);
    }

    if (totalCards <= 0) {
        return false;
    }

    const QString message = tr("今天需要复习以下牌组：\n\n%1\n\n共 %2 张卡片。")
            .arg(deckLines.join('\n'))
            .arg(totalCards);
    StyledDialogs::info(parentForDialog, tr("复习提醒"), message);
    settings.setValue("lastReviewReminderDate", todayKey);
    return true;
}

// Review flow

void AppController::handleStartReview(const QString& deckName) {
    qDebug() << "AppController request: start review ->" << deckName;

    // 记录当前正在复习的牌组名称
    m_currentReviewingDeckName = deckName;

    auto stats = m_deckController->getDeckStats(deckName);
    m_mainWindow->updateSummaryStats(stats.totalCards, stats.masteryRate, stats.totalReviews);

    // Look up the mutable deck used by the review session.
    Model::Deck* targetDeck = nullptr;

    for (const auto& deck : m_deckController->getDecks()) {
        if (deck.deckName == deckName) {
            targetDeck = const_cast<Model::Deck*>(&deck);
            break;
        }
    }

    if (!targetDeck) {
        qWarning() << "AppController 异常拦截: 无法在现有牌组库中匹配到名称:" << deckName;
        return;
    }

    // 今日复习进度基线：从持久化的卡片 lastReviewed 还原“今日已复习”的数量，
    // 这样关闭并重新打开应用后，进度条不会被错误地清零重算。
    m_todayReviewedBaseline = targetDeck->getReviewedTodayCount();

    // A manual rest day wins over the normal review schedule.
    QString todayStr = QDate::currentDate().toString("yyyy-MM-dd");
    if (targetDeck->manualSchedule.contains(todayStr)) {
        int todayPlan = targetDeck->manualSchedule.value(todayStr);
        if (todayPlan == -1) {
            qDebug() << "AppController: rest day for" << deckName;
            m_mainWindow->showFinishedSummaryPage();
            m_mainWindow->updateProgressView(m_todayReviewedBaseline, m_todayReviewedBaseline);
            return;
        }
    }

    // 文件路径从已解析的目录构建，与 initializeControllers 保持一致
    QString targetFilePath = QDir(m_decksDirPath).filePath(targetDeck->deckId + ".json");

    // Start the review queue for due cards.
    bool hasDueCards = m_reviewController->startReview(targetDeck, targetFilePath);

    if (hasDueCards) {
        qDebug() << "AppController: 成功唤醒复习队列，今日待攻克卡片总计:" << m_reviewController->totalCount() << "张";
    } else {
        // 没有到期卡片时 ReviewController 不会发任何信号，必须显式把 UI 切到完成态，
        // 否则切换卡组时旧的提问/评分按钮会残留在屏幕上。
        qDebug() << "AppController: 该牌组今日没有任何到期卡片。";
        m_mainWindow->showFinishedSummaryPage();
        // 没有待复习卡片时，进度即为今日已复习的全部（可能为 0，也可能是今天早些时候已完成的量）。
        m_mainWindow->updateProgressView(m_todayReviewedBaseline, m_todayReviewedBaseline);
    }
}

void AppController::handleSubmitFeedback(int quality) {
    // Scores are only accepted after the answer side has been shown.
    if (m_reviewController->currentState() != ReviewController::ReviewState::AnswerState) {
        qWarning() << "AppController 安全拦截: 当前卡片未处于答案态，评分按钮拒绝响应。";
        return;
    }

    // UI buttons use integer scores; the review controller keeps the typed enum.
    auto feedbackEnum = static_cast<ReviewController::ReviewFeedback>(quality);

    qDebug() << "AppController request: submit feedback ->" << quality;

    // submitFeedback updates SM-2 fields and writes the deck back to disk.
    bool saveResult = m_reviewController->submitFeedback(feedbackEnum);

    if (!saveResult) {
        qCritical() << "AppController: failed to save review feedback; card state was rolled back.";
    }

    // One successful feedback submission counts as one review for daily stats.
    Service::StorageManager::incrementDailyReviewCount();

    // Refresh the seven-day chart after the daily counter changes.
    std::vector<int> chartData;
    QStringList chartLabels;
    Service::StorageManager::getWeeklyReviewData(chartData, chartLabels);
    m_mainWindow->updateWeeklyChart(chartData, chartLabels);

    if (!m_currentReviewingDeckName.isEmpty()) {
        auto stats = m_deckController->getDeckStats(m_currentReviewingDeckName);
        m_mainWindow->updateSummaryStats(stats.totalCards, stats.masteryRate, stats.totalReviews);
    }
}

void AppController::handleResetDeck(const QString& deckName) {
    qDebug() << "AppController: 正在重置牌组进度 ->" << deckName;

    if (!m_deckController->resetDeck(deckName)) {
        qWarning() << "AppController: 牌组重置失败:" << deckName;
        return;
    }

    qDebug() << "AppController: 重置完成，重启复习会话...";
    handleStartReview(deckName);
}

void AppController::handleAppQuit() {
    // Deck changes are saved during each operation, so shutdown currently only
    // needs to pass through cleanly.
    qDebug() << "AppController: closing.";
}
QMap<QDate, QStringList> AppController::getCalendarData(int year, int month) {
    QMap<QDate, QStringList> calendarData;

    // 遍历所有牌组，计算该月每一天需要复习哪些牌组
    for (const auto& deck : m_deckController->getDecks()) {

        // First collect dates generated by the review algorithm.
        for (const auto& cardPtr : deck.cards) {
            if (cardPtr) {
                QDate naturalDate = cardPtr->nextReviewDate;
                if (naturalDate.year() == year && naturalDate.month() == month) {
                    // 如果这天本来就要复习这个牌组，且还没记录过，就加进去
                    if (!calendarData[naturalDate].contains(deck.deckName)) {
                        calendarData[naturalDate].append(deck.deckName);
                    }
                }
            }
        }

        // Then apply manual calendar overrides.
        for (auto it = deck.manualSchedule.constBegin(); it != deck.manualSchedule.constEnd(); ++it) {
            QDate schedDate = QDate::fromString(it.key(), "yyyy-MM-dd");
            if (schedDate.year() == year && schedDate.month() == month) {
                int status = it.value();

                if (status == -1) {
                    // Rest day: remove this deck from the due list.
                    calendarData[schedDate].removeAll(deck.deckName);
                } else if (status == 1) {
                    // Forced review: include the deck even if no card is due naturally.
                    if (!calendarData[schedDate].contains(deck.deckName)) {
                        calendarData[schedDate].append(deck.deckName);
                    }
                }
            }
        }
    }
    return calendarData;
}

void AppController::handleUpdateDeckSchedule(const QString& deckName, const QDate& date, int status) {
    qDebug() << "AppController 调度: 用户修改日历计划 -> 牌组:" << deckName << "日期:" << date << "状态:" << status;

    Model::Deck* targetDeck = nullptr;
    for (const auto& deck : m_deckController->getDecks()) {
        if (deck.deckName == deckName) {
            targetDeck = const_cast<Model::Deck*>(&deck);
            break;
        }
    }

    if (!targetDeck) return;

    QString dateStr = date.toString("yyyy-MM-dd");
    if (status == 0) {
        targetDeck->manualSchedule.remove(dateStr); // 0代表清除用户设置，恢复算法默认
    } else {
        targetDeck->manualSchedule[dateStr] = status; // 写入 1 (复习) 或 -1 (休假)
    }

    QString targetFilePath = QDir(m_decksDirPath).filePath(targetDeck->deckId + ".json");
    if (!Service::StorageManager::saveDeck(*targetDeck, targetFilePath)) {
        qCritical() << "AppController 调度落盘失败: 无法持久化牌组日历计划 ->" << targetFilePath;
    }
}
} // namespace MindPalace::Controller
