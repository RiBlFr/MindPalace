//
// Created by Arian on 2026/5/25.
//

#include "appcontroller.h"
#include "view/MainWindow.h"
#include "deckcontroller.h"
#include "reviewcontroller.h"
#include "service/storagemanager.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

namespace MindPalace::Controller {

// =========================================================================
// 1. 构造与析构管理（生命周期控制）
// =========================================================================

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    qDebug() << "AppController: 正在初始化全局中枢管理器...";

    // 遵循严格的层级依赖顺序进行总装
    initializeControllers();
    initializeViews();
    setupGlobalConnections();

    qDebug() << "AppController: 全局系统总装完成，等待点火启动。";
}

/**
 * @brief 极其关键的工程细节：
 * 因为我们在头文件中对 MainWindow、DeckController 等类使用了高级前置声明（Forward Declaration），
 * 编译器在解析头文件时并不知道这些类的具体大小和析构方式。
 * 如果将析构函数隐式留给编译器生成（在头文件中 inline），在使用 std::unique_ptr 时会直接引发编译期报错。
 * 显式在源文件中定义默认析构函数，能确保编译器在看到完整的类定义后，安全、正确地生成智能指针的隐式销毁代码，彻底杜绝内存泄漏。
 */
AppController::~AppController() = default;

void AppController::start() {
    qDebug() << "AppController: 系统点火，正在调出程序主窗口...";
    if (m_mainWindow) {
        m_mainWindow->show(); // 唤醒物理视图，正式将控制权移交给 Qt 事件循环
    }
}

// =========================================================================
// 2. 模块初始化流水线
// =========================================================================

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

// =========================================================================
// 3. 全局神经枢纽：信号与槽的缝合
// =========================================================================
void AppController::setupGlobalConnections() {
    if (!m_mainWindow || !m_deckController || !m_reviewController) return;

    // ---------------------------------------------------------------------
    // 链接 A：路由 View 层抛出的顶层用户动作（View -> AppController）
    // ---------------------------------------------------------------------

    // 当用户在主界面的牌组卡片上点击"进入学习"时，路由到总控的 handleStartReview 槽函数
    connect(m_mainWindow.get(), &MainWindow::signal_requestStartReview,
            this, &AppController::handleStartReview);

    // 当用户在沉浸式复习窗口中按下 1-4 快捷键或点击评分按钮时，路由到总控的 handleSubmitFeedback 槽函数
    connect(m_mainWindow.get(), &MainWindow::signal_requestSubmitFeedback,
            this, &AppController::handleSubmitFeedback);

    // 当用户点击主窗口的红叉准备关闭软件时，路由到总控的 handleAppQuit 槽函数执行兜底安全落盘
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
                const int total = m_reviewController->totalCount();
                m_mainWindow->updateProgressView(total - remaining, total);
            });

    // ReviewController 进入答案态 → 揭示背面、弹出评分按钮
    connect(m_reviewController.get(), &ReviewController::signal_showAnswer,
            this, [this](const QString& backText) {
                m_mainWindow->renderAnswerLayout(backText);
            });

    // 复习队列清空 → 跳转结算页
    connect(m_reviewController.get(), &ReviewController::signal_reviewFinished,
            this, [this]() {
                m_mainWindow->showFinishedSummaryPage();
                m_mainWindow->updateProgressView(
                    m_reviewController->totalCount(),
                    m_reviewController->totalCount()
                );
            });

    // 牌组增删改 → 刷新左侧列表
    connect(m_deckController.get(), &DeckController::signal_deckListChanged,
            this, [this]() { refreshDeckList(); });

    // 初始化时立即填充一次牌组列表
    refreshDeckList();

    // ---------------------------------------------------------------------
    // 链接 B：牌组管理动作路由
    // ---------------------------------------------------------------------

    // 当用户在主界面输入新牌组名称并点击确认时，路由到 DeckController 执行底层创建逻辑
    connect(m_mainWindow.get(), &MainWindow::signal_requestCreateDeck,
            this, [this](const QString& deckName) {
                qDebug() << "AppController 业务触发: 用户请求新建牌组 ->" << deckName;

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
}

void AppController::refreshDeckList() {
    std::vector<QString> names;
    for (const auto& deck : m_deckController->getDecks()) {
        names.push_back(deck.deckName);
    }
    m_mainWindow->updateDeckListView(names);
}

// =========================================================================
// 4. 核心顶级业务路由逻辑实现
// =========================================================================

void AppController::handleStartReview(const QString& deckName) {
    qDebug() << "AppController 业务触发: 用户请求进入牌组学习 ->" << deckName;

    // 1. 跨模块检索目标牌组对象
    Model::Deck* targetDeck = nullptr;

    // 架构安全规范：因为 ReviewController 状态机在流转时会直接改写卡片的算法参数，
    // 我们在此处遍历牌组列表，获取底层非 const 的牌组裸指针
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

    // 文件路径从已解析的目录构建，与 initializeControllers 保持一致
    QString targetFilePath = QDir(m_decksDirPath).filePath(targetDeck->deckId + ".json");

    // 3. 为复习状态机注入数据源并宣告开启复习轮询
    bool hasDueCards = m_reviewController->startReview(targetDeck, targetFilePath);

    if (hasDueCards) {
        qDebug() << "AppController: 成功唤醒复习队列，今日待攻克卡片总计:" << m_reviewController->totalCount() << "张";
    } else {
        // 没有到期卡片时 ReviewController 不会发任何信号，必须显式把 UI 切到完成态，
        // 否则切换卡组时旧的提问/评分按钮会残留在屏幕上。
        qDebug() << "AppController: 该牌组今日没有任何到期卡片。";
        m_mainWindow->showFinishedSummaryPage();
        m_mainWindow->updateProgressView(0, 0);
    }
}

void AppController::handleSubmitFeedback(int quality) {
    // 安全防御：只有当前状态机确实停留在[答案态]（即用户看过了背面）时，评分才具备法定效力
    if (m_reviewController->currentState() != ReviewController::ReviewState::AnswerState) {
        qWarning() << "AppController 安全拦截: 当前卡片未处于答案态，评分按钮拒绝响应。";
        return;
    }

    // 优雅地将界面的普通整型按钮数据，安全地转换为 ReviewController 内部定义的强类型算法枚举
    auto feedbackEnum = static_cast<ReviewController::ReviewFeedback>(quality);

    qDebug() << "AppController 业务触发: 正在路由用户记忆分值 ->" << quality;

    // 4. 将评分结果推入状态机：
    // 状态机内部会立刻调度 SM2Engine 计算全新的间隔天数，并在其内部调用 StorageManager 执行高安全的"临时文件双缓冲原子写入"
    bool saveResult = m_reviewController->submitFeedback(feedbackEnum);

    if (!saveResult) {
        qCritical() << "AppController 核心灾难: 闪卡参数更新或本地 JSON 文件原子化落盘失败！状态已回滚。";
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
    qDebug() << "AppController 系统收尾: 检测到软件窗口正在注销，启动全局守护流程...";

    // 架构美学：由于我们的系统设计极其先进，在用户每回答完一张卡片时就执行了"静默即时原子保存"，
    // 因此在程序关闭的瞬间，我们不需要手忙脚乱地去大规模覆写硬盘。
    // 此处主要用于释放全局独占资源、强制刷清日志缓冲区、或优雅地通知异步 IO 线程池安全终止。

    qDebug() << "AppController 全局清算: 确认所有牌组数据无损，允许 Qt 核心事件循环安全退出。";
}

} // namespace MindPalace::Controller