// humanoid_reward.h
// Humanoid 奖励函数定义 (10分量)
// 参考: aurora-planning-engine/config/humanoid_nav_data_training.yaml
#ifndef HUMANOID_REWARD_H
#define HUMANOID_REWARD_H

#include <string>
#include <cmath>
#include <map>
#include "common/log/logger.h"

namespace aurora::planner {

/**
 * @brief Humanoid 奖励配置 (10分量)
 *
 * 对齐训练侧 humanoid_nav_data_training.yaml 的 reward 配置
 */
struct HumanoidRewardConfig {
    // 所有参数从 planner_weights.yaml (humanoid.reward.*) 加载，无硬编码默认值
    double w_approach = 0.0;
    double w_goal = 0.0;
    double w_data_value = 0.0;
    double w_value_guide = 0.0;
    double w_coverage = 0.0;
    double w_efficiency = 0.0;
    double w_repeat = 0.0;
    double w_collision = 0.0;
    double w_speed = 0.0;
    double w_time = 0.0;
    double first_visit_bonus = 0.0;
    double collision_penalty = 0.0;
    double near_obstacle_scale = 0.0;
    double safe_distance = 0.0;
    double stall_penalty = 0.0;
    double min_speed_threshold = 0.0;
};

/**
 * @brief Humanoid 奖励状态 (用于计算奖励的输入)
 */
struct HumanoidRewardState {
    // 导航
    double dist_to_goal = 0.0;
    double prev_dist_to_goal = 0.0;
    bool reached_goal = false;

    // 数据采集
    double data_value = 0.0;          // 当前位置数据价值
    double data_rarity = 0.0;         // 当前位置稀缺度
    bool is_new_visit = false;        // 是否首次访问
    double coverage_ratio = 0.0;      // 覆盖率
    double prev_coverage_ratio = 0.0;

    // 高价值引导
    double value_alignment = 0.0;     // cos(Δθ) × forward_vel

    // 安全
    bool collision = false;
    double min_obstacle_dist = 10.0;  // 最近障碍距离

    // 速度
    double forward_vel = 0.0;
    double prev_forward_vel = 0.0;

    // 效率
    bool is_revisit = false;          // 是否重复访问已收集区域

    HumanoidRewardState() = default;
};

/**
 * @brief Humanoid 奖励分解结果
 */
struct HumanoidRewardBreakdown {
    double approach_reward = 0.0;
    double goal_reward = 0.0;
    double data_value_reward = 0.0;
    double value_guide_reward = 0.0;
    double coverage_reward = 0.0;
    double efficiency_penalty = 0.0;
    double repeat_penalty = 0.0;
    double collision_penalty = 0.0;
    double speed_reward = 0.0;
    double time_penalty = 0.0;
    double total = 0.0;
};

/**
 * @brief Humanoid 奖励计算器
 */
class HumanoidRewardCalculator {
public:
    /**
     * @brief 计算 Humanoid 总奖励
     */
    static double computeReward(const HumanoidRewardState& state,
                                const HumanoidRewardConfig& config) {
        auto breakdown = computeRewardBreakdown(state, config);
        return breakdown.total;
    }

    /**
     * @brief 计算奖励分解 (用于分析)
     */
    static HumanoidRewardBreakdown computeRewardBreakdown(
        const HumanoidRewardState& state,
        const HumanoidRewardConfig& config) {
        HumanoidRewardBreakdown bd;

        // 1. 目标接近奖励
        double progress = state.prev_dist_to_goal - state.dist_to_goal;
        bd.approach_reward = config.w_approach * progress;

        // 2. 目标完成奖励 (稀疏)
        bd.goal_reward = state.reached_goal ? config.w_goal : 0.0;

        // 3. 数据价值采集 (核心)
        bd.data_value_reward = config.w_data_value * state.data_value * state.data_rarity;
        if (state.is_new_visit) {
            bd.data_value_reward += config.first_visit_bonus;
        }

        // 4. 高价值引导
        bd.value_guide_reward = config.w_value_guide * state.value_alignment;

        // 5. 覆盖率
        double coverage_gain = state.coverage_ratio - state.prev_coverage_ratio;
        bd.coverage_reward = config.w_coverage * coverage_gain;

        // 6. 路径效率惩罚
        if (progress <= 0 && !state.reached_goal) {
            bd.efficiency_penalty = config.w_efficiency * (-0.1);
        }

        // 7. 重复访问惩罚
        if (state.is_revisit) {
            bd.repeat_penalty = config.w_repeat * (-0.5);
        }

        // 8. 碰撞/危险惩罚
        if (state.collision) {
            bd.collision_penalty = config.w_collision * config.collision_penalty;
        } else if (state.min_obstacle_dist < config.safe_distance) {
            double danger = 1.0 - state.min_obstacle_dist / config.safe_distance;
            bd.collision_penalty = config.w_collision * config.near_obstacle_scale * (-danger);
        }

        // 9. 速度跟踪
        if (std::abs(state.forward_vel) < config.min_speed_threshold &&
            !state.reached_goal) {
            bd.speed_reward = config.w_speed * config.stall_penalty;
        } else {
            bd.speed_reward = config.w_speed * 0.1 * state.forward_vel;
        }

        // 10. 时间惩罚
        bd.time_penalty = config.w_time * (-0.1);

        // 总和
        bd.total = bd.approach_reward + bd.goal_reward + bd.data_value_reward +
                   bd.value_guide_reward + bd.coverage_reward + bd.efficiency_penalty +
                   bd.repeat_penalty + bd.collision_penalty + bd.speed_reward +
                   bd.time_penalty;

        return bd;
    }

    /**
     * @brief 从 YAML 参数映射更新配置
     */
    static HumanoidRewardConfig fromParams(const std::map<std::string, double>& params) {
        HumanoidRewardConfig config;
        auto get = [&](const std::string& key) -> double {
            auto it = params.find(key);
            if (it == params.end()) {
                AD_WARN(HumanoidReward, "Missing reward config key: %s", key.c_str());
                return 0.0;
            }
            return it->second;
        };

        config.w_approach    = get("humanoid_reward_w_approach");
        config.w_goal        = get("humanoid_reward_w_goal");
        config.w_data_value  = get("humanoid_reward_w_data_value");
        config.w_value_guide = get("humanoid_reward_w_value_guide");
        config.w_coverage    = get("humanoid_reward_w_coverage");
        config.w_efficiency  = get("humanoid_reward_w_efficiency");
        config.w_repeat      = get("humanoid_reward_w_repeat");
        config.w_collision   = get("humanoid_reward_w_collision");
        config.w_speed       = get("humanoid_reward_w_speed");
        config.w_time        = get("humanoid_reward_w_time");

        config.first_visit_bonus     = get("humanoid_reward_first_visit_bonus");
        config.collision_penalty     = get("humanoid_reward_collision_penalty");
        config.near_obstacle_scale   = get("humanoid_reward_near_obstacle_scale");
        config.safe_distance         = get("humanoid_reward_safe_distance");
        config.stall_penalty         = get("humanoid_reward_stall_penalty");
        config.min_speed_threshold   = get("humanoid_reward_min_speed_threshold");

        return config;
    }
};

} // namespace aurora::planner

#endif // HUMANOID_REWARD_H
