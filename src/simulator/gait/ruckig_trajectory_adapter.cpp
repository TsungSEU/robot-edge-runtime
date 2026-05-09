// ruckig_trajectory_adapter.cpp - Ruckig轨迹生成器适配器实现
// 集成Ruckig实时轨迹规划库到Aurora步态系统

#include "ruckig_trajectory_adapter.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aurora::gait {

// ============================================================================
// RuckigTrajectoryAdapter 实现
// ============================================================================

RuckigTrajectoryAdapter::RuckigTrajectoryAdapter(
    const RuckigTrajectoryConfig& config)
    : config_(config)
    , state_()
{
    initializeRuckig();
}

void RuckigTrajectoryAdapter::setConfig(const RuckigTrajectoryConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    initializeRuckig();
}

const RuckigTrajectoryConfig& RuckigTrajectoryAdapter::getConfig() const {
    return config_;
}

void RuckigTrajectoryAdapter::initializeRuckig() {
    // 创建Ruckig实例，degrees_of_freedom=3 (x, y, z)
    // Note: New Ruckig API doesn't use ThrowOnError template parameter
    ruckig_ = std::make_unique<ruckig::Ruckig<3>>(config_.time_step);

    // 重置输入参数
    input_ = ruckig::InputParameter<3>();
    output_ = ruckig::OutputParameter<3>();

    // 应用约束
    applyConstraints(input_);

    // 设置控制模式 (PositionVelocity is not available in new Ruckig API)
    // Map POSITION_VELOCITY to POSITION for compatibility
    switch (config_.control_mode) {
        case RuckigControlMode::POSITION:
        case RuckigControlMode::POSITION_VELOCITY:
            input_.control_interface = ruckig::ControlInterface::Position;
            break;
        case RuckigControlMode::VELOCITY:
            input_.control_interface = ruckig::ControlInterface::Velocity;
            break;
    }

    // 设置同步选项
    input_.synchronization = config_.enable_synchronization
        ? ruckig::Synchronization::Time
        : ruckig::Synchronization::TimeIfNecessary;

    // Note: discontinuous_joints field was removed in newer Ruckig versions
    // The discontinuous_position config is now handled differently
}

void RuckigTrajectoryAdapter::applyConstraints(
    ruckig::InputParameter<3>& input) const {

    // 设置每个维度的最大速度、加速度、加加速度
    for (size_t i = 0; i < 3; ++i) {
        input.max_velocity[i] = config_.constraints.max_velocity;
        input.max_acceleration[i] = config_.constraints.max_acceleration;
        input.max_jerk[i] = config_.constraints.max_jerk;
    }

    // 应用独立的X约束
    input.max_velocity[0] = config_.constraints.x.max_velocity;
    input.max_acceleration[0] = config_.constraints.x.max_acceleration;
    input.max_jerk[0] = config_.constraints.x.max_jerk;

    // 应用独立的Y约束
    input.max_velocity[1] = config_.constraints.y.max_velocity;
    input.max_acceleration[1] = config_.constraints.y.max_acceleration;
    input.max_jerk[1] = config_.constraints.y.max_jerk;

    // 应用独立的Z约束
    input.max_velocity[2] = config_.constraints.z.max_velocity;
    input.max_acceleration[2] = config_.constraints.z.max_acceleration;
    input.max_jerk[2] = config_.constraints.z.max_jerk;
}

void RuckigTrajectoryAdapter::footPositionToRuckigVector(
    const FootPosition& pos,
    const FootVelocity& vel,
    std::array<double, 3>& position,
    std::array<double, 3>& velocity) const {

    position[0] = pos.x;
    position[1] = pos.y;
    position[2] = pos.z;

    velocity[0] = vel.vx;
    velocity[1] = vel.vy;
    velocity[2] = vel.vz;
}

