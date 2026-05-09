// velocity_locomotion_controller.cpp - 速度控制步行控制器实现
// 将速度命令转换为步态参数

#include "velocity_locomotion_controller.h"
#include <algorithm>
#include <cmath>

namespace aurora::gait {

// ============================================================================
// VelocityLocomotionController 实现
// ============================================================================

VelocityLocomotionController::VelocityLocomotionController(
    const VelocityLocomotionControllerConfig& config)
    : config_(config)
    , current_velocity_()
    , target_velocity_()
    , velocity_smoother_(nullptr)
    , smoother_initialized_(false)
    , velocity_command_callback_()
    , gait_parameters_update_callback_()
{
}

void VelocityLocomotionController::setConfig(
    const VelocityLocomotionControllerConfig& config) {

    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    smoother_initialized_ = false;  // 需要重新初始化平滑器
}

const VelocityLocomotionControllerConfig& VelocityLocomotionController::getConfig()
    const {
    return config_;
}

void VelocityLocomotionController::setVelocityCommand(
    const VelocityCommand& command, bool immediate) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 裁剪速度到允许范围
    target_velocity_ = command.clamp(
        config_.max_forward,
        config_.max_lateral,
        config_.max_angular
    );

    // 应用速度缩放
    target_velocity_.forward *= config_.velocity_scale;
    target_velocity_.lateral *= config_.velocity_scale;
    target_velocity_.angular *= config_.velocity_scale;

    if (immediate || !config_.smoothing_config.enabled) {
        // 立即应用，跳过平滑
        current_velocity_ = target_velocity_;
        smoother_initialized_ = false;
    }

    // 触发回调
    triggerVelocityCommandCallback(target_velocity_);
}

VelocityCommand VelocityLocomotionController::getCurrentVelocityCommand() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_velocity_;
}

VelocityCommand VelocityLocomotionController::getTargetVelocityCommand() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return target_velocity_;
}

void VelocityLocomotionController::stop() {
    setVelocityCommand(VelocityCommand(0, 0, 0), false);
}

GaitParameters VelocityLocomotionController::update(double dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 平滑速度
    current_velocity_ = smoothVelocity(target_velocity_, dt);

    // 转换为步态参数
    GaitParameters params = velocityToGaitParameters(current_velocity_);

    // 触发回调
    triggerGaitParametersUpdateCallback(params);

    return params;
}

GaitParameters VelocityLocomotionController::velocityToGaitParameters(
    const VelocityCommand& velocity) const {

    const auto& converter_config = config_.converter_config;

    GaitParameters params;
    params.step_height = 0.05;  // 默认步高

    // 计算水平速度
    double v_horizontal = velocity.horizontalNorm();

    // 如果速度接近零，返回站立参数
    if (v_horizontal < 1e-6 && std::abs(velocity.angular) < 1e-6) {
        params.step_frequency = 0.0;
        params.step_length = 0.0;
        params.duty_factor = 1.0;
        params.stance_duration = 0.0;
        params.swing_duration = 0.0;
        return params;
    }

    // 调整步频（0.8 - 2.0 Hz based on velocity）
    params.step_frequency = std::clamp(
        converter_config.min_frequency + v_horizontal * converter_config.velocity_frequency_factor,
        converter_config.min_frequency,
        converter_config.max_frequency
    );

    // 计算步长
    double step_length = v_horizontal / params.step_frequency;
    params.step_length = std::min(step_length, converter_config.max_step_length);

    // 调整步高（侧向运动增加步高）
    double lateral_factor = std::abs(velocity.lateral) * converter_config.lateral_height_factor;
    params.step_height = 0.05 + lateral_factor;

    // 调整占空比（转向时减少占空比）
    double turn_factor = std::abs(velocity.angular) * converter_config.turn_duty_factor;
    params.duty_factor = std::clamp(0.6 - turn_factor, 0.5, 0.75);

    // 计算支撑/摆动时长
    params.updateDurationsFromDutyFactor();

    return params;
}

void VelocityLocomotionController::setVelocityCommandCallback(
    VelocityCommandCallback callback) {

    std::lock_guard<std::mutex> lock(mutex_);
    velocity_command_callback_ = std::move(callback);
}

void VelocityLocomotionController::setGaitParametersUpdateCallback(
    GaitParametersUpdateCallback callback) {

    std::lock_guard<std::mutex> lock(mutex_);
    gait_parameters_update_callback_ = std::move(callback);
}

void VelocityLocomotionController::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_velocity_ = VelocityCommand();
    target_velocity_ = VelocityCommand();
    smoother_initialized_ = false;
}

bool VelocityLocomotionController::isMoving() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !current_velocity_.isZero(1e-3);
}

double VelocityLocomotionController::getMotionProgress() const {
    // 简化实现：基于当前速度相对于目标速度
    double current_norm = current_velocity_.horizontalNorm();
    double target_norm = target_velocity_.horizontalNorm();

    if (target_norm < 1e-6) {
        return current_norm < 1e-3 ? 1.0 : 0.0;
    }

    return std::clamp(current_norm / target_norm, 0.0, 1.0);
}

