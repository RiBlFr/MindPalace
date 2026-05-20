//
// Created by Arian on 2026/5/20.
//

#include "sm2engine.h"
#include <algorithm>
#include <QDate>

namespace MindPalace::Service {

void SM2Engine::calculate(Model::Card* card, int quality) {
    // 1. 安全拦截：防止空指针引发程序的段错误
    if (!card) return;

    // 2. 刷新复习时间戳
    // 无论用户回答得好坏，只要进行了一次判定，上次复习日期就必须更新为今天
    card->lastReviewed = QDate::currentDate();

    // ==========================================
    // 核心分支 A：记忆断裂 (生疏)
    // ==========================================
    if (quality < 3) {
        // 当评分 q < 3 (即选择了“生疏”) 时，意味着记忆提取彻底失败
        card->repetitions = 0;       // 连续成功记忆的次数被无情清零
        card->interval = 1.0f;       // 间隔被强制重置为 1 天，打回原形
        card->nextReviewDate = card->lastReviewed.addDays(1);

        // 算法精髓：在经典 SM-2 中，完全忘记时 EF 难度系数通常保持不变，
        // 重点在于重置复习周期，因此这里直接返回，跳过 EF 的扣减。
        return;
    }

    // ==========================================
    // 核心分支 B：记忆成功提取 (困难 / 良好 / 简单)
    // ==========================================

    // 1. 成功复习次数累加
    card->repetitions += 1;

    // 2. 难度系数的动态演化计算
    // 原始公式：EF' = EF + (0.1 - (5 - q) * (0.08 + (5 - q) * 0.02))
    auto q = static_cast<float>(quality);
    const float oldEF = card->easeFactor;
    const float newEF = oldEF + (0.1f - (5.0f - q) * (0.08f + (5.0f - q) * 0.02f));

    // 触底反弹机制：为了防止某张卡片因为连续回答“困难”导致间隔无限收缩，
    // 强制规定 EF 的绝对下限为 1.3
    card->easeFactor = std::max(1.3f, newEF);

    // 3. 间隔区间的阶梯式增长
    if (card->repetitions == 1) {
        card->interval = 1.0f;       // 第一次成功：明天再复习一次以巩固短期记忆
    } else if (card->repetitions == 2) {
        card->interval = 6.0f;       // 第二次成功：记忆开始稳固，直接拉长到 6 天后
    } else {
        // 第三次及以后：进入长时记忆区，复习间隔呈指数级放大
        card->interval = card->interval * card->easeFactor;
    }

    // 4. 精准推算下一次的绝对复习日期
    // Qt 的 addDays 函数接收整数，这里将 float 向下取整，确保天数逻辑稳健
    const int daysToAdd = static_cast<int>(card->interval);
    card->nextReviewDate = card->lastReviewed.addDays(daysToAdd);
}

} // namespace MindPalace::Service