void RuckigTrajectoryAdapter::ruckigVectorToFootPosition(
    const std::array<double, 3>& position,
    const std::array<double, 3>& velocity,
    FootPosition& pos,
    FootVelocity& vel) const {

    pos.x = position[0];
    pos.y = position[1];
    pos.z = position[2];

    vel.vx = velocity[0];
    vel.vy = velocity[1];
    vel.vz = velocity[2];
}

double RuckigTrajectoryAdapter::calculateMinimumTimeTrajectory(
    const FootPosition& current_pos,
    const FootVelocity& current_vel,
    const FootPosition& target_pos,
    const FootVelocity& target_vel) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 设置当前状态
    footPositionToRuckigVector(current_pos, current_vel,
                                input_.current_position,
                                input_.current_velocity);
    input_.current_acceleration = {0.0, 0.0, 0.0};

    // 设置目标状态
    footPositionToRuckigVector(target_pos, target_vel,
                                input_.target_position,
                                input_.target_velocity);
    input_.target_acceleration = {0.0, 0.0, 0.0};

    // 计算轨迹 - 使用新API，calculate现在返回Trajectory
    auto result = ruckig_->calculate(input_, output_.trajectory);

    if (result == ruckig::Result::Working
        || result == ruckig::Result::Finished) {

        state_.current_position = current_pos;
        state_.current_velocity = current_vel;
        state_.target_position = target_pos;
        state_.target_velocity = target_vel;
        state_.duration = output_.trajectory.get_duration();
        state_.current_time = 0.0;
        state_.is_finished = false;

        return state_.duration;
    }

    // 计算失败，返回0
    state_.is_finished = true;
    return 0.0;
}

bool RuckigTrajectoryAdapter::calculateFixedTimeTrajectory(
    const FootPosition& current_pos,
    const FootVelocity& current_vel,
    const FootPosition& target_pos,
    const FootVelocity& target_vel,
    double duration) {

    if (duration < config_.minimum_duration) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 设置当前状态
    footPositionToRuckigVector(current_pos, current_vel,
                                input_.current_position,
                                input_.current_velocity);
    input_.current_acceleration = {0.0, 0.0, 0.0};

    // 设置目标状态
    footPositionToRuckigVector(target_pos, target_vel,
                                input_.target_position,
                                input_.target_velocity);
    input_.target_acceleration = {0.0, 0.0, 0.0};

    // 设置最小持续时间
    input_.minimum_duration = duration;

    // 计算轨迹 - 使用新API，calculate现在返回Trajectory
    auto result = ruckig_->calculate(input_, output_.trajectory);

    if (result == ruckig::Result::Working
        || result == ruckig::Result::Finished) {

        state_.current_position = current_pos;
        state_.current_velocity = current_vel;
        state_.target_position = target_pos;
        state_.target_velocity = target_vel;
        state_.duration = output_.trajectory.get_duration();
        state_.current_time = 0.0;
        state_.is_finished = false;

        return true;
    }

    state_.is_finished = true;
    return false;
}

TrajectoryPoint RuckigTrajectoryAdapter::getNextPoint(double dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_.is_finished) {
        // 返回目标状态
        return TrajectoryPoint(
            state_.target_position,
            state_.target_velocity,
            state_.current_time,
            1.0
        );
    }

    // 执行轨迹计算
    auto result = ruckig_->update(input_, output_);

    if (result == ruckig::Result::Finished) {
        state_.is_finished = true;
    }

    // 获取新位置
    FootPosition new_pos;
    FootVelocity new_vel;
    ruckigVectorToFootPosition(output_.new_position,
                                output_.new_velocity,
                                new_pos, new_vel);

    // 更新当前状态
    state_.current_position = new_pos;
    state_.current_velocity = new_vel;
    state_.current_time += output_.time;

    // 更新输入参数为当前状态
    std::copy(output_.new_position.begin(),
              output_.new_position.end(),
              input_.current_position.begin());
    std::copy(output_.new_velocity.begin(),
              output_.new_velocity.end(),
              input_.current_velocity.begin());
    std::copy(output_.new_acceleration.begin(),
              output_.new_acceleration.end(),
              input_.current_acceleration.begin());

    double progress = (state_.duration > 0)
        ? state_.current_time / state_.duration
        : 1.0;

    return TrajectoryPoint(new_pos, new_vel, state_.current_time, progress);
}

