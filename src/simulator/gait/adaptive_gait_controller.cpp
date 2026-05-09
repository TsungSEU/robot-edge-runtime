// adaptive_gait_controller.cpp - 自适应步态控制器实现
// 根据环境因素动态调整步态参数

#include "adaptive_gait_controller.h"
#include <algorithm>
#include <cmath>

namespace aurora::gait {

// ============================================================================
// AdaptiveGaitController 实现
// ============================================================================

AdaptiveGaitController::AdaptiveGaitController(
    const AdaptiveGaitControllerConfig& config)
    : config_(config)
    , environment_factors_()
{
}

void AdaptiveGaitController::setConfig(
    const AdaptiveGaitControllerConfig& config) {

    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const AdaptiveGaitControllerConfig& AdaptiveGaitController::getConfig() const {
    return config_;
}

void AdaptiveGaitController::setBaseGaitParameters(
    const GaitParameters& params) {

    std::lock_guard<std::mutex> lock(mutex_);
    config_.base_gait_params = params;
}

void AdaptiveGaitController::updateEnvironmentFactors(
    const EnvironmentFactors& factors) {

    std::lock_guard<std::mutex> lock(mutex_);
    environment_factors_ = factors;
}

GaitParameters AdaptiveGaitController::computeAdaptiveGait() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.enabled) {
        return config_.base_gait_params;
    }

    // 从基础参数开始
    GaitParameters params = config_.base_gait_params;

    // 依次应用各种自适应
    params = applyVelocityAdaptation(params, environment_factors_.velocity_factor);
    params = applyTurnAdaptation(params, environment_factors_.turn_factor);
    params = applySlopeAdaptation(params, environment_factors_.slope_factor);
    params = applyTerrainAdaptation(params, environment_factors_.roughness_factor);

    return params;
}

GaitParameters AdaptiveGaitController::applyVelocityAdaptation(
    const GaitParameters& base,
    double velocity_factor) const {

    if (!config_.velocity_config.enabled) {
        return base;
    }

    GaitParameters params = base;

    // 调整步频：频率 = min_freq + velocity * factor
    params.step_frequency = std::clamp(
        config_.velocity_config.min_frequency +
        velocity_factor * config_.velocity_config.frequency_factor,
        config_.velocity_config.min_frequency,
        config_.velocity_config.max_frequency
    );

    // 调整步长
    if (velocity_factor > 0.1) {
        params.step_length = std::min(
            params.step_length * (1.0 + velocity_factor * 0.5),
            config_.velocity_config.max_step_length
        );
    }

    // 更新支撑/摆动时长
    params.updateDurationsFromDutyFactor();

    return params;
}

GaitParameters AdaptiveGaitController::applyTurnAdaptation(
    const GaitParameters& base,
    double turn_factor) const {

    if (!config_.turn_config.enabled) {
        return base;
    }

    GaitParameters params = base;

    // 转向时减少占空比以提高稳定性
    double duty_reduction = turn_factor * config_.turn_config.duty_factor_adjustment;
    params.duty_factor = std::clamp(
        params.duty_factor - duty_reduction,
        config_.turn_config.min_duty_factor,
        config_.turn_config.max_duty_factor
    );

    // 更新支撑/摆动时长
    params.updateDurationsFromDutyFactor();

    return params;
}

GaitParameters AdaptiveGaitController::applySlopeAdaptation(
    const GaitParameters& base,
    double slope_factor) const {

    if (!config_.slope_config.enabled) {
        return base;
    }

    GaitParameters params = base;

    // 坡度因子范围 [-1, 1]
    // 负值 = 下坡，正值 = 上坡

    if (slope_factor > 0) {
        // 上坡：增加占空比以提高稳定性
        double duty_increase = slope_factor * config_.slope_config.duty_factor_adjustment;
        params.duty_factor = std::clamp(
            params.duty_factor + duty_increase,
            config_.slope_config.min_duty_factor,
            config_.slope_config.max_duty_factor
        );

        // 上坡可能需要增加步高
        params.step_height += slope_factor * config_.slope_config.step_height_adjustment;
    } else {
        // 下坡：适当减少占空比，增加步高以防绊倒
        double duty_decrease = std::abs(slope_factor) * config_.slope_config.duty_factor_adjustment * 0.5;
        params.duty_factor = std::clamp(
            params.duty_factor - duty_decrease,
            config_.slope_config.min_duty_factor,
            config_.slope_config.max_duty_factor
        );

        params.step_height += std::abs(slope_factor) * config_.slope_config.step_height_adjustment * 0.5;
    }

    // 更新支撑/摆动时长
    params.updateDurationsFromDutyFactor();

    return params;
}

