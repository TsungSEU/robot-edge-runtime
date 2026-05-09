// humanoid_state.h (43-dim)
// Humanoid 状态空间定义 (43维)
// 参考: aurora-planning-engine/docs/guides/scenarios.md
#ifndef HUMANOID_STATE_H
#define HUMANOID_STATE_H

#include <vector>
#include <string>
#include <cmath>
#include <array>
#include <algorithm>

namespace aurora::planner {

/**
 * @brief Nav_Data 场景原始状态信息 (未归一化)
 *
 * 分层控制架构: Aurora (10Hz) → LivelyBot (50Hz) → MuJoCo (1000Hz)
 */
struct HumanoidStateInfo {
    // ===== 基座线速度 (from LivelyBot) =====
    double vx;                  // 前进速度 (m/s)
    double vy;                  // 侧向速度 (m/s)
    double vz;                  // 垂直速度 (m/s)

    // ===== 基座角速度 (from LivelyBot) =====
    double wx;                  // roll角速度 (rad/s)
    double wy;                  // pitch角速度 (rad/s)
    double wz;                  // yaw角速度 (rad/s)

    // ===== 归一化位置 =====
    double x;                   // 全局X坐标 (m)
    double y;                   // 全局Y坐标 (m)

    // ===== 朝向 =====
    double theta;               // 朝向角 (rad)

    // ===== 目标方向 =====
    double goal_dx;             // 目标Δx (m)
    double goal_dy;             // 目标Δy (m)
    double goal_distance;       // 目标距离 (m)
    double goal_bearing;        // 目标相对朝向 (rad) = atan2(goal_dy, goal_dx) - theta

    // ===== 数据价值扇区 (8方向) =====
    std::array<double, 8> data_value_sectors;  // [0,1] 每个方向的数据价值评分

    // ===== 障碍物扇区 (4方向: 前/后/左/右) =====
    std::array<double, 4> obstacle_sectors;    // 归一化距离 [0,1]

    // ===== 当前位置数据 =====
    double current_value;       // 当前位置数据价值 [0,1]
    double current_rarity;      // 当前位置稀缺度 [0,1]

    // ===== 采集状态 =====
    double collected_ratio;     // 已收集比例 [0,1]
    double coverage_ratio;      // 覆盖率 [0,1]

    // ===== 环境 =====
    int terrain_type;           // 地形类型 (0-6)
    double obstacle_density;    // 局部障碍密度 [0,1]

    // ===== 步态相位 (from LivelyBot) =====
    double gait_phase;          // 步态相位 [0,1]

    // ===== 动作历史 (最近8步 forward_vel) =====
    std::array<double, 8> action_history;      // 最近8步的前进速度命令

    // ===== 剩余预算 =====
    double remaining_budget;    // [0,1]

    // ===== 地图参数 (用于归一化) =====
    double map_width;           // 环境宽度 (m)
    double map_height;          // 环境高度 (m)
    double max_range;           // 观测最大范围 (m)

    HumanoidStateInfo()
        : vx(0), vy(0), vz(0)
        , wx(0), wy(0), wz(0)
        , x(0), y(0)
        , theta(0)
        , goal_dx(0), goal_dy(0), goal_distance(0), goal_bearing(0)
        , data_value_sectors{}
        , obstacle_sectors{}
        , current_value(0), current_rarity(0)
        , collected_ratio(0), coverage_ratio(0)
        , terrain_type(0), obstacle_density(0)
        , gait_phase(0)
        , action_history{}
        , remaining_budget(1.0)
        , map_width(40.0), map_height(40.0), max_range(10.0) {}
};

/**
 * @brief 归一化后的 Humanoid 状态向量 (43维)
 *
 * 维度布局 (对齐训练侧 humanoid_nav_data_training.yaml):
 *  [0-2]:   base_lin_vel [vx, vy, vz]      ×2.0
 *  [3-5]:   base_ang_vel [wx, wy, wz]      ×1.0
 *  [6-7]:   norm_position [x/W, y/H]
 *  [8-9]:   heading [sinθ, cosθ]
 *  [10-11]: goal_direction [sinΔθ, cosΔθ]
 *  [12-14]: goal_distance [Δx, Δy, ‖Δ‖]    ÷max_range
 *  [15-22]: data_value_sectors (8方向)       [0,1]
 *  [23-26]: obstacle_sectors (4方向)         ÷max_range
 *  [27-28]: current_value [value, rarity]    [0,1]
 *  [29-30]: collection_status [ratio, coverage] [0,1]
 *  [31]:    terrain_type                     ÷6
 *  [32]:    obstacle_density                 [0,1]
 *  [33]:    gait_phase                       sin(2π·φ)
 *  [34-41]: action_history (8步)             raw forward_vel
 *  [42]:    remaining_budget                 [0,1]
 */
struct HumanoidState {
    static constexpr int STATE_DIM = 43;

    std::vector<double> features;

    HumanoidState() : features(STATE_DIM, 0.0) {}

    /**
     * @brief 从 HumanoidStateInfo 构建43维归一化状态
     */
    static HumanoidState fromStateInfo(const HumanoidStateInfo& info);

    size_t size() const { return features.size(); }

    double operator[](size_t index) const {
        return (index < features.size()) ? features[index] : 0.0;
    }

    void setFeature(size_t index, double value) {
        if (index < features.size()) {
            features[index] = value;
        }
    }

    const std::vector<double>& getFeatures() const { return features; }

    bool isValid() const {
        return features.size() == STATE_DIM &&
               !std::isnan(features[0]) && !std::isinf(features[0]);
    }

    std::string toString() const;
};

} // namespace aurora::planner

#endif // HUMANOID_STATE_H
