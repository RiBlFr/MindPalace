# 记忆殿堂 (Mind Palace) 🧠

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Framework-Qt%206-green.svg)](https://www.qt.io/)

**“记忆殿堂”** 是一款基于 C++ 与 Qt 框架开发的跨平台桌面端智能闪卡（Flashcard）学习工具 。它结合了认知心理学的 **SM-2 间隔重复算法** 与现代化的极简设计，旨在帮助学习者科学地对抗遗忘，实现高效记忆。

## ✨ 核心特性

*   **算法驱动复习**：引入经典 SM-2 算法，根据用户的反馈动态调整卡片复习频率，精准定位遗忘临界点 。
*   **增强型 MVC 架构**：系统深度解耦，采用被动视图（Passive View）模式，确保逻辑与界面的高度独立 。
*   **极简美学交互**：左侧常驻导航与右侧看板布局，支持沉浸式复习模式与快捷键操作 。
*   **数据安全保障**：采用“双缓冲原子替换”机制执行 JSON 写入，有效防止突发断电导致的文件损坏 。
*   **进度可视化**：内置自定义绘制的环形进度条与统计报表，直观感知学习进度 。

## 🏗️ 系统架构

本项目采用 **增强型 MVC + 服务层** 模式进行开发 ：

*   **Model 层**：纯粹的数据容器，使用 `std::unique_ptr` 严格管理卡片所有权 。
*   **Service 层**：提供无状态的算法计算（SM2Engine）与原子化文件 IO（StorageManager） 。
*   **Controller 层**：作为系统中枢管理复习状态机，隔离 View 与 Model 。
*   **View 层**：负责 UI 渲染，通过 Qt 信号槽机制向上抛出用户事件 。

### 📂 项目目录结构

```text
MindPalace/
├── src/
│   ├── model/       # Card, Deck 数据模型 
│   ├── service/     # SM-2 算法引擎, 原子化存储管理 
│   ├── controller/  # 业务状态流转中枢 
│   └── view/        # MainWindow, 复习窗口, 自定义组件 
├── res/             # QSS 样式表与图标资源 
└── data/decks/      # 用户数据存储目录 (.json) 
```

## 🧠 核心算法：SM-2

系统通过用户的反馈等级（生疏、困难、良好、简单）实时计算下一次复习日期 ：

1.  **难度系数 (EF) 更新**：基于反馈质量 $q$ 动态调整卡片权重 。
2.  **复习间隔 (I) 调度**：根据复习次数 $n$ 采用不同的倍率公式，实现记忆曲线的自动化适配 。

## 🛠️ 协作规范

*   **代码风格**：统一使用 `CamelCase`（小驼峰）命名法，关键函数必须包含 Doxygen 风格注释 。
*   **版本控制**：使用 Git 协作，每日 Push 进度。严禁直接在 `main` 分支进行未验证的修改 。

## ⚖️ 开源协议

本项目采用 MIT LICENSE 协议授权 。