GaitParameters AdaptiveGaitController::applyTerrainAdaptation(
    const GaitParameters& base,
    double roughness_factor) const {

    if (!config_.terrain_config.enabled) {
        return base;
    }

    GaitParameters params = base;

    // 粗糙地形：增加步高，降低步频
    if (roughness_factor > 0.3) {
        // 增加步高
        double height_increase = roughness_factor * config_.terrain_config.step_height_factor;
        params.step_height = std::clamp(
            base.step_height + height_increase,
            config_.terrain_config.min_step_height,
            config_.terrain_config.max_step_height
        );

        // 降低步频
        double freq_reduction = std::min(
            roughness_factor * std::abs(config_.terrain_config.frequency_adjustment),
            config_.terrain_config.max_frequency_reduction
        );
        params.step_frequency = std::max(
            params.step_frequency * (1.0 - freq_reduction),
            0.5  // 最低频率
        );

        // 更新支撑/摆动时长
        params.updateDurationsFromDutyFactor();
    }

    return params;
}

EnvironmentFactors AdaptiveGaitController::getCurrentEnvironmentFactors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return environment_factors_;
}

void AdaptiveGaitController::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    environment_factors_ = EnvironmentFactors();
}

double AdaptiveGaitController::getAdaptationScore() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return computeAdaptationScore();
}

double AdaptiveGaitController::computeAdaptationScore() const {
    // 综合评分基于环境因素

    // 速度评分：适中速度最好
    double velocity_score = 1.0 - std::abs(environment_factors_.velocity_factor - 0.5) * 2.0;
    velocity_score = std::clamp(velocity_score, 0.0, 1.0);

    // 转向评分：转向越少越好
    double turn_score = 1.0 - environment_factors_.turn_factor;
    turn_score = std::clamp(turn_score, 0.0, 1.0);

    // 坡度评分：平坦地面最好
    double slope_score = 1.0 - std::abs(environment_factors_.slope_factor);
    slope_score = std::clamp(slope_score, 0.0, 1.0);

    // 地形评分：越平坦越好
    double terrain_score = 1.0 - environment_factors_.roughness_factor;
    terrain_score = std::clamp(terrain_score, 0.0, 1.0);

    // 摩擦评分：摩擦越大越好（但要适中）
    double friction_score = environment_factors_.friction_factor;

    // 综合评分
    return (velocity_score + turn_score + slope_score + terrain_score + friction_score) / 5.0;
}

// ============================================================================
// GaitAutoTuner 实现
// ============================================================================

GaitAutoTuner::GaitAutoTuner()
    : slip_count_(0.0)
    , stability_violation_count_(0.0)
{
}

GaitParameters GaitAutoTuner::tuneFromContactFeedback(
    const GaitParameters& current_params,
    const ContactInfo& contact_info) {

    std::lock_guard<std::mutex> lock(mutex_);

    GaitParameters params = current_params;

    // 检测打滑
    if (contact_info.is_slipping) {
        slip_count_ += 1.0;

        // 打滑时增加占空比，减小步长
        params.duty_factor = std::min(params.duty_factor + 0.05, 0.75);
        params.step_length = std::max(params.step_length * 0.9, 0.15);
        params.updateDurationsFromDutyFactor();
    } else {
        // 慢慢恢复
        slip_count_ = std::max(0.0, slip_count_ - 0.01);
    }

    // 检测冲击
    if (contact_info.state == ContactState::IMPACT) {
        // 冲击大时增加步高
        params.step_height = std::min(params.step_height + 0.01, 0.15);
    }

    return params;
}

GaitParameters GaitAutoTuner::adjustForSlip(
    const GaitParameters& current_params) {

    std::lock_guard<std::mutex> lock(mutex_);

    GaitParameters params = current_params;

    if (slip_count_ > 5.0) {
        // 频繁打滑，显著调整
        params.duty_factor = std::min(params.duty_factor + 0.1, 0.75);
        params.step_length = std::max(params.step_length * 0.8, 0.1);
        params.step_height = std::min(params.step_height + 0.02, 0.15);
        params.updateDurationsFromDutyFactor();

        slip_count_ = 0.0;  // 重置计数
    }

    return params;
}

GaitParameters GaitAutoTuner::adjustForStability(
    const GaitParameters& current_params,
    double stability_margin) {

    std::lock_guard<std::mutex> lock(mutex_);

    GaitParameters params = current_params;

    // 稳定性不足时调整
    if (stability_margin < 0.03) {  // 3cm
        stability_violation_count_ += 1.0;

        if (stability_violation_count_ > 10.0) {
            // 增加占空比，降低步频，增加步高
            params.duty_factor = std::min(params.duty_factor + 0.1, 0.8);
            params.step_frequency = std::max(params.step_frequency * 0.9, 0.8);
            params.step_height = std::min(params.step_height + 0.02, 0.15);
            params.updateDurationsFromDutyFactor();

            stability_violation_count_ = 0.0;
        }
    } else {
        stability_violation_count_ = std::max(0.0, stability_violation_count_ - 0.1);
    }

    return params;
}

