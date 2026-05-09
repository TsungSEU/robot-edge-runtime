// velocity_locomotion_controller.h - 速度控制步行控制器
// 实现简单的速度命令接口，将速度转换为步态参数
//
// 核心功能：
// 1. 接受速度命令 (forward, lateral, angular)
// 2. 将速度转换为步态参数 (step_length, frequency, height)
// 3. 与现有GaitCoordinator集成
// 4. 支持速度平滑（使用Ruckig）

#ifndef VELOCITY_LOCOMOTION_CONTROLLER_H
#define VELOCITY_LOCOMOTION_CONTROLLER_H

#include "gait_state_machine.h"
#include "ruckig_trajectory_adapter.h"
#include <memory>
#include <mutex>
#include <functional>
#include <cmath>

namespace aurora::gait {

/**
 * @brief 速度命令
 */
struct VelocityCommand {
    double forward;   // 前向速度 (米/秒，正值前进)
    double lateral;   // 侧向速度 (米/秒，正值向左)
    double angular;   // 角速度 (弧度/秒，正值逆时针)

    VelocityCommand(double f = 0, double l = 0, double a = 0)
        : forward(f), lateral(l), angular(a)
    {}

    /**
     * @brief 计算合速度
     */
    double horizontalNorm() const {
        return std::sqrt(forward * forward + lateral * lateral);
    }

    /**
     * @brief 检查是否为零速度
     */
    bool isZero(double tolerance = 1e-6) const {
        return std::abs(forward) < tolerance &&
               std::abs(lateral) < tolerance &&
               std::abs(angular) < tolerance;
    }

    /**
     * @brief 裁剪到最大速度
     */
    VelocityCommand clamp(double max_forward, double max_lateral, double max_angular) const {
        return VelocityCommand(
            std::clamp(forward, -max_forward, max_forward),
            std::clamp(lateral, -max_lateral, max_lateral),
            std::clamp(angular, -max_angular, max_angular)
        );
    }
};

/**
 * @brief 速度到步态参数转换器配置
 */
struct VelocityToGaitConverterConfig {
    // 步频范围（Hz）
    double min_frequency;
    double max_frequency;

    // 最大步长（米）
    double max_step_length;

    // 侧向运动对步高的影响系数
    double lateral_height_factor;

    // 转向对占空比的影响系数
    double turn_duty_factor;

    // 速度对步频的影响系数
    double velocity_frequency_factor;

    VelocityToGaitConverterConfig()
        : min_frequency(0.8)
        , max_frequency(2.0)
        , max_step_length(0.4)
        , lateral_height_factor(0.02)
        , turn_duty_factor(0.1)
        , velocity_frequency_factor(0.5)
    {}
};

/**
 * @brief 速度平滑配置
 */
struct VelocitySmoothingConfig {
    bool enabled;                    // 是否启用平滑
    bool use_ruckig;                 // 是否使用Ruckig
    double max_acceleration;         // 最大加速度 (米/秒²)
    double max_jerk;                 // 最大加加速度 (米/秒³)
    double smoothing_time;           // 平滑时间（秒）

    VelocitySmoothingConfig()
        : enabled(true)
        , use_ruckig(true)
        , max_acceleration(5.0)
        , max_jerk(20.0)
        , smoothing_time(0.2)
    {}
};

/**
 * @brief 速度控制配置
 */
struct VelocityLocomotionControllerConfig {
    // 速度限制
    double max_forward;
    double max_lateral;
    double max_angular;

    double min_forward;
    double min_lateral;
    double min_angular;

    // 速度缩放
    double velocity_scale;

    // 速度到步态转换配置
    VelocityToGaitConverterConfig converter_config;

    // 速度平滑配置
    VelocitySmoothingConfig smoothing_config;

    VelocityLocomotionControllerConfig()
        : max_forward(1.0)
        , max_lateral(1.0)
        , max_angular(1.0)
        , min_forward(0.2)
        , min_lateral(0.2)
        , min_angular(0.1)
        , velocity_scale(1.0)
        , converter_config()
        , smoothing_config()
    {}
};

/**
 * @brief 速度命令回调
 */
using VelocityCommandCallback = std::function<void(const VelocityCommand& command)>;

/**
 * @brief 步态参数更新回调
 */
using GaitParametersUpdateCallback = std::function<void(const GaitParameters& params)>;

/**
 * @brief 速度控制步行控制器
 *
 * 将速度命令转换为步态参数并协调运动
 */
class VelocityLocomotionController {
public:
    explicit VelocityLocomotionController(
        const VelocityLocomotionControllerConfig& config = {});

    ~VelocityLocomotionController() = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const VelocityLocomotionControllerConfig& config);