TrajectoryPoint RuckigTrajectoryAdapter::getPointAtTime(double time) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (time >= state_.duration || state_.is_finished) {
        state_.is_finished = true;
        return TrajectoryPoint(
            state_.target_position,
            state_.target_velocity,
            state_.duration,
            1.0
        );
    }

    // 使用Ruckig的轨迹计算函数
    std::array<double, 3> new_pos, new_vel, new_acc;
    output_.trajectory.at_time(time, new_pos, new_vel, new_acc);

    FootPosition pos;
    FootVelocity vel;
    ruckigVectorToFootPosition(new_pos, new_vel, pos, vel);

    double progress = (state_.duration > 0) ? time / state_.duration : 1.0;

    return TrajectoryPoint(pos, vel, time, progress);
}

std::vector<TrajectoryPoint> RuckigTrajectoryAdapter::generateTrajectoryPoints(
    double time_step) {

    std::vector<TrajectoryPoint> points;

    // 计算点数
    size_t num_points = static_cast<size_t>(state_.duration / time_step) + 1;
    points.reserve(num_points);

    // 生成轨迹点
    double current_time = 0.0;
    while (current_time <= state_.duration) {
        points.push_back(getPointAtTime(current_time));
        current_time += time_step;
    }

    // 确保包含终点
    if (points.empty() || points.back().progress < 1.0) {
        points.push_back(TrajectoryPoint(
            state_.target_position,
            state_.target_velocity,
            state_.duration,
            1.0
        ));
    }

    return points;
}

void RuckigTrajectoryAdapter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    state_ = RuckigTrajectoryState();
    output_ = ruckig::OutputParameter<3>();

    // 重置输入加速度
    input_.current_acceleration = {0.0, 0.0, 0.0};
    input_.target_acceleration = {0.0, 0.0, 0.0};
}

void RuckigTrajectoryAdapter::setControlMode(RuckigControlMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.control_mode = mode;
    initializeRuckig();
}

void RuckigTrajectoryAdapter::setConstraints(
    const RuckigConstraints& constraints) {

    std::lock_guard<std::mutex> lock(mutex_);
    config_.constraints = constraints;
    applyConstraints(input_);
}

const RuckigTrajectoryState& RuckigTrajectoryAdapter::getTrajectoryState()
    const {
    return state_;
}

bool RuckigTrajectoryAdapter::isFinished() const {
    return state_.is_finished;
}

double RuckigTrajectoryAdapter::getProgress() const {
    return (state_.duration > 0)
        ? state_.current_time / state_.duration
        : (state_.is_finished ? 1.0 : 0.0);
}

void RuckigTrajectoryAdapter::abort() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.is_finished = true;
}

bool RuckigTrajectoryAdapter::validateTrajectory(
    const std::vector<TrajectoryPoint>& trajectory) const {

    for (const auto& point : trajectory) {
        // 检查位置约束
        if (std::abs(point.position.x) > 1.0) return false;
        if (std::abs(point.position.y) > 1.0) return false;
        if (point.position.z < -0.5 || point.position.z > 1.0) return false;

        // 检查速度约束
        double v_norm = point.velocity.norm();
        if (v_norm > config_.constraints.max_velocity * 1.1) {
            return false;  // 允许10%误差
        }
    }

    return true;
}

double RuckigTrajectoryAdapter::benchmarkCalculationTime(
    size_t iterations) const {

    // 创建测试实例
    RuckigTrajectoryAdapter test_adapter(config_);

    FootPosition start(0, 0, 0);
    FootVelocity start_vel(0, 0, 0);
    FootPosition target(0.25, 0, 0.05);
    FootVelocity target_vel(0, 0, 0);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        test_adapter.calculateMinimumTimeTrajectory(
            start, start_vel, target, target_vel);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);

    return static_cast<double>(duration.count()) / iterations / 1e6;
}