double GaitAutoTuner::computeStepHeightAdjustment() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 基于打滑和稳定性问题推荐步高调整
    double adjustment = 0.0;

    if (slip_count_ > 3.0) {
        adjustment += 0.02;
    }

    if (stability_violation_count_ > 5.0) {
        adjustment += 0.01;
    }

    return std::clamp(adjustment, 0.0, 0.05);
}

// ============================================================================
// DynamicGaitPlanner 实现
// ============================================================================

DynamicGaitPlanner::DynamicGaitPlanner(
    const AdaptiveGaitControllerConfig& config)
    : adaptive_controller_(std::make_unique<AdaptiveGaitController>(config))
{
}

GaitParameters DynamicGaitPlanner::planNextGait(
    const VelocityCommand& current_velocity,
    const VelocityCommand& target_velocity,
    const EnvironmentFactors& environment) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 更新环境因素
    adaptive_controller_->updateEnvironmentFactors(environment);

    // 计算目标速度
    VelocityCommand planned_velocity = target_velocity;

    // 考虑环境因素调整速度
    if (environment.slope_factor > 0.5) {
        // 上坡减速
        planned_velocity.forward *= 0.7;
    } else if (environment.slope_factor < -0.5) {
        // 下坡适当减速保持安全
        planned_velocity.forward *= 0.8;
    }

    if (environment.roughness_factor > 0.5) {
        // 粗糙地形减速
        planned_velocity.forward *= 0.6;
    }

    if (environment.friction_factor < 0.5) {
        // 低摩擦环境减速
        planned_velocity.forward *= 0.7;
        planned_velocity.angular *= 0.7;
    }

    // 计算速度因子
    double v_horizontal = planned_velocity.horizontalNorm();
    double velocity_factor = std::clamp(v_horizontal, 0.0, 1.0);

    // 计算转向因子
    double turn_factor = std::clamp(std::abs(planned_velocity.angular), 0.0, 1.0);

    // 更新环境因素用于自适应
    EnvironmentFactors adaptive_factors = environment;
    adaptive_factors.velocity_factor = velocity_factor;
    adaptive_factors.turn_factor = turn_factor;

    adaptive_controller_->updateEnvironmentFactors(adaptive_factors);

    // 获取自适应步态
    return adaptive_controller_->computeAdaptiveGait();
}

GaitParameters DynamicGaitPlanner::planTransition(
    const GaitParameters& from,
    const GaitParameters& to,
    double progress) {

    // 裁剪进度
    progress = std::clamp(progress, 0.0, 1.0);

    GaitParameters result;

    // 使用平滑插值（sigmoid）
    double alpha = progress * progress * (3.0 - 2.0 * progress);

    result.step_frequency = from.step_frequency + alpha * (to.step_frequency - from.step_frequency);
    result.step_height = from.step_height + alpha * (to.step_height - from.step_height);
    result.step_length = from.step_length + alpha * (to.step_length - from.step_length);
    result.duty_factor = from.duty_factor + alpha * (to.duty_factor - from.duty_factor);
    result.swing_apex_ratio = from.swing_apex_ratio + alpha * (to.swing_apex_ratio - from.swing_apex_ratio);

    // 更新支撑/摆动时长
    result.updateDurationsFromDutyFactor();

    return result;
}

GaitParameters DynamicGaitPlanner::predictFutureGait(
    const VelocityCommand& velocity,
    double look_ahead_time) const {

    std::lock_guard<std::mutex> lock(mutex_);

    // 简化实现：基于当前速度预测
    // 更复杂的实现可以考虑加速度、地形变化等

    GaitParameters params = adaptive_controller_->getConfig().base_gait_params;

    double v_horizontal = velocity.horizontalNorm();
    double velocity_factor = std::clamp(v_horizontal, 0.0, 1.0);

    // 预测：速度会继续增加
    double predicted_velocity = std::min(velocity_factor + look_ahead_time * 0.5, 1.0);

    // 应用速度自适应
    AdaptiveGaitController* non_const_controller =
        const_cast<AdaptiveGaitController*>(adaptive_controller_.get());

    EnvironmentFactors factors;
    factors.velocity_factor = predicted_velocity;
    factors.turn_factor = std::clamp(std::abs(velocity.angular), 0.0, 1.0);

    non_const_controller->updateEnvironmentFactors(factors);

    return non_const_controller->computeAdaptiveGait();
}

} // namespace aurora::gait
