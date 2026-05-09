// gait_trigger.h - 基于步态的采集触发器
// 核心理念：不在规划点采集，而在足端实际落地的稳定期采集

#pragma once

#include <memory>
#include <functional>
#include <chrono>
#include <vector>
#include <mutex>
#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include "aurora_edge_runtime/srv/trigger_recording.hpp"
#include "trigger/ITrigger.h"

// 性能优化：使用无锁环形缓冲区
#include "common/performance_utils.h"

namespace aurora::collector {

// 常量定义
constexpr size_t MAX_FOOTPRINTS = 1000;  // 最大足迹记录数量

/**
 * @brief 步态事件类型
 */
enum class GaitEventType {
    FOOTSTRIKE,      // 足端着地
    STABLE_STANCE,   // 双脚支撑稳定期（可采集）
    SWING_START,     // 摆动相开始
    STEP_COMPLETE    // 单步完成
};

/**
 * @brief 步态事件回调
 */
using GaitEventCallback = std::function<void(const GaitEventType& event, const Point& foot_position)>;

/**
 * @brief 足端轨迹点（用于记录实际足迹）
 */
struct Footprint {
    Point position;           // 足端世界坐标位置
    double timestamp;          // 时间戳
    bool is_left_foot;         // 是否左脚
    double gait_phase;         // 步态相位
    bool is_stable;            // 是否稳定（可用于采集）

    Footprint() : position(0, 0), timestamp(0), is_left_foot(false),
                 gait_phase(0), is_stable(false) {}

    Footprint(double x, double y, bool left, double phase, bool stable)
        : position(x, y), is_left_foot(left), gait_phase(phase), is_stable(stable) {
        timestamp = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};

/**
 * @brief 步态触发器
 *
 * 监听机器人实际状态，在合适的时机触发采集
 * 核心原则：
 * 1. 采集基于实际足端落点，而非规划路径点
 * 2. 采集时机在双脚支撑的稳定期
 * 3. 考虑步长约束，避免过于密集采集
 */
class GaitTrigger : public rclcpp::Node {
public:
    GaitTrigger();
    ~GaitTrigger() = default;

    /**
     * @brief 设置步态事件回调
     */
    void setEventCallback(GaitEventCallback callback);

    /**
     * @brief 检查是否应该触发采集（基于当前步态状态）
     *
     * @param current_pos 当前机器人位置
     * @param last_collect_pos 上次采集位置
     * @param last_collect_time 上次采集时间
     * @return true 如果应该采集
     */
    bool shouldTriggerCollection(const Point& current_pos,
                                const Point& last_collect_pos,
                                const std::chrono::steady_clock::time_point& last_collect_time);

    /**
     * @brief 获取最近的稳定足迹（用于采集点选择）
     */
    Footprint getLastStableFootprint() const;

    /**
     * @brief 获取所有记录的足迹
     */
    std::vector<Footprint> getFootprints() const;

    /**
     * @brief 设置采集参数
     */
    void setMinStepDistance(double distance);
    void setMinCollectionInterval(double seconds);

private:
    // ROS2 service client for triggering recording
    rclcpp::Client<aurora_edge_runtime::srv::TriggerRecording>::SharedPtr trigger_client_;

    // Async trigger method
    void triggerRecordingViaService(const TriggerContext& context);

    // ROS2 订阅者
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_sub_;

    // 回调处理
    void odomCallback(const nav_msgs::msg::Odometry::SharedPtr msg);
    void jointCallback(const sensor_msgs::msg::JointState::SharedPtr msg);

    // 步态分析
    void analyzeGaitState();
    bool isInStablePhase() const;
    void detectFootstrike();

    // 步态状态
    struct GaitState {
        double phase;              // 步态相位 [0-2π]
        bool left_foot_stance;     // 左脚是否在支撑相
        bool right_foot_stance;    // 右脚是否在支撑相
        double left_foot_z;        // 左脚高度
        double right_foot_z;       // 右脚高度
        bool is_stable;            // 是否在稳定期（双脚支撑且稳定）
        uint64_t step_count;       // 步数

        GaitState() : phase(0), left_foot_stance(true), right_foot_stance(true),
                     left_foot_z(0), right_foot_z(0), is_stable(true), step_count(0) {}
    } gait_state_;

    // 机器人状态
    Point robot_position_;
    double robot_yaw_;

    // 性能优化：使用无锁环形缓冲区存储足迹历史
    // 容量为 MAX_FOOTPRINTS + 1，避免缓冲区满时无法写入
    // mutable 允许在 const 成员函数中修改
    mutable aurora::performance::LockFreeRingBuffer<Footprint, MAX_FOOTPRINTS + 1> footprints_ring_;

    // 为了兼容 getFootprints() 返回 vector，保留一个缓存
    mutable std::vector<Footprint> footprints_cache_;

    // 事件回调
    GaitEventCallback event_callback_;

    // 采集参数
    double min_step_distance_ = 0.15;      // 最小步长 15cm
    double min_collection_interval_ = 1.0; // 最小采集间隔 1秒
    double stable_phase_threshold_ = 0.3;  // 稳定相位阈值（支撑相中间30%）

    // 互斥锁
    mutable std::mutex state_mutex_;
};

} // namespace aurora::collector
