// ruckig_trajectory_adapter.h - Ruckig轨迹生成器适配器
// 集成Ruckig实时轨迹规划库到Aurora步态系统
//
// 核心功能：
// 1. 3-DOF足端轨迹规划 (x, y, z)
// 2. 位置和速度控制模式
// 3. 实时计算 (<1ms)
// 4. 与现有轨迹类型保持兼容

#ifndef RUCKIG_TRAJECTORY_ADAPTER_H
#define RUCKIG_TRAJECTORY_ADAPTER_H

#include "gait_state_machine.h"
#include "foot_trajectory_generator.h"  // For TrajectoryPoint, SwingTrajectoryParams, etc.
#include <ruckig/ruckig.hpp>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>

namespace aurora::gait {

/**
 * @brief Ruckig轨迹控制模式
 */
enum class RuckigControlMode : uint8_t {
    POSITION,          // 位置控制模式 (位置、速度、加速度约束)
    VELOCITY,          // 速度控制模式 (速度、加速度、加加速度约束)
    POSITION_VELOCITY  // 混合模式 (位置目标 + 速度剖面)
};

/**
 * @brief Ruckig轨迹约束
 */
struct RuckigConstraints {
    double max_velocity;      // 最大速度 (米/秒)
    double max_acceleration;  // 最大加速度 (米/秒²)
    double max_jerk;          // 最大加加速度 (米/秒³)

    // 每个维度的独立约束 (可选)
    struct {
        double max_velocity;
        double max_acceleration;
        double max_jerk;
    } x, y, z;

    RuckigConstraints()
        : max_velocity(1.0)
        , max_acceleration(5.0)
        , max_jerk(100.0)
        , x{max_velocity, max_acceleration, max_jerk}
        , y{max_velocity, max_acceleration, max_jerk}
        , z{max_velocity / 2.0, max_acceleration, max_jerk}
    {}

    RuckigConstraints(double v_max, double a_max, double j_max)
        : max_velocity(v_max)
        , max_acceleration(a_max)
        , max_jerk(j_max)
        , x{v_max, a_max, j_max}
        , y{v_max, a_max, j_max}
        , z{v_max / 2.0, a_max, j_max}
    {}
};

/**
 * @brief Ruckig轨迹状态
 */
struct RuckigTrajectoryState {
    FootPosition current_position;      // 当前位置
    FootVelocity current_velocity;      // 当前速度
    FootPosition target_position;       // 目标位置
    FootVelocity target_velocity;       // 目标速度

    double current_time;                // 当前轨迹时间
    double duration;                    // 轨迹总持续时间
    bool is_finished;                   // 是否完成

    RuckigTrajectoryState()
        : current_position()
        , current_velocity()
        , target_position()
        , target_velocity()
        , current_time(0)
        , duration(0)
        , is_finished(false)
    {}
};

/**
 * @brief Ruckig轨迹生成器配置
 */
struct RuckigTrajectoryConfig {
    RuckigControlMode control_mode;     // 控制模式
    RuckigConstraints constraints;      // 轨迹约束
    double minimum_duration;            // 最小持续时间 (秒)
    double time_step;                   // 时间步长 (秒)

    bool enable_synchronization;        // 是否同步所有维度
    bool discontinuous_position;        // 是否允许位置不连续
    bool immediate_calculation;         // 是否立即计算

    RuckigTrajectoryConfig()
        : control_mode(RuckigControlMode::POSITION)
        , constraints()
        , minimum_duration(0.1)
        , time_step(0.001)  // 1kHz
        , enable_synchronization(true)
        , discontinuous_position(false)
        , immediate_calculation(true)
    {}
};

/**
 * @brief Ruckig轨迹生成器适配器
 *
 * 包装Ruckig API用于3-DOF足端轨迹生成
 */
class RuckigTrajectoryAdapter {
public:
    explicit RuckigTrajectoryAdapter(
        const RuckigTrajectoryConfig& config = {});

    ~RuckigTrajectoryAdapter() = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const RuckigTrajectoryConfig& config);

    /**
     * @brief 获取配置
     */
    const RuckigTrajectoryConfig& getConfig() const;

    /**
     * @brief 计算从当前位置到目标位置的最小时间轨迹
     * @param current_pos 当前位置
     * @param current_vel 当前速度
     * @param target_pos 目标位置
     * @param target_vel 目标速度 (可选)
     * @return 轨迹持续时间
     */
    double calculateMinimumTimeTrajectory(
        const FootPosition& current_pos,
        const FootVelocity& current_vel,
        const FootPosition& target_pos,
        const FootVelocity& target_vel = FootVelocity());

    /**
     * @brief 计算固定时间轨迹
     * @param current_pos 当前位置
     * @param current_vel 当前速度
     * @param target_pos 目标位置
     * @param target_vel 目标速度
     * @param duration 持续时间
     * @return 成功返回true
     */
    bool calculateFixedTimeTrajectory(
        const FootPosition& current_pos,
        const FootVelocity& current_vel,
        const FootPosition& target_pos,
        const FootVelocity& target_vel,
        double duration);

    /**
     * @brief 计算轨迹上的下一个点
     * @param dt 时间步长
     * @return 轨迹点
     */
    TrajectoryPoint getNextPoint(double dt);

    /**
     * @brief 获取轨迹在指定时间的点
     * @param time 时间
     * @return 轨迹点
     */
    TrajectoryPoint getPointAtTime(double time);

    /**
     * @brief 生成完整轨迹点序列
     * @param time_step 时间步长
     * @return 轨迹点序列
     */
    std::vector<TrajectoryPoint> generateTrajectoryPoints(double time_step = 0.001);

    /**
     * @brief 重置轨迹生成器
     */
    void reset();

