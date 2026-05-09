// safety_system.h - 机器人安全系统
// 实现综合安全保护，包括急停、关节限位、输入超时检测
//
// 核心功能：
// 1. 紧急停止 - 立即停止所有运动
// 2. 关节限制保护 - 位置、速度、力矩限制
// 3. 输入超时检测 - 检测输入丢失
// 4. 看门狗定时器 - 定期喂狗防止死锁

#ifndef SAFETY_SYSTEM_H
#define SAFETY_SYSTEM_H

#include "gait_state_machine.h"
#include "kinematics_control_layer.h"  // For JointState definition
#include <vector>
#include <mutex>
#include <functional>
#include <chrono>
#include <string>
#include <unordered_map>

namespace aurora::gait {

/**
 * @brief 安全事件类型
 */
enum class SafetyEvent : uint8_t {
    EMERGENCY_STOP_TRIGGERED,     // 急停触发
    JOINT_LIMIT_VIOLATION,        // 关节限位违反
    INPUT_TIMEOUT,                // 输入超时
    WATCHDOG_EXPIRED,             // 看门狗超时
    VELOCITY_EXCEEDED,            // 速度超限
    POSITION_EXCEEDED,            // 位置超限
    EFFORT_EXCEEDED,              // 力矩超限
    COLLISION_DETECTED,           // 碰撞检测
    STABILITY_LOST                // 稳定性丢失
};

/**
 * @brief 关节限位配置
 */
struct JointLimit {
    std::string joint_name;
    int joint_index;

    // 位置限制（弧度）
    double min_position;
    double max_position;

    // 速度限制（弧度/秒）
    double max_velocity;

    // 力矩限制（N·m）
    double max_effort;

    // 使能标志
    bool enabled;

    JointLimit()
        : joint_name("")
        , joint_index(-1)
        , min_position(-M_PI)
        , max_position(M_PI)
        , max_velocity(10.0)
        , max_effort(100.0)
        , enabled(true)
    {}

    JointLimit(const std::string& name, int index,
               double pos_min, double pos_max,
               double vel_max, double eff_max)
        : joint_name(name)
        , joint_index(index)
        , min_position(pos_min)
        , max_position(pos_max)
        , max_velocity(vel_max)
        , max_effort(eff_max)
        , enabled(true)
    {}
};

/**
 * @brief 安全系统配置
 */
struct SafetySystemConfig {
    // 紧急停止
    bool emergency_stop_enabled;
    double emergency_stop_response_time;  // 秒

    // 关节限位
    std::vector<JointLimit> joint_limits;

    // 输入超时
    bool input_timeout_enabled;
    double input_timeout_seconds;

    // 看门狗
    bool watchdog_enabled;
    double watchdog_timeout_seconds;
    double watchdog_feed_interval;

    // 碰撞检测
    bool collision_detection_enabled;
    double collision_force_threshold;     // N
    double collision_duration_threshold;  // 秒

    // 稳定性检查
    bool stability_check_enabled;
    double stability_margin;              // 米
    double max_com_velocity;              // 米/秒

    SafetySystemConfig()
        : emergency_stop_enabled(true)
        , emergency_stop_response_time(0.01)
        , input_timeout_enabled(true)
        , input_timeout_seconds(1.0)
        , watchdog_enabled(true)
        , watchdog_timeout_seconds(0.5)
        , watchdog_feed_interval(0.1)
        , collision_detection_enabled(false)
        , collision_force_threshold(50.0)
        , collision_duration_threshold(0.1)
        , stability_check_enabled(true)
        , stability_margin(0.05)
        , max_com_velocity(2.0)
    {}
};

/**
 * @brief 安全事件回调
 */
using SafetyEventCallback = std::function<void(SafetyEvent event, const std::string& message)>;

/**
 * @brief 安全系统状态
 */
struct SafetySystemStatus {
    bool emergency_stop_active;
    bool input_timeout_triggered;
    bool watchdog_expired;
    bool joint_limit_violated;
    bool collision_detected;
    bool stability_lost;

    std::string violated_joint_name;
    SafetyEvent last_event;
    std::string last_message;

    SafetySystemStatus()
        : emergency_stop_active(false)
        , input_timeout_triggered(false)
        , watchdog_expired(false)
        , joint_limit_violated(false)
        , collision_detected(false)
        , stability_lost(false)
        , violated_joint_name("")
        , last_event(static_cast<SafetyEvent>(255))
        , last_message("")
    {}

    /**
     * @brief 检查是否有任何安全违规
     */
    bool hasViolation() const {
        return emergency_stop_active ||
               input_timeout_triggered ||
               watchdog_expired ||
               joint_limit_violated ||
               collision_detected ||
               stability_lost;
    }

    /**
     * @brief 重置状态
     */
    void reset() {
        emergency_stop_active = false;
        input_timeout_triggered = false;
        watchdog_expired = false;
        joint_limit_violated = false;
        collision_detected = false;
        stability_lost = false;
        violated_joint_name = "";
        last_message = "";
    }
};

/**
 * @brief 安全系统
 *
 * 综合安全保护系统
 */
class SafetySystem {
public:
    explicit SafetySystem(const SafetySystemConfig& config = {});
    ~SafetySystem() = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const SafetySystemConfig& config);

