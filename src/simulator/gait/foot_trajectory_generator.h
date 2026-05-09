// foot_trajectory_generator.h - Unitree风格的足端轨迹生成器
// 参考：Unitree四足机器人足端轨迹规划方法
//
// 核心设计原则：
// 1. 摆动轨迹：平滑的抬脚和落脚，使用三次样条或正弦组合
// 2. 支撑轨迹：足端固定在地面，躯干相对于足端移动
// 3. 可配置：支持动态调整步高、步长、步宽等参数
// 4. 约束检查：确保轨迹满足关节限位和稳定性要求

#ifndef FOOT_TRAJECTORY_GENERATOR_H
#define FOOT_TRAJECTORY_GENERATOR_H

#include "gait_state_machine.h"
#include <vector>
#include <memory>
#include <functional>
#include <cmath>

namespace aurora::gait {

// Forward declarations (to avoid circular dependency with ruckig_trajectory_adapter.h)
class RuckigTrajectoryAdapter;
struct RuckigTrajectoryConfig;

/**
 * @brief 轨迹类型
 */
enum class TrajectoryType : uint8_t {
    SWING,           // 摆动轨迹
    STANCE,          // 支撑轨迹
    TRANSITION,      // 转换轨迹（如坐下、站起）
    CUSTOM           // 自定义轨迹
};

/**
 * @brief 摆动轨迹样式
 */
enum class SwingTrajectoryStyle : uint8_t {
    SINUSOID,        // 正弦轨迹（平滑，经典）
    CUBIC_SPLINE,    // 三次样条（更灵活）
    QUINTIC,         // 五次多项式（端点速度/加速度为0）
    CYCLOID,         // 摆线轨迹（起落点速度为0）
    COMBO,           // 组合轨迹（Z用正弦，XY用样条）
    RUCKIG           // Ruckig实时轨迹规划（最优时间轨迹，<1ms）
};

/**
 * @brief 轨迹点
 */
struct TrajectoryPoint {
    FootPosition position;      // 位置
    FootVelocity velocity;      // 速度
    double time;                // 时间
    double progress;            // 进度 [0, 1]

    TrajectoryPoint()
        : position(), velocity(), time(0), progress(0)
    {}

    TrajectoryPoint(const FootPosition& pos, double t, double prog = 0)
        : position(pos), velocity(), time(t), progress(prog)
    {}

    TrajectoryPoint(const FootPosition& pos, const FootVelocity& vel, double t, double prog = 0)
        : position(pos), velocity(vel), time(t), progress(prog)
    {}
};

/**
 * @brief 轨迹约束
 */
struct TrajectoryConstraints {
    double max_foot_height;         // 最大足端高度
    double min_foot_height;         // 最小足端高度
    double max_forward_reach;       // 最大前伸距离
    double max_backward_reach;      // 最大后伸距离
    double max_lateral_reach;       // 最大侧伸距离
    double min_ground_clearance;    // 最小离地间隙

    FootPosition bounding_box_min;  // 边界框最小值
    FootPosition bounding_box_max;  // 边界框最大值

    TrajectoryConstraints()
        : max_foot_height(0.15)
        , min_foot_height(-0.05)
        , max_forward_reach(0.4)
        , max_backward_reach(-0.3)
        , max_lateral_reach(0.2)
        , min_ground_clearance(0.02)
        , bounding_box_min(-0.3, -0.2, -0.7)
        , bounding_box_max(0.3, 0.2, 0.1)
    {}

    /**
     * @brief 检查位置是否满足约束
     */
    bool checkPosition(const FootPosition& pos) const {
        if (pos.z < min_foot_height || pos.z > max_foot_height) return false;
        if (pos.x < max_backward_reach || pos.x > max_forward_reach) return false;
        if (std::abs(pos.y) > max_lateral_reach) return false;

        if (pos.x < bounding_box_min.x || pos.x > bounding_box_max.x) return false;
        if (pos.y < bounding_box_min.y || pos.y > bounding_box_max.y) return false;
        if (pos.z < bounding_box_min.z || pos.z > bounding_box_max.z) return false;

        return true;
    }

    /**
     * @brief 裁剪位置到约束范围内
     */
    FootPosition clipPosition(const FootPosition& pos) const {
        FootPosition result = pos;
        result.x = std::clamp(result.x, bounding_box_min.x, bounding_box_max.x);
        result.y = std::clamp(result.y, bounding_box_min.y, bounding_box_max.y);
        result.z = std::clamp(result.z, bounding_box_min.z, bounding_box_max.z);
        return result;
    }
};

/**
 * @brief 足端轨迹生成器配置
 */
struct FootTrajectoryGeneratorConfig {
    SwingTrajectoryStyle swing_style;    // 摆动轨迹样式
    double trajectory_resolution;        // 轨迹分辨率（每步采样点数）
    bool use_velocity_profile;           // 是否使用速度剖面
    bool enable_smoothing;               // 是否启用轨迹平滑
    double smoothing_factor;             // 平滑因子
    bool check_constraints;              // 是否检查约束
    TrajectoryConstraints constraints;   // 轨迹约束