    /**
     * @brief 设置控制模式
     */
    void setControlMode(RuckigControlMode mode);

    /**
     * @brief 设置约束
     */
    void setConstraints(const RuckigConstraints& constraints);

    /**
     * @brief 获取当前轨迹状态
     */
    const RuckigTrajectoryState& getTrajectoryState() const;

    /**
     * @brief 判断轨迹是否完成
     */
    bool isFinished() const;

    /**
     * @brief 获取轨迹进度 [0, 1]
     */
    double getProgress() const;

    /**
     * @brief 中断当前轨迹
     */
    void abort();

    /**
     * @brief 验证轨迹是否满足约束
     * @param trajectory 轨迹点序列
     * @return 全部满足返回true
     */
    bool validateTrajectory(const std::vector<TrajectoryPoint>& trajectory) const;

    /**
     * @brief 性能测试 - 计算轨迹生成时间
     * @param iterations 迭代次数
     * @return 平均计算时间 (秒)
     */
    double benchmarkCalculationTime(size_t iterations = 1000) const;

private:
    RuckigTrajectoryConfig config_;
    RuckigTrajectoryState state_;
    std::unique_ptr<ruckig::Ruckig<3>> ruckig_;
    ruckig::InputParameter<3> input_;
    ruckig::OutputParameter<3> output_;
    mutable std::mutex mutex_;

    /**
     * @brief 初始化Ruckig实例
     */
    void initializeRuckig();

    /**
     * @brief 将FootPosition转换为Ruckig输入向量
     */
    void footPositionToRuckigVector(
        const FootPosition& pos,
        const FootVelocity& vel,
        std::array<double, 3>& position,
        std::array<double, 3>& velocity) const;

    /**
     * @brief 从Ruckig输出向量转换为FootPosition
     */
    void ruckigVectorToFootPosition(
        const std::array<double, 3>& position,
        const std::array<double, 3>& velocity,
        FootPosition& pos,
        FootVelocity& vel) const;

    /**
     * @brief 应用约束到Ruckig输入参数
     */
    void applyConstraints(ruckig::InputParameter<3>& input) const;

    /**
     * @brief 更新轨迹状态
     */
    void updateTrajectoryState();
};

/**
 * @brief 多腿Ruckig轨迹协调器
 *
 * 协调多条腿的Ruckig轨迹生成
 */
class MultiLegRuckigCoordinator {
public:
    explicit MultiLegRuckigCoordinator(
        const RuckigTrajectoryConfig& config = {});

    ~MultiLegRuckigCoordinator() = default;

    /**
     * @brief 设置腿数量
     */
    void setNumLegs(size_t num_legs);

    /**
     * @brief 获取指定腿的轨迹生成器
     */
    std::shared_ptr<RuckigTrajectoryAdapter> getLegTrajectory(LegID leg_id);

    /**
     * @brief 计算所有腿的同步轨迹
     * @param current_positions 当前位置
     * @param current_velocities 当前速度
     * @param target_positions 目标位置
     * @param target_velocities 目标速度
     * @return 最大持续时间
     */
    double calculateSynchronizedTrajectories(
        const std::vector<FootPosition>& current_positions,
        const std::vector<FootVelocity>& current_velocities,
        const std::vector<FootPosition>& target_positions,
        const std::vector<FootVelocity>& target_velocities);

    /**
     * @brief 更新所有腿的轨迹
     * @param dt 时间步长
     * @return 所有腿的新轨迹点
     */
    std::vector<TrajectoryPoint> updateAllLegs(double dt);

    /**
     * @brief 重置所有轨迹
     */
    void resetAll();

    /**
     * @brief 判断所有轨迹是否完成
     */
    bool allFinished() const;

    /**
     * @brief 获取总体进度 [0, 1]
     */
    double getOverallProgress() const;

private:
    std::vector<std::shared_ptr<RuckigTrajectoryAdapter>> leg_trajectories_;
    mutable std::mutex mutex_;
};

/**
 * @brief Ruckig轨迹可视化辅助类
 */
class RuckigTrajectoryVisualizer {
public:
    /**
     * @brief 生成轨迹统计信息
     */
    static std::string generateTrajectoryStats(
        const std::vector<TrajectoryPoint>& trajectory);

    /**
     * @brief 检查轨迹平滑度 (加加速度峰值)
     */
    static double calculateMaxJerk(
        const std::vector<TrajectoryPoint>& trajectory);

    /**
     * @brief 检查轨迹连续性
     */
    static bool checkContinuity(
        const std::vector<TrajectoryPoint>& trajectory,
        double position_tolerance = 1e-6,
        double velocity_tolerance = 1e-3);
};

/**
 * @brief Ruckig与现有轨迹类型的兼容性适配器
 *
 * 使Ruckig轨迹可以与现有的SwingTrajectoryStyle无缝切换
 */
class RuckigCompatibilityAdapter {
public:
    /**
     * @brief 从SwingTrajectoryParams创建Ruckig轨迹
     */
    static std::vector<TrajectoryPoint> generateRuckigTrajectoryFromSwingParams(
        const SwingTrajectoryParams& swing_params,
        const RuckigTrajectoryConfig& ruckig_config = {});

    /**
     * @brief 比较Ruckig与传统轨迹的相似度
     */
    static double compareTrajectorySimilarity(
        const std::vector<TrajectoryPoint>& trajectory1,
        const std::vector<TrajectoryPoint>& trajectory2);

    /**
     * @brief 转换轨迹样式到Ruckig约束
     */
    static RuckigConstraints swingStyleToRuckigConstraints(
        SwingTrajectoryStyle style,
        double step_height,
        double step_length,
        double duration);
};

} // namespace aurora::gait

#endif // RUCKIG_TRAJECTORY_ADAPTER_H
