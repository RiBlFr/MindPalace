//
// Created by Arian on 2026/5/20.
//

#ifndef MINDPALACE_SM2ENGINE_H
#define MINDPALACE_SM2ENGINE_H

#include "model/Card.h"

namespace MindPalace::Service {

    /**
     * @brief 间隔重复 (Spaced Repetition) 算法引擎
     * * 这是一个基于认知心理学“遗忘曲线”设计的无状态服务类 (Stateless Service)。
     * 它负责封装改进版的 SuperMemo-2 (SM-2) 核心算法，
     * 通过纯数学运算，动态调整学习者的认知负荷与复习频率。
     */
    class SM2Engine {
    public:
        // ==========================================
        // 1. 构造控制
        // ==========================================
        // 明确禁用构造函数和析构函数。
        // 作为一个纯算法工具类，它不需要在内存中实例化任何对象。
        SM2Engine() = delete;

        // ==========================================
        // 2. 核心调度接口
        // ==========================================
        /**
         * @brief 根据用户的记忆提取反馈，更新卡片的调度参数
         * * 此函数会根据用户的评分，直接修改传入卡片的难度系数(Ease Factor, EF)、
         * 成功复习次数(Repetitions)以及下一次的复习间隔(Interval)。
         * * @param card    指向待更新卡片对象的指针 (直接在原内存上修改，避免拷贝开销)
         * @param quality 记忆反馈质量评分 (范围 0-5)：
         * - 0: 完全忘记 (生疏) -> 认知链路断裂，强制重置复习周期
         * - 3: 经提示想起 (困难) -> 记忆提取极其费力，大幅削减 EF
         * - 4: 犹豫后想起 (良好) -> 标准的成功提取，维持当前学习节奏
         * - 5: 瞬间想起 (简单) -> 认知过度顺滑，加速延长下一次复习间隔
         */
        static void calculate(Model::Card* card, int quality);
    };

} // namespace MindPalace::Service

#endif // MINDPALACE_SM2ENGINE_H
