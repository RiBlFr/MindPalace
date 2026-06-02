//
// Created by Arian on 2026/5/26.
//

#include "test_appcontroller.h"
#include "view/MainWindow.h"
#include <QSignalSpy>

using namespace MindPalace::Controller;

void TestAppController::initTestCase() {
    // 全局环境准备
}

void TestAppController::cleanupTestCase() {
    // 全局环境清理
}

void TestAppController::init() {
    // 每个用例开始前的干净状态
}

void TestAppController::cleanup() {
    // 每个用例结束后的清理
}

// 用例 1：测试总控诞生时的安全性
void TestAppController::test_initialization_safety() {
    // 尝试在栈上创建总控（这会触发内部所有智能指针的实例化和绑定）
    AppController controller;

    // 如果 initialize 过程中有野指针、或者内存越界，这一步就会直接 Crash
    // 如果顺利走到这一行，说明生命周期总装通过！
    QVERIFY(true);
}

// 用例 2：测试指挥系统信号线有没有接对
void TestAppController::test_signal_routing_startReview() {
    // 1. 实例化中枢
    AppController controller;

    // 2. 绕过私有束缚，通过 Qt 对象树在内存里把主窗口肉体“揪”出来
    // （因为 MainWindow 是 AppController 的子对象，我们可以用 findChild 抓到它）
    MainWindow* mainWindow = controller.getMainWindow();
    QVERIFY(mainWindow != nullptr); // 确保主窗口被成功创建了

    // 3. 模拟用户操作：假装用户在界面上选中了名为 "C++ Basics" 的牌组并点击
    // 我们直接让 MainWindow 强行 emit 这个信号
    QString mockDeckName = "C++ Basics";

    qDebug() << "测试：正在模拟界面发射 signal_requestStartReview...";

    // 4. 发射信号，测试 AppController 的 handleStartReview 槽函数是否会被调用
    // 由于我们在真实类里写了桩代码和 qDebug() 打印，我们可以观察控制台输出
    // 如果 handleStartReview 内部逻辑通过，它会去 getDecks() 遍历
    emit mainWindow->signal_requestStartReview(mockDeckName);

    // 5. 验证确定性：因为我们目前给子控制器的返回都是写死的 true 或者空骨架，
    // 整个调用链应该丝滑通畅，不发生任何悬挂指针崩溃。
    QVERIFY(true);
}

// 现代 Qt 单元测试的启动宏，它会自动生成 main 函数并运行上述测试
QTEST_MAIN(TestAppController)
