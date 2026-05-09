// motion_mode_manager.h - 运动模式管理器
// 实现多种机器人运动模式的管理和切换
//
// 核心功能：
// 1. 支持PASSIVE, DAMPING, JOINT, STAND, LOCOMOTION模式
// 2. 模式转换验证和安全检查
// 3. 模式特定参数管理
// 4. 优先级输入源仲裁

#ifndef MOTION_MODE_MANAGER_H
#define MOTION_MODE_MANAGER_H

#include "gait_state_machine.h"
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <string>

namespace aurora::gait {

/**
 * @brief 运动模式
 */
enum class MotionMode : uint8_t {
    PASSIVE = 0,      // 零扭矩，顺应模式
    DAMPING = 1,      // 阻尼模式，逐渐停止
    JOINT = 2,        // 关节位置控制
    STAND = 3,        // 站立姿态，带平衡
    LOCOMOTION = 4,   // 行走模式
    UNKNOWN = 255
};

/**
 * @brief 运动模式字符串转换
 */
inline const char* motionModeToString(MotionMode mode) {
    switch (mode) {
        case MotionMode::PASSIVE:    return "PASSIVE";
        case MotionMode::DAMPING:    return "DAMPING";
        case MotionMode::JOINT:      return "JOINT";
        case MotionMode::STAND:      return "STAND";
        case MotionMode::LOCOMOTION: return "LOCOMOTION";
        default:                     return "UNKNOWN";
    }
}

inline MotionMode stringToMotionMode(const std::string& str) {
    if (str == "PASSIVE")    return MotionMode::PASSIVE;
    if (str == "DAMPING")    return MotionMode::DAMPING;
    if (str == "JOINT")      return MotionMode::JOINT;
    if (str == "STAND")      return MotionMode::STAND;
    if (str == "LOCOMOTION") return MotionMode::LOCOMOTION;
    return MotionMode::UNKNOWN;
}

/**
 * @brief 运动模式参数基类
 */
struct MotionModeParams {
    MotionMode mode;
    bool torque_enabled;
    double stiffness;
    double damping;

    MotionModeParams(MotionMode m = MotionMode::UNKNOWN)
        : mode(m)
        , torque_enabled(false)
        , stiffness(0.0)
        , damping(0.0)
    {}

    virtual ~MotionModeParams() = default;
};

/**
 * @brief PASSIVE模式参数
 */
struct PassiveModeParams : public MotionModeParams {
    PassiveModeParams()
        : MotionModeParams(MotionMode::PASSIVE)
    {
        torque_enabled = false;
        stiffness = 0.0;
        damping = 0.0;
    }
};

/**
 * @brief DAMPING模式参数
 */
struct DampingModeParams : public MotionModeParams {
    double target_velocity;  // 目标速度（通常为0）
    double damping_ratio;    // 阻尼比

    DampingModeParams()
        : MotionModeParams(MotionMode::DAMPING)
        , target_velocity(0.0)
        , damping_ratio(0.5)
    {
        torque_enabled = true;
        damping = 10.0;
    }
};

/**
 * @brief JOINT模式参数
 */
struct JointModeParams : public MotionModeParams {
    std::vector<double> target_positions;      // 目标关节位置
    std::vector<double> target_velocities;     // 目标关节速度
    double position_tolerance;                 // 位置容差
    double velocity_tolerance;                 // 速度容差

    JointModeParams()
        : MotionModeParams(MotionMode::JOINT)
        , position_tolerance(0.01)
        , velocity_tolerance(0.1)
    {
        torque_enabled = true;
        stiffness = 1000.0;
        damping = 50.0;
    }
};

/**
 * @brief STAND模式参数
 */
struct StandModeParams : public MotionModeParams {
    bool balance_enabled;    // 启用平衡控制
    double body_height;      // 期望身体高度
    std::vector<double> default_joint_positions;  // 默认站立关节位置

    StandModeParams()
        : MotionModeParams(MotionMode::STAND)
        , balance_enabled(true)
        , body_height(0.7)
    {
        torque_enabled = true;
        stiffness = 1000.0;
        damping = 50.0;
    }
};

/**
 * @brief LOCOMOTION模式参数
 */
struct LocomotionModeParams : public MotionModeParams {
    GaitMode gait_mode;             // 步态模式
    GaitParameters gait_params;     // 步态参数
    double target_velocity;         // 目标速度

    LocomotionModeParams()
        : MotionModeParams(MotionMode::LOCOMOTION)
        , gait_mode(GaitMode::BIPED_WALK)
        , target_velocity(0.0)
    {
        torque_enabled = true;
        stiffness = 500.0;
        damping = 30.0;
    }
};

/**
 * @brief 运动模式转换结果
 */
enum class ModeTransitionResult : uint8_t {
    SUCCESS,           // 转换成功
    INVALID_SOURCE,    // 源模式无效
    INVALID_TARGET,    // 目标模式无效
    NOT_ALLOWED,       // 转换不被允许
    SAFETY_VIOLATION,  // 违反安全约束
    TIMEOUT            // 转换超时
};

/**
 * @brief 运动模式事件
 */
enum class ModeEvent : uint8_t {
    MODE_CHANGED,           // 模式已改变
    TRANSITION_STARTED,     // 转换开始
    TRANSITION_COMPLETED,   // 转换完成
    TRANSITION_FAILED,      // 转换失败
    SAFETY_TRIGGERED        // 安全触发
};

/**
 * @brief 运动模式管理器配置
 */
struct MotionModeManagerConfig {
    MotionMode default_mode;                    // 默认模式
    double transition_timeout;                   // 转换超时（秒）
    bool enable_safety_checks;                   // 启用安全检查
    bool allow_emergency_mode_switch;            // 允许紧急模式切换