// ============================================================================
// MultiLegRuckigCoordinator 实现
// ============================================================================

MultiLegRuckigCoordinator::MultiLegRuckigCoordinator(
    const RuckigTrajectoryConfig& config) {

    // 预创建两条腿的轨迹生成器 (双足)
    leg_trajectories_.push_back(
        std::make_shared<RuckigTrajectoryAdapter>(config));
    leg_trajectories_.push_back(
        std::make_shared<RuckigTrajectoryAdapter>(config));
}

void MultiLegRuckigCoordinator::setNumLegs(size_t num_legs) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (num_legs > leg_trajectories_.size()) {
        // 添加新的轨迹生成器
        size_t current_size = leg_trajectories_.size();
        for (size_t i = current_size; i < num_legs; ++i) {
            leg_trajectories_.push_back(
                std::make_shared<RuckigTrajectoryAdapter>(
                    leg_trajectories_[0]->getConfig()));
        }
    }
}

std::shared_ptr<RuckigTrajectoryAdapter>
MultiLegRuckigCoordinator::getLegTrajectory(LegID leg_id) {

    int index = static_cast<int>(leg_id);
    if (index >= 0 && index < static_cast<int>(leg_trajectories_.size())) {
        return leg_trajectories_[index];
    }
    return nullptr;
}

double MultiLegRuckigCoordinator::calculateSynchronizedTrajectories(
    const std::vector<FootPosition>& current_positions,
    const std::vector<FootVelocity>& current_velocities,
    const std::vector<FootPosition>& target_positions,
    const std::vector<FootVelocity>& target_velocities) {

    std::lock_guard<std::mutex> lock(mutex_);

    size_t num_legs = std::min({current_positions.size(),
                                current_velocities.size(),
                                target_positions.size(),
                                target_velocities.size(),
                                leg_trajectories_.size()});

    double max_duration = 0.0;

    for (size_t i = 0; i < num_legs; ++i) {
        double duration = leg_trajectories_[i]->calculateMinimumTimeTrajectory(
            current_positions[i],
            current_velocities[i],
            target_positions[i],
            target_velocities[i]
        );

        max_duration = std::max(max_duration, duration);
    }

    // 如果需要同步，重新计算所有轨迹以使用相同的持续时间
    if (num_legs > 0 && max_duration > 0) {
        for (size_t i = 0; i < num_legs; ++i) {
            leg_trajectories_[i]->calculateFixedTimeTrajectory(
                current_positions[i],
                current_velocities[i],
                target_positions[i],
                target_velocities[i],
                max_duration
            );
        }
    }

    return max_duration;
}

std::vector<TrajectoryPoint> MultiLegRuckigCoordinator::updateAllLegs(
    double dt) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<TrajectoryPoint> points;
    points.reserve(leg_trajectories_.size());

    for (auto& trajectory : leg_trajectories_) {
        points.push_back(trajectory->getNextPoint(dt));
    }

    return points;
}

void MultiLegRuckigCoordinator::resetAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& trajectory : leg_trajectories_) {
        trajectory->reset();
    }
}

bool MultiLegRuckigCoordinator::allFinished() const {
    for (const auto& trajectory : leg_trajectories_) {
        if (!trajectory->isFinished()) {
            return false;
        }
    }
    return true;
}

double MultiLegRuckigCoordinator::getOverallProgress() const {
    if (leg_trajectories_.empty()) {
        return 1.0;
    }

    double total_progress = 0.0;
    for (const auto& trajectory : leg_trajectories_) {
        total_progress += trajectory->getProgress();
    }

    return total_progress / leg_trajectories_.size();
}

// ============================================================================
// RuckigTrajectoryVisualizer 实现
// ============================================================================

