// humanoid_action.h (3-dim)
// Humanoid 动作空间定义 (3维连续速度命令)
// 参考: aurora-planning-engine/config/humanoid_nav_data_training.yaml
#ifndef HUMANOID_ACTION_H
#define HUMANOID_ACTION_H

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace aurora::planner {

/**
 * @brief Nav_Data 动作空间 (3维连续速度命令)
 *
 * 动作范围与 LivelyBot Pi 训练范围对齐:
 *  - forward_vel: [-0.3, 0.6] m/s (允许后退)
 *  - lateral_vel: [-0.3, 0.3] m/s
 *  - angular_vel: [-0.3, 0.3] rad/s
 *
 * ONNX 模型输出为 [-1, 1] 归一化值，通过 denormalize 映射到实际范围
 */
struct HumanoidAction {
    static constexpr int ACTION_DIM = 3;

    double forward_vel;    // 前进速度 (m/s) [-0.3, 0.6]
    double lateral_vel;    // 侧向速度 (m/s) [-0.3, 0.3]
    double angular_vel;    // 角速度 (rad/s) [-0.3, 0.3]

    HumanoidAction()
        : forward_vel(0), lateral_vel(0), angular_vel(0) {}

    HumanoidAction(double fwd, double lat, double ang)
        : forward_vel(fwd), lateral_vel(lat), angular_vel(ang) {}

    /**
     * @brief 从归一化向量 [-1, 1] 构建 (ONNX 输出)
     */
    static HumanoidAction fromNormalized(const std::vector<double>& normalized) {
        HumanoidAction action;
        if (normalized.size() >= 3) {
            action.forward_vel = denormalize(normalized[0], -0.3, 0.6);
            action.lateral_vel = denormalize(normalized[1], -0.3, 0.3);
            action.angular_vel = denormalize(normalized[2], -0.3, 0.3);
        }
        return action;
    }

    /**
     * @brief 转换为归一化向量 [-1, 1]
     */
    std::vector<double> toNormalized() const {
        return {
            normalize(forward_vel, -0.3, 0.6),
            normalize(lateral_vel, -0.3, 0.3),
            normalize(angular_vel, -0.3, 0.3)
        };
    }

    /**
     * @brief 裁剪到有效范围
     */
    void clip() {
        forward_vel = std::clamp(forward_vel, -0.3, 0.6);
        lateral_vel = std::clamp(lateral_vel, -0.3, 0.3);
        angular_vel = std::clamp(angular_vel, -0.3, 0.3);
    }

    bool isValid() const {
        return !std::isnan(forward_vel) && !std::isnan(lateral_vel) && !std::isnan(angular_vel) &&
               !std::isinf(forward_vel) && !std::isinf(lateral_vel) && !std::isinf(angular_vel);
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "HumanoidAction(fwd=" << forward_vel
            << ", lat=" << lateral_vel
            << ", ang=" << angular_vel << ")";
        return oss.str();
    }

    // ===== 归一化/反归一化工具 =====

    static double denormalize(double norm, double min_val, double max_val) {
        return 0.5 * (norm + 1.0) * (max_val - min_val) + min_val;
    }

    static double normalize(double value, double min_val, double max_val) {
        if (max_val == min_val) return 0.0;
        return 2.0 * (value - min_val) / (max_val - min_val) - 1.0;
    }
};

/**
 * @brief Humanoid 动作范围参数 (对齐训练配置)
 */
struct HumanoidActionParams {
    double min_forward_vel = -0.3;   // m/s
    double max_forward_vel = 0.6;    // m/s
    double min_lateral_vel = -0.3;   // m/s
    double max_lateral_vel = 0.3;    // m/s
    double min_angular_vel = -0.3;   // rad/s
    double max_angular_vel = 0.3;    // rad/s

    static constexpr double CLIP_ACTIONS = 18.0;  // 对齐训练配置
};

} // namespace aurora::planner

#endif // HUMANOID_ACTION_H
