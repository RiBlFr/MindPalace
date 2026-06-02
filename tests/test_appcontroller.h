//
// Created by Arian on 2026/5/26.
//

#ifndef TEST_APPCONTROLLER_H
#define TEST_APPCONTROLLER_H

#include <QtTest>
#include "controller/appcontroller.h"

class TestAppController : public QObject {
    Q_OBJECT

private slots:
    // QtTest 会自动识别并按顺序执行以 private slots 声明的以下函数

    void initTestCase();    // 在所有测试开始前执行一次
    void cleanupTestCase(); // 在所有测试结束后执行一次
    void init();            // 在每单个测试用例开始前执行（环境重置）
    void cleanup();         // 在每单个测试用例结束后执行

    // ==========================================
    // 具体的测试用例（Test Cases）
    // ==========================================
    void test_initialization_safety(); // 测试初始化是否内存安全
    void test_signal_routing_startReview(); // 测试核心信号路由是否接通
};

#endif // TEST_APPCONTROLLER_H