    /**
     * @brief 获取配置
     */
    const SafetySystemConfig& getConfig() const;

    /**
     * @brief 获取状态
     */
    const SafetySystemStatus& getStatus() const;

    /**
     * @brief 设置关节限位
     */
    void setJointLimit(const JointLimit& limit);

    /**
     * @brief 批量设置关节限位
     */
    void setJointLimits(const std::vector<JointLimit>& limits);

    /**
     * @brief 检查关节状态
     * @return 违规返回true
     */
    bool checkJointState(int joint_index, const JointState& state);

    /**
     * @brief 批量检查关节状态
     * @return 违规返回true
     */
    bool checkJointStates(const std::vector<JointState>& states);

    /**
     * @brief 触发紧急停止
     */
    void triggerEmergencyStop(const std::string& reason = "Manual trigger");

    /**
     * @brief 清除紧急停止
     */
    void clearEmergencyStop();

    /**
     * @brief 判断紧急停止是否激活
     */
    bool isEmergencyStopActive() const;

    /**
     * @brief 喂看门狗
     */
    void feedWatchdog();

    /**
     * @brief 重置看门狗
     */
    void resetWatchdog();

    /**
     * @brief 检查输入超时
     */
    void checkInputTimeout();

    /**
     * @brief 更新输入时间戳
     */
    void updateInputTimestamp();

    /**
     * @brief 检查看门狗
     */
    void checkWatchdog();

    /**
     * @brief 更新安全系统
     * @param dt 时间步长
     */
    void update(double dt);

    /**
     * @brief 重置所有状态
     */
    void reset();

    /**
     * @brief 设置事件回调
     */
    void setEventCallback(SafetyEventCallback callback);

    /**
     * @brief 获取安全违规描述
     */
    static std::string getEventDescription(SafetyEvent event);

private:
    SafetySystemConfig config_;
    SafetySystemStatus status_;

    // 时间戳
    std::chrono::steady_clock::time_point last_input_time_;
    std::chrono::steady_clock::time_point last_watchdog_feed_;

    // 关节限位映射
    std::unordered_map<int, JointLimit> joint_limits_map_;

    // 回调
    SafetyEventCallback event_callback_;

    mutable std::mutex mutex_;

    /**
     * @brief 触发安全事件
     */
    void triggerSafetyEvent(SafetyEvent event, const std::string& message);

    /**
     * @brief 检查单个关节限位
     */
    bool checkSingleJointLimit(
        const JointLimit& limit,
        const JointState& state,
        std::string& violation_message);
};

/**
 * @brief 关节限位验证器
 *
 * 快速关节状态验证
 */
class JointLimitValidator {
public:
    explicit JointLimitValidator(const std::vector<JointLimit>& limits);

    /**
     * @brief 验证关节状态
     * @param joint_index 关节索引
     * @param state 关节状态
     * @return 有效返回true
     */
    bool validate(int joint_index, const JointState& state) const;

    /**
     * @brief 验证并裁剪关节状态
     * @param joint_index 关节索引
     * @param state 关节状态（将被裁剪到限制范围内）
     * @return 原始状态有效返回true
     */
    bool validateAndClip(int joint_index, JointState& state) const;

    /**
     * @brief 获取关节限位
     */
    const JointLimit* getJointLimit(int joint_index) const;

private:
    std::unordered_map<int, JointLimit> limits_map_;
    mutable std::mutex mutex_;
};

/**
 * @brief 看门狗定时器
 *
 * 定期喂狗防止系统挂起
 */
class WatchdogTimer {
public:
    explicit WatchdogTimer(double timeout_seconds, double feed_interval);

    /**
     * @brief 喂狗
     */
    void feed();

    /**
     * @brief 检查是否超时
     */
    bool isExpired() const;

    /**
     * @brief 重置
     */
    void reset();

    /**
     * @brief 获取距离上次喂狗的时间（秒）
     */
    double getTimeSinceLastFeed() const;

private:
    double timeout_seconds_;
    std::chrono::steady_clock::time_point last_feed_time_;
    mutable std::mutex mutex_;
};

/**
 * @brief 输入超时检测器
 *
 * 检测输入信号丢失
 */
class InputTimeoutDetector {
public:
    explicit InputTimeoutDetector(double timeout_seconds);

    /**
     * @brief 更新输入时间戳
     */
    void update();

    /**
     * @brief 检查是否超时
     */
    bool isTimeout() const;

    /**
     * @brief 获取距离上次输入的时间（秒）
     */
    double getTimeSinceLastInput() const;

    /**
     * @brief 重置
     */
    void reset();

private:
    double timeout_seconds_;
    std::chrono::steady_clock::time_point last_input_time_;
    mutable std::mutex mutex_;
};

} // namespace aurora::gait

#endif // SAFETY_SYSTEM_H