    MotionModeManagerConfig()
        : default_mode(MotionMode::LOCOMOTION)
        , transition_timeout(2.0)
        , enable_safety_checks(true)
        , allow_emergency_mode_switch(true)
    {}
};

/**
 * @brief 运动模式事件回调
 */
using ModeEventCallback = std::function<void(
    MotionMode from,
    MotionMode to,
    ModeEvent event,
    ModeTransitionResult result
)>;

/**
 * @brief 运动模式管理器
 *
 * 管理机器人运动模式的状态和转换
 */
class MotionModeManager {
public:
    explicit MotionModeManager(
        const MotionModeManagerConfig& config = {});

    ~MotionModeManager() = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const MotionModeManagerConfig& config);

    /**
     * @brief 获取配置
     */
    const MotionModeManagerConfig& getConfig() const;

    /**
     * @brief 请求模式转换
     * @param target_mode 目标模式
     * @param force 是否强制转换（跳过安全检查）
     * @return 转换结果
     */
    ModeTransitionResult requestModeTransition(
        MotionMode target_mode,
        bool force = false);

    /**
     * @brief 立即切换模式（紧急情况）
     * @param target_mode 目标模式
     * @return 转换结果
     */
    ModeTransitionResult emergencyModeSwitch(MotionMode target_mode);

    /**
     * @brief 获取当前模式
     */
    MotionMode getCurrentMode() const;

    /**
     * @brief 获取上一个模式
     */
    MotionMode getPreviousMode() const;

    /**
     * @brief 判断模式转换是否被允许
     */
    bool isTransitionAllowed(MotionMode from, MotionMode to) const;

    /**
     * @brief 获取模式参数
     */
    template<typename T>
    std::shared_ptr<T> getModeParams(MotionMode mode) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mode_params_.find(static_cast<int>(mode));
        if (it != mode_params_.end()) {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        return nullptr;
    }

    /**
     * @brief 设置模式参数
     */
    void setModeParams(
        MotionMode mode,
        std::shared_ptr<MotionModeParams> params);

    /**
     * @brief 设置事件回调
     */
    void setEventCallback(ModeEventCallback callback);

    /**
     * @brief 更新模式管理器
     * @param dt 时间步长
     */
    void update(double dt);

    /**
     * @brief 重置到默认模式
     */
    void reset();

    /**
     * @brief 判断是否在转换中
     */
    bool isTransitioning() const;

    /**
     * @brief 获取转换进度 [0, 1]
     */
    double getTransitionProgress() const;

private:
    MotionModeManagerConfig config_;
    MotionMode current_mode_;
    MotionMode previous_mode_;
    MotionMode target_mode_;

    bool transitioning_;
    double transition_progress_;
    double transition_time_;

    // 模式参数映射
    std::unordered_map<int, std::shared_ptr<MotionModeParams>> mode_params_;

    ModeEventCallback event_callback_;
    mutable std::mutex mutex_;

    /**
     * @brief 初始化默认模式参数
     */
    void initializeDefaultModeParams();

    /**
     * @brief 验证模式转换安全性
     */
    bool validateTransitionSafety(MotionMode from, MotionMode to) const;

    /**
     * @brief 执行模式转换
     */
    void executeModeTransition(MotionMode from, MotionMode to);

    /**
     * @brief 触发事件回调
     */
    void triggerEventCallback(
        MotionMode from,
        MotionMode to,
        ModeEvent event,
        ModeTransitionResult result);
};

/**
 * @brief 输入源优先级
 */
enum class InputSourcePriority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief 输入源仲裁器
 *
 * 管理多个输入源的优先级和冲突
 */
class InputSourceArbitrator {
public:
    explicit InputSourceArbitrator() = default;
    ~InputSourceArbitrator() = default;

    /**
     * @brief 注册输入源
     */
    void registerInputSource(
        const std::string& source_id,
        InputSourcePriority priority);

    /**
     * @brief 注销输入源
     */
    void unregisterInputSource(const std::string& source_id);

    /**
     * @brief 获取当前活动输入源
     */
    std::string getActiveSource() const;

    /**
     * @brief 请求输入源激活
     */
    bool requestActivation(const std::string& source_id);

    /**
     * @brief 释放输入源
     */
    void releaseActivation(const std::string& source_id);

private:
    struct SourceInfo {
        std::string id;
        InputSourcePriority priority;
        bool active;
    };

    std::vector<SourceInfo> sources_;
    mutable std::mutex mutex_;

    /**
     * @brief 获取最高优先级活动源
     */
    std::string getHighestPrioritySource() const;
};

} // namespace aurora::gait

#endif // MOTION_MODE_MANAGER_H
