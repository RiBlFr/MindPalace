//
// Created by Arian on 2026/5/20.
//

#ifndef MINDPALACE_SM2ENGINE_H
#define MINDPALACE_SM2ENGINE_H

#include "model/Card.h"

namespace MindPalace::Service {

    /**
     * @brief 间隔重复 (Spaced Repetition) 算法引擎
     * 封装 SM-2 调度规则，负责更新卡片的间隔、难度系数和下次复习日期。
     */
    class SM2Engine {
    public:
        // Stateless utility class.
        SM2Engine() = delete;

        /**
         * @brief 根据用户的记忆提取反馈，更新卡片的调度参数
         * @param card 待更新卡片，函数会直接修改该对象
         * @param quality 记忆反馈质量评分 (范围 0-5)：
         * - 0: 忘记，重置复习周期
         * - 3: 困难，降低难度系数
         * - 4: 普通，按当前节奏推进
         * - 5: 熟悉，更快拉长下次复习间隔
         */
        static void calculate(Model::Card* card, int quality);
    };

} // namespace MindPalace::Service

#endif // MINDPALACE_SM2ENGINE_H
