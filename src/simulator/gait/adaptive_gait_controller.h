// adaptive_gait_controller.h - 自适应步态控制器
// 根据速度、地形、坡度等环境因素动态调整步态参数
//
// 核心功能：
// 1. 速度自适应 - 根据移动速度调整步频和步长
// 2. 转向自适应 - 根据角速度调整占空比
// 3. 坡度自适应 - 根据坡度调整步态
// 4. 地形自适应 - 根据粗糙度调整步高和步频

#ifndef ADAPTIVE_GAIT_CONTROLLER_H
#define ADAPTIVE_GAIT_CONTROLLER_H

#include "gait_state_machine.h"
#include "velocity_locomotion_controller.h"
#include "ground_contact_model.h"
#include <memory>
#include <mutex>
#include <functional>
#include <cmath>

namespace aurora::gait {

/**
 * @brief 环境因素
 */
struct EnvironmentFactors {
    double velocity_factor;      // 速度因子 [0, 1]
    double turn_factor;          // 转向因子 [0, 1]
    double slope_factor;         // 坡度因子 [-1, 1]（负值下坡，正值上坡）
    double roughness_factor;     // 粗糙度因子 [0, 1]
    double friction_factor;      // 摩擦因子 [0, 1]

    EnvironmentFactors()
        : velocity_factor(0.0)
        , turn_factor(0.0)
        , slope_factor(0.0)
        , roughness_factor(0.0)
        , friction_factor(1.0)
    {}
};

/**
 * @brief 速度自适应配置
 */
struct VelocityAdaptationConfig {
    bool enabled;
    double min_frequency;
    double max_frequency;
    double max_step_length;
    double frequency_factor;    // frequency = base + velocity * factor
    double step_length_factor;  // step_length scale factor

    VelocityAdaptationConfig()
        : enabled(true)
        , min_frequency(0.8)
        , max_frequency(2.0)
        , max_step_length(0.4)
        , frequency_factor(0.5)
        , step_length_factor(1.0)
    {}
};

/**
 * @brief 转向自适应配置
 */
struct TurnAdaptationConfig {
    bool enabled;
    double duty_factor_adjustment;  // 占空比调整系数
    double min_duty_factor;
    double max_duty_factor;

    TurnAdaptationConfig()
        : enabled(true)
        , duty_factor_adjustment(0.1)
        , min_duty_factor(0.5)
        , max_duty_factor(0.75)
    {}
};

/**
 * @brief 坡度自适应配置
 */
struct SlopeAdaptationConfig {
    bool enabled;
    double duty_factor_adjustment;  // 占空比调整系数
    double max_duty_factor;
    double min_duty_factor;
    double step_height_adjustment;  // 步高调整系数
    double max_slope_angle;         // 最大坡度（弧度）

    SlopeAdaptationConfig()
        : enabled(true)
        , duty_factor_adjustment(0.15)
        , max_duty_factor(0.75)
        , min_duty_factor(0.5)
        , step_height_adjustment(0.03)
        , max_slope_angle(M_PI / 4)  // 45度
    {}
};

/**
 * @brief 地形自适应配置
 */
struct TerrainAdaptationConfig {
    bool enabled;
    double min_roughness;
    double max_roughness;
    double min_step_height;
    double max_step_height;
    double step_height_factor;
    double frequency_adjustment;   // 步频调整（负值表示粗糙地形降低步频）
    double max_frequency_reduction;

    TerrainAdaptationConfig()
        : enabled(true)
        , min_roughness(0.0)
        , max_roughness(1.0)
        , min_step_height(0.05)
        , max_step_height(0.15)
        , step_height_factor(0.1)
        , frequency_adjustment(-0.3)
        , max_frequency_reduction(0.5)
    {}
};

/**
 * @brief 自适应步态控制器配置
 */
struct AdaptiveGaitControllerConfig {
    VelocityAdaptationConfig velocity_config;
    TurnAdaptationConfig turn_config;
    SlopeAdaptationConfig slope_config;
    TerrainAdaptationConfig terrain_config;

    // 基础步态参数
    GaitParameters base_gait_params;

    // 启用/禁用所有自适应
    bool enabled;

    AdaptiveGaitControllerConfig()
        : velocity_config()
        , turn_config()
        , slope_config()
        , terrain_config()
        , base_gait_params()
        , enabled(false)
    {}
};

/**
 * @brief 自适应步态控制器
 *
 * 根据环境因素动态调整步态参数
 */
class AdaptiveGaitController {
public:
    explicit AdaptiveGaitController(
        const AdaptiveGaitControllerConfig& config = {});