void VelocityLocomotionController::initializeVelocitySmoother() {
    if (smoother_initialized_) {
        return;
    }

    // 创建3-DOF Ruckig实例用于速度平滑
    RuckigTrajectoryConfig ruckig_config;
    ruckig_config.control_mode = RuckigControlMode::POSITION;
    ruckig_config.constraints.max_velocity = config_.max_forward;
    ruckig_config.constraints.max_acceleration = config_.smoothing_config.max_acceleration;
    ruckig_config.constraints.max_jerk = config_.smoothing_config.max_jerk;
    ruckig_config.enable_synchronization = false;  // 各维度独立平滑

    velocity_smoother_ = std::make_unique<RuckigTrajectoryAdapter>(ruckig_config);
    smoother_initialized_ = true;
}

VelocityCommand VelocityLocomotionController::smoothVelocity(
    const VelocityCommand& target, double dt) {

    if (!config_.smoothing_config.enabled) {
        return target;
    }

    if (!config_.smoothing_config.use_ruckig) {
        // 简单指数平滑
        double alpha = std::clamp(dt / config_.smoothing_config.smoothing_time, 0.0, 1.0);
        return VelocityCommand(
            current_velocity_.forward + alpha * (target.forward - current_velocity_.forward),
            current_velocity_.lateral + alpha * (target.lateral - current_velocity_.lateral),
            current_velocity_.angular + alpha * (target.angular - current_velocity_.angular)
        );
    }

    // 使用Ruckig平滑
    initializeVelocitySmoother();

    // 转换速度为位置（积分）
    // 注意：这里使用位置模式，所以将速度映射到位置
    FootPosition current_pos(current_velocity_.forward, current_velocity_.lateral, current_velocity_.angular);
    FootPosition target_pos(target.forward, target.lateral, target.angular);
    FootVelocity current_vel(0, 0, 0);  // 速度变化率，设为0

    // 计算轨迹
    velocity_smoother_->calculateMinimumTimeTrajectory(
        current_pos, current_vel, target_pos, FootVelocity(0, 0, 0)
    );

    // 获取平滑后的点
    TrajectoryPoint point = velocity_smoother_->getPointAtTime(std::min(dt, 0.1));

    return VelocityCommand(point.position.x, point.position.y, point.position.z);
}

void VelocityLocomotionController::triggerVelocityCommandCallback(
    const VelocityCommand& command) {

    if (velocity_command_callback_) {
        velocity_command_callback_(command);
    }
}

void VelocityLocomotionController::triggerGaitParametersUpdateCallback(
    const GaitParameters& params) {

    if (gait_parameters_update_callback_) {
        gait_parameters_update_callback_(params);
    }
}

// ============================================================================
// AdaptiveVelocityController 实现
// ============================================================================

AdaptiveVelocityController::AdaptiveVelocityController(
    const VelocityLocomotionControllerConfig& config)
    : mode_(VelocityControlMode::DIRECT)
    , config_(config)
    , slope_(0.0)
    , roughness_(0.0)
    , friction_(1.0)
{
}

void AdaptiveVelocityController::setControlMode(VelocityControlMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = mode;
}

VelocityControlMode AdaptiveVelocityController::getControlMode() const {
    return mode_;
}

void AdaptiveVelocityController::setEnvironmentFeedback(
    double slope, double roughness, double friction) {

    std::lock_guard<std::mutex> lock(mutex_);
    slope_ = std::clamp(slope, -M_PI / 4, M_PI / 4);  // 限制在±45度
    roughness_ = std::clamp(roughness, 0.0, 1.0);
    friction_ = std::clamp(friction, 0.1, 1.0);
}

VelocityCommand AdaptiveVelocityController::adjustVelocity(
    const VelocityCommand& command) const {

    if (mode_ == VelocityControlMode::DIRECT) {
        return command;
    }

    VelocityCommand adjusted = command;

    // 应用环境调整因子
    double slope_factor = calculateSlopeFactor();
    double roughness_factor = calculateRoughnessFactor();
    double friction_factor = calculateFrictionFactor();

    // 综合调整因子
    double total_factor = slope_factor * roughness_factor * friction_factor;

    // 调整速度
    adjusted.forward *= total_factor;
    adjusted.lateral *= total_factor;

    // 转向速度也受摩擦影响
    adjusted.angular *= friction_factor;

    return adjusted;
}

double AdaptiveVelocityController::getAdaptationScore() const {
    // 计算适应性评分 [0, 1]，越高表示环境越适合运动
    double slope_score = 1.0 - std::abs(slope_) / (M_PI / 4);
    double roughness_score = 1.0 - roughness_;
    double friction_score = friction_;

    return (slope_score + roughness_score + friction_score) / 3.0;
}

double AdaptiveVelocityController::calculateSlopeFactor() const {
    // 上坡减速，下坡适当加速但保持安全
    if (slope_ > 0) {
        // 上坡：每15度减少50%速度
        return std::max(0.3, 1.0 - (slope_ / (M_PI / 12)) * 0.5);
    } else {
        // 下坡：适当减速保持安全
        return std::max(0.5, 1.0 + (slope_ / (M_PI / 12)) * 0.3);
    }
}

double AdaptiveVelocityController::calculateRoughnessFactor() const {
    // 粗糙地形减速
    return std::max(0.3, 1.0 - roughness_ * 0.7);
}

double AdaptiveVelocityController::calculateFrictionFactor() const {
    // 低摩擦环境减速
    // 摩擦系数 < 0.3 时开始减速
    if (friction_ >= 0.6) {
        return 1.0;
    } else {
        return std::max(0.3, friction_ / 0.6);
    }
}

} // namespace aurora::gait