    FootTrajectoryGeneratorConfig()
        : swing_style(SwingTrajectoryStyle::COMBO)
        , trajectory_resolution(50)
        , use_velocity_profile(true)
        , enable_smoothing(true)
        , smoothing_factor(0.5)
        , check_constraints(true)
        , constraints()
    {}
};

/**
 * @brief 摆动轨迹参数
 */
struct SwingTrajectoryParams {
    FootPosition start;         // 起始位置
    FootPosition end;           // 目标位置
    FootPosition mid;           // 中间点（用于样条）
    double height;              // 最大抬脚高度
    double distance;            // 水平移动距离
    double duration;            // 持续时间
    double apex_ratio;          // 顶点位置比例 [0, 1]
    double lift_ratio;          // 抬起比例
    double land_ratio;          // 落地比例

    SwingTrajectoryParams()
        : start()
        , end()
        , mid()
        , height(0.05)
        , distance(0)
        , duration(0.32)
        , apex_ratio(0.5)
        , lift_ratio(0.2)
        , land_ratio(0.2)
    {
        // 默认中点在起止点中间上方
        mid = FootPosition(0, 0, height);
    }

    /**
     * @brief 从起始和目标位置计算参数
     */
    static SwingTrajectoryParams fromEndpoints(
        const FootPosition& start_pos,
        const FootPosition& end_pos,
        double step_height,
        double step_duration,
        double apex = 0.5)
    {
        SwingTrajectoryParams params;
        params.start = start_pos;
        params.end = end_pos;
        params.height = step_height;
        params.duration = step_duration;
        params.apex_ratio = apex;

        // 计算中点
        params.mid = FootPosition(
            (start_pos.x + end_pos.x) / 2.0,
            (start_pos.y + end_pos.y) / 2.0,
            std::max(start_pos.z, end_pos.z) + step_height
        );

        // 计算水平距离
        double dx = end_pos.x - start_pos.x;
        double dy = end_pos.y - start_pos.y;
        params.distance = std::sqrt(dx * dx + dy * dy);

        return params;
    }
};

/**
 * @brief 支撑轨迹参数
 */
struct StanceTrajectoryParams {
    FootPosition fixed_position;   // 固定位置（世界坐标）
    FootPosition body_offset;      // 躯干相对足端的偏移变化
    double duration;               // 持续时间
    double load_factor;            // 承重因子

    StanceTrajectoryParams()
        : fixed_position()
        , body_offset()
        , duration(0.48)
        , load_factor(1.0)
    {}
};

/**
 * @brief 足端轨迹生成器
 *
 * 生成单条腿的足端轨迹
 */
class FootTrajectoryGenerator {
public:
    explicit FootTrajectoryGenerator(
        const FootTrajectoryGeneratorConfig& config = {});

    ~FootTrajectoryGenerator() = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const FootTrajectoryGeneratorConfig& config);

    /**
     * @brief 获取配置
     */
    const FootTrajectoryGeneratorConfig& getConfig() const;

    /**
     * @brief 生成摆动轨迹
     * @param params 摆动轨迹参数
     * @return 轨迹点序列
     */
    std::vector<TrajectoryPoint> generateSwingTrajectory(
        const SwingTrajectoryParams& params) const;

    /**
     * @brief 生成支撑轨迹
     * @param params 支撑轨迹参数
     * @return 轨迹点序列
     */
    std::vector<TrajectoryPoint> generateStanceTrajectory(
        const StanceTrajectoryParams& params) const;

    /**
     * @brief 计算摆动轨迹上的单点（实时计算）
     * @param params 摆动轨迹参数
     * @param progress 进度 [0, 1]
     * @return 轨迹点
     */
    TrajectoryPoint computeSwingPoint(
        const SwingTrajectoryParams& params,
        double progress) const;

    /**
     * @brief 计算支撑轨迹上的单点（实时计算）
     * @param params 支撑轨迹参数
     * @param time 时间
     * @return 轨迹点
     */
    TrajectoryPoint computeStancePoint(
        const StanceTrajectoryParams& params,
        double time) const;

    /**
     * @brief 设置约束
     */
    void setConstraints(const TrajectoryConstraints& constraints);

    /**
     * @brief 获取约束
     */
    const TrajectoryConstraints& getConstraints() const;

    /**
     * @brief 设置Ruckig配置（当swing_style为RUCKIG时使用）
     */
    void setRuckigConfig(const RuckigTrajectoryConfig& config);