    ~AdaptiveGaitController() = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const AdaptiveGaitControllerConfig& config);

    /**
     * @brief 获取配置
     */
    const AdaptiveGaitControllerConfig& getConfig() const;

    /**
     * @brief 设置基础步态参数
     */
    void setBaseGaitParameters(const GaitParameters& params);

    /**
     * @brief 更新环境因素
     */
    void updateEnvironmentFactors(const EnvironmentFactors& factors);

    /**
     * @brief 计算自适应后的步态参数
     * @return 调整后的步态参数
     */
    GaitParameters computeAdaptiveGait() const;

    /**
     * @brief 应用速度自适应
     */
    GaitParameters applyVelocityAdaptation(
        const GaitParameters& base,
        double velocity_factor) const;

    /**
     * @brief 应用转向自适应
     */
    GaitParameters applyTurnAdaptation(
        const GaitParameters& base,
        double turn_factor) const;

    /**
     * @brief 应用坡度自适应
     */
    GaitParameters applySlopeAdaptation(
        const GaitParameters& base,
        double slope_factor) const;

    /**
     * @brief 应用地形自适应
     */
    GaitParameters applyTerrainAdaptation(
        const GaitParameters& base,
        double roughness_factor) const;

    /**
     * @brief 获取当前环境因素
     */
    EnvironmentFactors getCurrentEnvironmentFactors() const;

    /**
     * @brief 重置控制器
     */
    void reset();

    /**
     * @brief 获取适应性评分 [0, 1]
     * 表示当前环境对步行的适应性
     */
    double getAdaptationScore() const;

private:
    AdaptiveGaitControllerConfig config_;
    EnvironmentFactors environment_factors_;
    mutable std::mutex mutex_;

    /**
     * @brief 计算综合适应性评分
     */
    double computeAdaptationScore() const;
};

/**
 * @brief 自适应步态调节器
 *
 * 自动调节步态参数以优化步行的稳定性和效率
 */
class GaitAutoTuner {
public:
    explicit GaitAutoTuner();

    /**
     * @brief 根据接触反馈自动调优步态参数
     * @param current_params 当前步态参数
     * @param contact_info 接触信息
     * @return 调优后的步态参数
     */
    GaitParameters tuneFromContactFeedback(
        const GaitParameters& current_params,
        const ContactInfo& contact_info);

    /**
     * @brief 根据打滑事件调整步态
     */
    GaitParameters adjustForSlip(
        const GaitParameters& current_params);

    /**
     * @brief 根据稳定性调整步态
     */
    GaitParameters adjustForStability(
        const GaitParameters& current_params,
        double stability_margin);

private:
    // 学习参数
    double slip_count_;
    double stability_violation_count_;
    mutable std::mutex mutex_;

    /**
     * @brief 计算推荐步高调整
     */
    double computeStepHeightAdjustment() const;
};

/**
 * @brief 动态步态规划器
 *
 * 结合当前状态和环境因素规划最优步态
 */
class DynamicGaitPlanner {
public:
    explicit DynamicGaitPlanner(
        const AdaptiveGaitControllerConfig& config = {});

    /**
     * @brief 规划下一步态参数
     * @param current_velocity 当前速度
     * @param target_velocity 目标速度
     * @param environment 环境因素
     * @return 规划的步态参数
     */
    GaitParameters planNextGait(
        const VelocityCommand& current_velocity,
        const VelocityCommand& target_velocity,
        const EnvironmentFactors& environment);

    /**
     * @brief 规划平滑过渡步态
     * @param from 起始步态
     * @param to 目标步态
     * @param progress 过渡进度 [0, 1]
     * @return 过渡步态参数
     */
    static GaitParameters planTransition(
        const GaitParameters& from,
        const GaitParameters& to,
        double progress);

    /**
     * @brief 预测未来步态需求
     */
    GaitParameters predictFutureGait(
        const VelocityCommand& velocity,
        double look_ahead_time) const;

private:
    std::unique_ptr<AdaptiveGaitController> adaptive_controller_;
    mutable std::mutex mutex_;
};

} // namespace aurora::gait

#endif // ADAPTIVE_GAIT_CONTROLLER_H