std::string RuckigTrajectoryVisualizer::generateTrajectoryStats(
    const std::vector<TrajectoryPoint>& trajectory) {

    if (trajectory.empty()) {
        return "Empty trajectory";
    }

    double max_jerk = calculateMaxJerk(trajectory);
    bool is_continuous = checkContinuity(trajectory);

    char buffer[256];
    snprintf(buffer, sizeof(buffer),
        "Points: %zu, Duration: %.3fs, Max Jerk: %.2f, Continuous: %s",
        trajectory.size(),
        trajectory.back().time,
        max_jerk,
        is_continuous ? "Yes" : "No"
    );

    return std::string(buffer);
}

double RuckigTrajectoryVisualizer::calculateMaxJerk(
    const std::vector<TrajectoryPoint>& trajectory) {

    if (trajectory.size() < 3) {
        return 0.0;
    }

    double max_jerk = 0.0;
    double dt = trajectory[1].time - trajectory[0].time;

    for (size_t i = 2; i < trajectory.size(); ++i) {
        // 计算加加速度: da/dt
        double ax = (trajectory[i].velocity.vx - trajectory[i-1].velocity.vx) / dt;
        double ax_prev = (trajectory[i-1].velocity.vx - trajectory[i-2].velocity.vx) / dt;
        double jx = (ax - ax_prev) / dt;

        double ay = (trajectory[i].velocity.vy - trajectory[i-1].velocity.vy) / dt;
        double ay_prev = (trajectory[i-1].velocity.vy - trajectory[i-2].velocity.vy) / dt;
        double jy = (ay - ay_prev) / dt;

        double az = (trajectory[i].velocity.vz - trajectory[i-1].velocity.vz) / dt;
        double az_prev = (trajectory[i-1].velocity.vz - trajectory[i-2].velocity.vz) / dt;
        double jz = (az - az_prev) / dt;

        double jerk = std::sqrt(jx*jx + jy*jy + jz*jz);
        max_jerk = std::max(max_jerk, jerk);
    }

    return max_jerk;
}