    /**
     * @brief 获取Ruckig配置
     */
    const RuckigTrajectoryConfig& getRuckigConfig() const;

    /**
     * @brief 获取Ruckig适配器（用于高级配置）
     */
    std::shared_ptr<RuckigTrajectoryAdapter> getRuckigAdapter();

    /**
     * @brief 检查轨迹是否满足约束
     */
    bool checkTrajectory(const std::vector<TrajectoryPoint>& trajectory) const;

    /**
     * @brief 裁剪轨迹到约束范围内
     */
    std::vector<TrajectoryPoint> clipTrajectory(
        std::vector<TrajectoryPoint> trajectory) const;

    /**
     * @brief 平滑轨迹
     */
    std::vector<TrajectoryPoint> smoothTrajectory(
        const std::vector<TrajectoryPoint>& trajectory) const;

    /**
     * @brief 计算轨迹长度
     */
    static double computeTrajectoryLength(
        const std::vector<TrajectoryPoint>& trajectory);

    /**
     * @brief 计算轨迹持续时间
     */
    static double computeTrajectoryDuration(
        const std::vector<TrajectoryPoint>& trajectory);

private:
    FootTrajectoryGeneratorConfig config_;
    mutable std::mutex mutex_;

    // 正弦轨迹生成
    TrajectoryPoint generateSinusoidSwingPoint(
        const SwingTrajectoryParams& params,
        double progress) const;

    // 三次样条轨迹生成
    TrajectoryPoint generateCubicSplineSwingPoint(
        const SwingTrajectoryParams& params,
        double progress) const;

    // 五次多项式轨迹生成
    TrajectoryPoint generateQuinticSwingPoint(
        const SwingTrajectoryParams& params,
        double progress) const;

    // 摆线轨迹生成
    TrajectoryPoint generateCycloidSwingPoint(
        const SwingTrajectoryParams& params,
        double progress) const;

    // 组合轨迹生成
    TrajectoryPoint generateComboSwingPoint(
        const SwingTrajectoryParams& params,
        double progress) const;

    // Ruckig轨迹生成
    TrajectoryPoint generateRuckigSwingPoint(
        const SwingTrajectoryParams& params,
        double progress) const;

    // 辅助函数
    double cubicHermiteInterpolate(double p0, double p1, double v0, double v1, double t) const;
    double quinticInterpolate(double p0, double p1, double t) const;

    // Ruckig适配器（延迟初始化）
    // 使用指针以允许前向声明，避免循环依赖
    mutable std::shared_ptr<RuckigTrajectoryAdapter> ruckig_adapter_;
    mutable std::unique_ptr<RuckigTrajectoryConfig> ruckig_config_;
    mutable bool ruckig_initialized_;
};

/**
 * @brief 多腿轨迹生成器
 *
 * 协调多条腿的轨迹生成
 */
class MultiLegTrajectoryGenerator {
public:
    explicit MultiLegTrajectoryGenerator(
        const FootTrajectoryGeneratorConfig& config = {});

    ~MultiLegTrajectoryGenerator() = default;

    /**
     * @brief 生成所有腿的摆动轨迹
     */
    std::vector<std::vector<TrajectoryPoint>> generateAllSwingTrajectories(
        const std::vector<SwingTrajectoryParams>& params) const;

    /**
     * @brief 生成所有腿的支撑轨迹
     */
    std::vector<std::vector<TrajectoryPoint>> generateAllStanceTrajectories(
        const std::vector<StanceTrajectoryParams>& params) const;

    /**
     * @brief 获取指定腿的轨迹生成器
     */
    std::shared_ptr<FootTrajectoryGenerator> getLegGenerator(LegID leg_id);

    /**
     * @brief 获取所有轨迹生成器
     */
    const std::vector<std::shared_ptr<FootTrajectoryGenerator>>& getGenerators() const;

private:
    std::vector<std::shared_ptr<FootTrajectoryGenerator>> generators_;
    mutable std::mutex mutex_;
};

/**
 * @brief 轨迹可视化辅助类
 */
class TrajectoryVisualizer {
public:
    /**
     * @brief 生成轨迹的字符串表示
     */
    static std::string trajectoryToString(
        const std::vector<TrajectoryPoint>& trajectory,
        const std::string& prefix = "  ");

    /**
     * @brief 生成轨迹的CSV格式
     */
    static std::string trajectoryToCSV(
        const std::vector<TrajectoryPoint>& trajectory);

    /**
     * @brief 打印轨迹信息
     */
    static void printTrajectoryInfo(
        const std::vector<TrajectoryPoint>& trajectory);
};

} // namespace aurora::gait

#endif // FOOT_TRAJECTORY_GENERATOR_H