    /**
     * @brief 获取配置
     */
    const VelocityLocomotionControllerConfig& getConfig() const;

    /**
     * @brief 设置速度命令
     * @param command 速度命令
     * @param immediate 是否立即应用（跳过平滑）
     */
    void setVelocityCommand(const VelocityCommand& command, bool immediate = false);

    /**
     * @brief 获取当前速度命令
     */
    VelocityCommand getCurrentVelocityCommand() const;

    /**
     * @brief 获取目标速度命令
     */
    VelocityCommand getTargetVelocityCommand() const;

    /**
     * @brief 停止运动（零速度）
     */
    void stop();

    /**
     * @brief 更新控制器
     * @param dt 时间步长（秒）
     * @return 转换后的步态参数
     */
    GaitParameters update(double dt);

    /**
     * @brief 速度转步态参数
     * @param velocity 速度命令
     * @return 步态参数
     */
    GaitParameters velocityToGaitParameters(const VelocityCommand& velocity) const;

    /**
     * @brief 设置速度命令回调
     */
    void setVelocityCommandCallback(VelocityCommandCallback callback);

    /**
     * @brief 设置步态参数更新回调
     */
    void setGaitParametersUpdateCallback(GaitParametersUpdateCallback callback);

    /**
     * @brief 重置控制器
     */
    void reset();

    /**
     * @brief 判断是否在运动
     */
    bool isMoving() const;

    /**
     * @brief 获取运动进度 [0, 1]
     */
    double getMotionProgress() const;

private:
    VelocityLocomotionControllerConfig config_;
    VelocityCommand current_velocity_;
    VelocityCommand target_velocity_;

    // Ruckig平滑器（3维：forward, lateral, angular）
    std::unique_ptr<RuckigTrajectoryAdapter> velocity_smoother_;
    bool smoother_initialized_;

    // 回调
    VelocityCommandCallback velocity_command_callback_;
    GaitParametersUpdateCallback gait_parameters_update_callback_;

    mutable std::mutex mutex_;

    /**
     * @brief 初始化速度平滑器
     */
    void initializeVelocitySmoother();

    /**
     * @brief 平滑速度命令
     */
    VelocityCommand smoothVelocity(const VelocityCommand& target, double dt);

    /**
     * @brief 触发速度命令回调
     */
    void triggerVelocityCommandCallback(const VelocityCommand& command);

    /**
     * @brief 触发步态参数更新回调
     */
    void triggerGaitParametersUpdateCallback(const GaitParameters& params);
};

/**
 * @brief 速度控制模式
 *
 * 定义不同的速度控制策略
 */
enum class VelocityControlMode : uint8_t {
    DIRECT,              // 直接控制：速度直接映射到步态参数
    SMOOTH,              // 平滑控制：使用Ruckig平滑速度变化
    ADAPTIVE,            // 自适应控制：根据环境动态调整
    PREDICTIVE           // 预测控制：基于预测模型的控制
};

/**
 * @brief 自适应速度控制器
 *
 * 根据环境反馈自动调整速度
 */
class AdaptiveVelocityController {
public:
    explicit AdaptiveVelocityController(
        const VelocityLocomotionControllerConfig& config = {});

    ~AdaptiveVelocityController() = default;

    /**
     * @brief 设置控制模式
     */
    void setControlMode(VelocityControlMode mode);

    /**
     * @brief 获取控制模式
     */
    VelocityControlMode getControlMode() const;

    /**
     * @brief 设置环境反馈
     * @param slope 坡度（弧度，正值上坡）
     * @param roughness 地形粗糙度 [0, 1]
     * @param friction 摩擦系数 [0, 1]
     */
    void setEnvironmentFeedback(double slope, double roughness, double friction);

    /**
     * @brief 调整速度命令
     * @param command 原始速度命令
     * @return 调整后的速度命令
     */
    VelocityCommand adjustVelocity(const VelocityCommand& command) const;

    /**
     * @brief 获取环境适应性评分 [0, 1]
     */
    double getAdaptationScore() const;

private:
    VelocityControlMode mode_;
    VelocityLocomotionControllerConfig config_;

    // 环境反馈
    double slope_;
    double roughness_;
    double friction_;

    mutable std::mutex mutex_;

    /**
     * @brief 计算坡度调整因子
     */
    double calculateSlopeFactor() const;

    /**
     * @brief 计算粗糙度调整因子
     */
    double calculateRoughnessFactor() const;

    /**
     * @brief 计算摩擦调整因子
     */
    double calculateFrictionFactor() const;
};

} // namespace aurora::gait

#endif // VELOCITY_LOCOMOTION_CONTROLLER_H