bool RuckigTrajectoryVisualizer::checkContinuity(
    const std::vector<TrajectoryPoint>& trajectory,
    double position_tolerance,
    double velocity_tolerance) {

    for (size_t i = 1; i < trajectory.size(); ++i) {
        // 检查位置连续性
        double pos_diff = (trajectory[i].position -
                          trajectory[i-1].position).norm();

        double expected_diff = trajectory[i].time - trajectory[i-1].time;
        double max_expected_pos = (trajectory[i-1].velocity.norm() + 1.0) * expected_diff;

        if (pos_diff > max_expected_pos + position_tolerance) {
            return false;
        }

        // 检查速度连续性
        double vel_diff = (trajectory[i].velocity -
                          trajectory[i-1].velocity).norm();

        if (vel_diff > velocity_tolerance) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// RuckigCompatibilityAdapter 实现
// ============================================================================

std::vector<TrajectoryPoint> RuckigCompatibilityAdapter::
generateRuckigTrajectoryFromSwingParams(
    const SwingTrajectoryParams& swing_params,
    const RuckigTrajectoryConfig& ruckig_config) {

    RuckigTrajectoryAdapter adapter(ruckig_config);

    // 设置初始速度为0
    FootVelocity start_vel(0, 0, 0);
    FootVelocity target_vel(0, 0, 0);

    // 计算轨迹
    adapter.calculateMinimumTimeTrajectory(
        swing_params.start,
        start_vel,
        swing_params.end,
        target_vel
    );

    // 生成轨迹点
    return adapter.generateTrajectoryPoints(0.001);
}

double RuckigCompatibilityAdapter::compareTrajectorySimilarity(
    const std::vector<TrajectoryPoint>& trajectory1,
    const std::vector<TrajectoryPoint>& trajectory2) {

    if (trajectory1.empty() || trajectory2.empty()) {
        return 0.0;
    }

    // 重采样到相同数量点
    size_t num_points = std::max(trajectory1.size(), trajectory2.size());

    double total_diff = 0.0;
    size_t count = 0;

    for (size_t i = 0; i < num_points; ++i) {
        double t1 = static_cast<double>(i) / num_points;
        double t2 = static_cast<double>(i) / num_points;

        // 找到对应时间点
        size_t idx1 = std::min(
            static_cast<size_t>(t1 * trajectory1.size()),
            trajectory1.size() - 1);
        size_t idx2 = std::min(
            static_cast<size_t>(t2 * trajectory2.size()),
            trajectory2.size() - 1);

        double diff = (trajectory1[idx1].position -
                       trajectory2[idx2].position).norm();

        total_diff += diff;
        ++count;
    }

    if (count == 0) return 0.0;

    double avg_diff = total_diff / count;

    // 转换为相似度分数 [0, 1]
    // 使用exp(-diff)作为相似度度量
    return std::exp(-avg_diff * 10.0);
}

RuckigConstraints RuckigCompatibilityAdapter::swingStyleToRuckigConstraints(
    SwingTrajectoryStyle style,
    double step_height,
    double step_length,
    double duration) {

    // 计算基本约束
    double max_vel_x = step_length / duration * 2.0;
    double max_vel_z = step_height / duration * 3.0;

    double max_acc_x = max_vel_x / (duration * 0.3);
    double max_acc_z = max_vel_z / (duration * 0.3);

    RuckigConstraints constraints;

    switch (style) {
        case SwingTrajectoryStyle::SINUSOID:
            // 正弦轨迹：平滑，中等约束
            constraints.max_velocity = std::max(max_vel_x, max_vel_z) * 1.2;
            constraints.max_acceleration = std::max(max_acc_x, max_acc_z) * 1.5;
            constraints.max_jerk = constraints.max_acceleration * 20.0;
            break;

        case SwingTrajectoryStyle::CUBIC_SPLINE:
            // 三次样条：灵活，略高约束
            constraints.max_velocity = std::max(max_vel_x, max_vel_z) * 1.3;
            constraints.max_acceleration = std::max(max_acc_x, max_acc_z) * 1.8;
            constraints.max_jerk = constraints.max_acceleration * 30.0;
            break;

        case SwingTrajectoryStyle::QUINTIC:
            // 五次多项式：端点速度/加速度为0，严格约束
            constraints.max_velocity = std::max(max_vel_x, max_vel_z) * 1.5;
            constraints.max_acceleration = std::max(max_acc_x, max_acc_z) * 2.0;
            constraints.max_jerk = constraints.max_acceleration * 50.0;
            break;

        case SwingTrajectoryStyle::CYCLOID:
            // 摆线轨迹：起落点速度为0，中等严格约束
            constraints.max_velocity = std::max(max_vel_x, max_vel_z) * 1.4;
            constraints.max_acceleration = std::max(max_acc_x, max_acc_z) * 2.0;
            constraints.max_jerk = constraints.max_acceleration * 40.0;
            break;

        case SwingTrajectoryStyle::COMBO:
        default:
            // 组合轨迹：Z用正弦，XY用样条
            constraints.max_velocity = std::max(max_vel_x, max_vel_z) * 1.3;
            constraints.max_acceleration = std::max(max_acc_x, max_acc_z) * 1.8;
            constraints.max_jerk = constraints.max_acceleration * 25.0;
            break;
    }

    // 设置独立的维度约束
    constraints.x.max_velocity = max_vel_x;
    constraints.x.max_acceleration = max_acc_x;
    constraints.x.max_jerk = constraints.max_jerk;

    constraints.y.max_velocity = max_vel_x * 0.5;  // Y方向较小
    constraints.y.max_acceleration = max_acc_x * 0.5;
    constraints.y.max_jerk = constraints.max_jerk * 0.5;

    constraints.z.max_velocity = max_vel_z;
    constraints.z.max_acceleration = max_acc_z;
    constraints.z.max_jerk = constraints.max_jerk;

    return constraints;
}

} // namespace aurora::gait
