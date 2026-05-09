// kinematics_control_layer.cpp - 运动学控制层实现

#include "kinematics_control_layer.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <Eigen/Dense>

namespace aurora {
namespace gait {

// ========== IKSolver ==========

IKSolver::IKSolver(const LegGeometry& geometry, const IKSolverConfig& config)
    : geometry_(geometry)
    , config_(config)
{
    // 初始化默认关节限位 (6个关节)
    joint_limits_.resize(6);

    // 髋偏航关节
    joint_limits_[0] = JointLimits(-0.5, 0.5, 3.0, 50.0);
    // 髋外展关节
    joint_limits_[1] = JointLimits(-0.4, 0.4, 3.0, 80.0);
    // 髋屈伸关节
    joint_limits_[2] = JointLimits(-1.0, 1.0, 5.0, 100.0);
    // 膝屈伸关节 (只能向后弯曲)
    joint_limits_[3] = JointLimits(-2.5, 0.0, 5.0, 100.0);
    // 踝屈伸关节
    joint_limits_[4] = JointLimits(-1.0, 1.0, 5.0, 50.0);
    // 踝内外翻关节
    joint_limits_[5] = JointLimits(-0.5, 0.5, 3.0, 50.0);
}

void IKSolver::setLegGeometry(const LegGeometry& geometry) {
    std::lock_guard<std::mutex> lock(mutex_);
    geometry_ = geometry;
}

const LegGeometry& IKSolver::getLegGeometry() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return geometry_;
}

void IKSolver::setJointLimits(const std::vector<JointLimits>& limits) {
    std::lock_guard<std::mutex> lock(mutex_);
    joint_limits_ = limits;
}

const std::vector<JointLimits>& IKSolver::getJointLimits() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return joint_limits_;
}

IKResult IKSolver::solve(const FootPosition& foot_position, bool is_left) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查工作空间
    if (config_.check_workspace && !isInWorkspace(foot_position, is_left)) {
        IKResult result;
        result.success = false;
        result.error_message = "Target position outside workspace";
        return result;
    }

    // 根据求解器类型选择求解方法
    switch (config_.solver_type) {
        case IKSolverType::ANALYTICAL:
            return solveAnalytical(foot_position, is_left);

        case IKSolverType::NUMERICAL: {
            // 使用零位作为初始值
            std::vector<JointState> initial_joints(6, JointState(0, 0));
            return solveNumerical(foot_position, initial_joints, is_left);
        }

        case IKSolverType::DampedLeastSquares: {
            std::vector<JointState> initial_joints(6, JointState(0, 0));
            return solveDampedLS(foot_position, initial_joints, is_left);
        }

        case IKSolverType::HYBRID:
        default: {
            // 先尝试解析求解
            auto result = solveAnalytical(foot_position, is_left);
            if (result.success) {
                return result;
            }
            // 失败则使用数值求解
            std::vector<JointState> initial_joints(6, JointState(0, 0));
            return solveNumerical(foot_position, initial_joints, is_left);
        }
    }
}

IKResult IKSolver::solve(const FootPosition& foot_position,
                          const std::vector<JointState>& current_joints,
                          bool is_left) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.solver_type == IKSolverType::ANALYTICAL) {
        return solveAnalytical(foot_position, is_left);
    }

    if (config_.solver_type == IKSolverType::DampedLeastSquares) {
        return solveDampedLS(foot_position, current_joints, is_left);
    }

    return solveNumerical(foot_position, current_joints, is_left);
}

FootPosition IKSolver::forwardKinematics(const std::vector<JointState>& joint_states,
                                         bool is_left) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (joint_states.size() < 6) {
        return FootPosition();
    }

    // 关节角度
    double hip_yaw = joint_states[0].position;
    double hip_roll = joint_states[1].position;
    double hip_pitch = joint_states[2].position;
    double knee_pitch = joint_states[3].position;
    double ankle_pitch = joint_states[4].position;
    double ankle_roll = joint_states[5].position;

    // 简化的FK计算（假设关节绕各自坐标轴旋转）
    // 实际应用中需要使用DH参数或变换矩阵

    double L1 = geometry_.upper_leg_length;
    double L2 = geometry_.lower_leg_length;

    // 腿在矢状面内的投影
    double leg_angle = hip_pitch + knee_pitch / 2.0;
    double leg_extension = L1 * std::cos(hip_pitch) + L2 * std::cos(hip_pitch + knee_pitch);

    FootPosition result;
    result.x = leg_extension * std::cos(hip_yaw);
    result.y = leg_extension * std::sin(hip_yaw);
    result.z = -leg_extension * std::sin(leg_angle);

    // 考虑髋外展
    result.y += L1 * std::sin(hip_roll);

    return result;
}

bool IKSolver::isInWorkspace(const FootPosition& foot_position, bool is_left) const {
    double max_reach = geometry_.getTotalLegLength() * config_.max_reach_extension;
    double distance = foot_position.horizontalNorm();

    if (distance > max_reach) return false;

    // 检查垂直范围
    double min_z = -geometry_.getTotalLegLength() * 1.1;
    double max_z = 0.1;

    if (foot_position.z < min_z || foot_position.z > max_z) return false;

    return true;
}

void IKSolver::getWorkspaceBounds(bool is_left,
                                   double& min_x, double& max_x,
                                   double& min_y, double& max_y,
                                   double& min_z, double& max_z) const {
    double max_reach = geometry_.getTotalLegLength() * config_.max_reach_extension;

    min_x = -max_reach;
    max_x = max_reach * 0.6;  // 前方伸展有限

    min_y = is_left ? 0 : -max_reach;
    max_y = is_left ? max_reach : 0;

    min_z = -geometry_.getTotalLegLength() * 1.1;
    max_z = 0.1;
}

IKResult IKSolver::solveAnalytical(const FootPosition& foot_position, bool is_left) const {
    IKResult result;
    result.joint_states.resize(6);
    result.success = false;

    // 获取腿部几何参数
    double L1 = geometry_.upper_leg_length;
    double L2 = geometry_.lower_leg_length;

    // 髋关节侧向偏移
    double hip_offset_y = is_left ? (geometry_.hip_width / 2.0) : (-geometry_.hip_width / 2.0);

    // 足端相对于髋关节的位置
    double dx = foot_position.x;
    double dy = hip_offset_y - foot_position.y;
    double dz = foot_position.z - geometry_.hip_offset_z;

    // ===== 关节0: hip_yaw (髋关节偏航) =====
    result.joint_states[0].position = std::atan2(dy, std::sqrt(dx*dx + dz*dz));

    // 旋转到hip_yaw坐标系
    double cos_yaw = std::cos(result.joint_states[0].position);
    double sin_yaw = std::sin(result.joint_states[0].position);
    double dx_rot = dx * cos_yaw + dz * sin_yaw;
    double dz_rot = -dx * sin_yaw + dz * cos_yaw;

    // ===== 关节1: hip_roll (髋关节外展) =====
    double lateral_dist = dy;
    result.joint_states[1].position = std::atan2(lateral_dist, -dz_rot);

    // 在sagittal面内的距离
    double r = std::sqrt(lateral_dist*lateral_dist + dz_rot*dz_rot);

    // ===== 关节2 & 3: hip_pitch & knee_pitch =====
    double D = std::sqrt(dx_rot*dx_rot + r*r);

    // 限制在可达范围内
    double min_reach = std::abs(L1 - L2) + 0.001;
    double max_reach = L1 + L2 - 0.001;

    if (D < min_reach || D > max_reach) {
        result.error_message = "Target position outside reachable range";
        return result;
    }

    // 使用余弦定理求膝关节角度
    double cos_knee = (L1*L1 + D*D - L2*L2) / (2.0 * L1 * D);
    cos_knee = std::clamp(cos_knee, -1.0, 1.0);
    double knee_angle_internal = std::acos(cos_knee);

    // 膝关节角度（负值表示向后弯曲）
    result.joint_states[3].position = -knee_angle_internal;

    // 髋关节俯仰角
    double alpha = std::atan2(dx_rot, r);
    double cos_hip = (L1*L1 + D*D - L2*L2) / (2.0 * L1 * D);
    cos_hip = std::clamp(cos_hip, -1.0, 1.0);
    double beta = std::acos(cos_hip);

    result.joint_states[2].position = alpha + beta;

    // ===== 关节4 & 5: ankle_pitch & ankle_roll =====
    // 保持脚水平
    result.joint_states[4].position = -(result.joint_states[2].position + result.joint_states[3].position);
    result.joint_states[5].position = -result.joint_states[1].position;

    result.success = applyJointLimits(result);

    if (!result.success) {
        result.error_message = "Joint limits violated";
    }

    return result;
}

IKResult IKSolver::solveNumerical(const FootPosition& foot_position,
                                   const std::vector<JointState>& initial_joints,
                                   bool is_left) const {
    IKResult result;
    result.joint_states = initial_joints;
    result.success = false;

    // 简化的梯度下降IK求解
    std::vector<double> angles(6);
    for (size_t i = 0; i < 6; ++i) {
        angles[i] = initial_joints[i].position;
    }

    const double learning_rate = 0.1;
    const double min_error = config_.tolerance;

    for (int iter = 0; iter < static_cast<int>(config_.max_iterations); ++iter) {
        result.iterations = iter;

        // 计算当前FK
        std::vector<JointState> current_joints(6);
        for (size_t i = 0; i < 6; ++i) {
            current_joints[i].position = angles[i];
        }
        FootPosition current_pos = forwardKinematics(current_joints, is_left);

        // 计算误差
        double error_x = foot_position.x - current_pos.x;
        double error_y = foot_position.y - current_pos.y;
        double error_z = foot_position.z - current_pos.z;
        double error = std::sqrt(error_x*error_x + error_y*error_y + error_z*error_z);

        result.residual = error;

        if (error < min_error) {
            result.success = true;
            break;
        }

        // 计算数值雅可比并更新角度
        const double delta = 1e-6;
        for (size_t i = 0; i < 6; ++i) {
            // 扰动
            angles[i] += delta;
            current_joints[i].position = angles[i];
            FootPosition pos_plus = forwardKinematics(current_joints, is_left);
            angles[i] -= delta;

            // 数值微分
            double dJ_dx = (pos_plus.x - current_pos.x) / delta;
            double dJ_dy = (pos_plus.y - current_pos.y) / delta;
            double dJ_dz = (pos_plus.z - current_pos.z) / delta;

            // 梯度下降更新
            double gradient = dJ_dx * error_x + dJ_dy * error_y + dJ_dz * error_z;
            angles[i] += learning_rate * gradient;

            // 应用限位
            if (i < joint_limits_.size()) {
                angles[i] = joint_limits_[i].clipPosition(angles[i]);
            }
        }
    }

    for (size_t i = 0; i < 6; ++i) {
        result.joint_states[i].position = angles[i];
    }

    return result;
}

IKResult IKSolver::solveDampedLS(const FootPosition& foot_position,
                                  const std::vector<JointState>& initial_joints,
                                  bool is_left) const {
    // 使用Eigen实现阻尼最小二乘IK
    IKResult result;
    result.joint_states = initial_joints;
    result.success = false;

    const int num_joints = 6;
    const int num_task = 3;  // x, y, z
    const double lambda = config_.damping_factor;

    Eigen::VectorXd joint_angles(num_joints);
    for (int i = 0; i < num_joints; ++i) {
        joint_angles(i) = initial_joints[i].position;
    }

    for (int iter = 0; iter < static_cast<int>(config_.max_iterations); ++iter) {
        // 计算当前末端位置
        std::vector<JointState> current_joints(6);
        for (int i = 0; i < 6; ++i) {
            current_joints[i].position = joint_angles(i);
        }
        FootPosition current_pos = forwardKinematics(current_joints, is_left);

        // 位置误差
        Eigen::VectorXd error(num_task);
        error(0) = foot_position.x - current_pos.x;
        error(1) = foot_position.y - current_pos.y;
        error(2) = foot_position.z - current_pos.z;

        result.residual = error.norm();
        result.iterations = iter;

        if (result.residual < config_.tolerance) {
            result.success = true;
            break;
        }

        // 计算雅可比矩阵
        Eigen::MatrixXd jacobian(num_task, num_joints);
        const double delta = 1e-6;

        for (int j = 0; j < num_joints; ++j) {
            joint_angles(j) += delta;
            current_joints[j].position = joint_angles(j);
            FootPosition pos_plus = forwardKinematics(current_joints, is_left);
            joint_angles(j) -= delta;

            jacobian(0, j) = (pos_plus.x - current_pos.x) / delta;
            jacobian(1, j) = (pos_plus.y - current_pos.y) / delta;
            jacobian(2, j) = (pos_plus.z - current_pos.z) / delta;
        }

        // Damped Least Squares: delta_theta = J^T * (J * J^T + lambda^2 * I)^(-1) * error
        Eigen::MatrixXd JJT = jacobian * jacobian.transpose();
        JJT += lambda * lambda * Eigen::MatrixXd::Identity(num_task, num_task);

        Eigen::VectorXd delta_theta = jacobian.transpose() * JJT.ldlt().solve(error);

        // 更新关节角度
        joint_angles += delta_theta;

        // 应用关节限位
        for (int i = 0; i < num_joints; ++i) {
            if (i < static_cast<int>(joint_limits_.size())) {
                joint_angles(i) = joint_limits_[i].clipPosition(joint_angles(i));
            }
        }
    }

    for (int i = 0; i < num_joints; ++i) {
        result.joint_states[i].position = joint_angles(i);
    }

    return result;
}

bool IKSolver::applyJointLimits(IKResult& result) const {
    bool all_valid = true;

    for (size_t i = 0; i < result.joint_states.size() && i < joint_limits_.size(); ++i) {
        if (config_.check_joint_limits) {
            if (!joint_limits_[i].checkPosition(result.joint_states[i].position)) {
                result.joint_states[i].position = joint_limits_[i].clipPosition(
                    result.joint_states[i].position);
                all_valid = false;
            }
        }
    }

    return all_valid;
}

// ========== KinematicsControlLayer ==========

KinematicsControlLayer::KinematicsControlLayer(const KinematicsControlConfig& config)
    : config_(config)
{
    joint_states_.resize(getNumJoints());
    joint_velocities_.resize(getNumJoints(), 0.0);

    // 创建默认IK求解器
    ik_solver_ = std::make_shared<IKSolver>();
}

void KinematicsControlLayer::setIKSolver(std::shared_ptr<IKSolver> solver) {
    std::lock_guard<std::mutex> lock(mutex_);
    ik_solver_ = solver;
}

std::shared_ptr<IKSolver> KinematicsControlLayer::getIKSolver() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ik_solver_;
}

std::vector<JointState> KinematicsControlLayer::update(
    double dt,
    const std::vector<FootPosition>& target_positions,
    const std::vector<JointState>& current_joints) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<JointState> new_joints = current_joints;
    if (new_joints.size() != getNumJoints()) {
        new_joints.resize(getNumJoints());
    }

    // 假设target_positions按腿顺序排列：FL, FR, RL, RR
    // 对于双足，只有LEFT和RIGHT

    size_t num_legs = std::min(target_positions.size(), size_t(4));
    size_t joints_per_leg = getJointsPerLeg();

    for (size_t leg = 0; leg < num_legs; ++leg) {
        bool is_left = (leg == 0 || leg == 2);  // FL, RL是左腿

        IKResult ik_result = ik_solver_->solve(target_positions[leg], is_left);

        size_t joint_offset = leg * joints_per_leg;
        for (size_t j = 0; j < joints_per_leg && j < ik_result.joint_states.size(); ++j) {
            size_t joint_idx = joint_offset + j;
            if (joint_idx < new_joints.size()) {
                new_joints[joint_idx].position = ik_result.joint_states[j].position;
            }
        }
    }

    // 平滑处理
    smoothJointVelocities(new_joints, current_joints, dt);

    // 应用加速度约束
    applyAccelerationConstraints(new_joints, current_joints, dt);

    // 验证
    if (!validateJointStates(new_joints)) {
        new_joints = current_joints;  // 回退到上一个状态
    }

    joint_states_ = new_joints;

    // 计算速度
    for (size_t i = 0; i < joint_velocities_.size() && i < new_joints.size(); ++i) {
        joint_velocities_[i] = (new_joints[i].position - current_joints[i].position) / dt;
    }

    return new_joints;
}

std::vector<JointState> KinematicsControlLayer::updateFromTrajectory(
    double dt,
    const std::vector<TrajectoryPoint>& trajectory_points,
    LegID leg_id,
    const std::vector<JointState>& current_joints) {

    if (trajectory_points.empty()) {
        return current_joints;
    }

    // 获取当前轨迹点
    FootPosition target_pos = trajectory_points.front().position;

    // 确定腿的索引
    size_t leg_idx = 0;
    switch (leg_id) {
        case LegID::FL:  leg_idx = 0; break;
        case LegID::FR:  leg_idx = 1; break;
        case LegID::RL:  leg_idx = 2; break;
        case LegID::RR:  leg_idx = 3; break;
        case LegID::LEFT:  leg_idx = 0; break;
        case LegID::RIGHT: leg_idx = 1; break;
        default: leg_idx = 0; break;
    }

    std::vector<FootPosition> targets(4, FootPosition());
    targets[leg_idx] = target_pos;

    return update(dt, targets, current_joints);
}

void KinematicsControlLayer::setJointTargets(const std::vector<JointState>& targets) {
    std::lock_guard<std::mutex> lock(mutex_);
    joint_states_ = targets;
}

const std::vector<JointState>& KinematicsControlLayer::getJointStates() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return joint_states_;
}

const std::vector<double>& KinematicsControlLayer::getJointVelocities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return joint_velocities_;
}

void KinematicsControlLayer::setConfig(const KinematicsControlConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const KinematicsControlConfig& KinematicsControlLayer::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void KinematicsControlLayer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fill(joint_states_.begin(), joint_states_.end(), JointState());
    std::fill(joint_velocities_.begin(), joint_velocities_.end(), 0.0);
}

const char* KinematicsControlLayer::getJointName(JointID joint_id) {
    switch (joint_id) {
        case JointID::LEFT_HIP_YAW:    return "left_hip_yaw";
        case JointID::LEFT_HIP_ROLL:   return "left_hip_roll";
        case JointID::LEFT_HIP_PITCH:  return "left_hip_pitch";
        case JointID::LEFT_KNEE_PITCH: return "left_knee_pitch";
        case JointID::LEFT_ANKLE_PITCH:return "left_ankle_pitch";
        case JointID::LEFT_ANKLE_ROLL: return "left_ankle_roll";
        case JointID::RIGHT_HIP_YAW:   return "right_hip_yaw";
        case JointID::RIGHT_HIP_ROLL:  return "right_hip_roll";
        case JointID::RIGHT_HIP_PITCH: return "right_hip_pitch";
        case JointID::RIGHT_KNEE_PITCH:return "right_knee_pitch";
        case JointID::RIGHT_ANKLE_PITCH:return "right_ankle_pitch";
        case JointID::RIGHT_ANKLE_ROLL:return "right_ankle_roll";
        default: return "unknown_joint";
    }
}

void KinematicsControlLayer::smoothJointVelocities(
    std::vector<JointState>& joints,
    const std::vector<JointState>& prev_joints,
    double dt) {

    if (!config_.enable_velocity_profile) return;

    double alpha = config_.velocity_smoothing_factor;

    for (size_t i = 0; i < joints.size() && i < prev_joints.size(); ++i) {
        double raw_velocity = (joints[i].position - prev_joints[i].position) / dt;
        double smoothed_velocity = alpha * raw_velocity +
                                  (1 - alpha) * prev_joints[i].velocity;

        joints[i].velocity = smoothed_velocity;
    }
}

void KinematicsControlLayer::applyAccelerationConstraints(
    std::vector<JointState>& joints,
    const std::vector<JointState>& prev_joints,
    double dt) {

    double max_accel = config_.max_joint_acceleration;

    for (size_t i = 0; i < joints.size() && i < prev_joints.size(); ++i) {
        double velocity = (joints[i].position - prev_joints[i].position) / dt;
        double accel = (velocity - prev_joints[i].velocity) / dt;

        if (std::abs(accel) > max_accel) {
            double clamped_accel = std::clamp(accel, -max_accel, max_accel);
            double new_velocity = prev_joints[i].velocity + clamped_accel * dt;
            joints[i].position = prev_joints[i].position + new_velocity * dt;
            joints[i].velocity = new_velocity;
            joints[i].acceleration = clamped_accel;
        } else {
            joints[i].acceleration = accel;
        }
    }
}

bool KinematicsControlLayer::validateJointStates(const std::vector<JointState>& joints) const {
    // 基本验证：检查NaN和无穷大
    for (const auto& joint : joints) {
        if (!std::isfinite(joint.position)) return false;
        if (!std::isfinite(joint.velocity)) return false;
    }
    return true;
}

// ========== MultiLegKinematicsController ==========

MultiLegKinematicsController::MultiLegKinematicsController(
    const KinematicsControlConfig& config)
    : leg_geometry_()
{
    setNumLegs(4);
}

void MultiLegKinematicsController::setNumLegs(size_t num_legs) {
    std::lock_guard<std::mutex> lock(mutex_);

    controllers_.clear();
    ik_solvers_.clear();

    for (size_t i = 0; i < num_legs; ++i) {
        auto controller = std::make_unique<KinematicsControlLayer>();
        auto solver = std::make_shared<IKSolver>(leg_geometry_);

        controller->setIKSolver(solver);

        controllers_.push_back(std::move(controller));
        ik_solvers_.push_back(solver);
    }
}

std::vector<std::vector<JointState>> MultiLegKinematicsController::update(
    double dt,
    const std::vector<FootPosition>& target_positions) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::vector<JointState>> all_joint_states;

    for (size_t i = 0; i < controllers_.size() && i < target_positions.size(); ++i) {
        std::vector<FootPosition> single_target = {target_positions[i]};
        std::vector<JointState> current_joints = controllers_[i]->getJointStates();
        if (current_joints.empty()) {
            current_joints.resize(6, JointState());
        }

        auto new_joints = controllers_[i]->update(dt, single_target, current_joints);
        all_joint_states.push_back(new_joints);
    }

    return all_joint_states;
}

const std::vector<JointState>& MultiLegKinematicsController::getLegJointStates(LegID leg_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    int index = static_cast<int>(leg_id) % 4;
    if (index >= 0 && index < static_cast<int>(controllers_.size())) {
        return controllers_[index]->getJointStates();
    }

    static const std::vector<JointState> empty;
    return empty;
}

const std::vector<std::vector<JointState>>&
MultiLegKinematicsController::getAllJointStates() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 缓存结果
    static thread_local std::vector<std::vector<JointState>> cached_states;
    cached_states.clear();

    for (const auto& controller : controllers_) {
        cached_states.push_back(controller->getJointStates());
    }

    return cached_states;
}

void MultiLegKinematicsController::setLegGeometry(const LegGeometry& geometry) {
    std::lock_guard<std::mutex> lock(mutex_);
    leg_geometry_ = geometry;

    for (auto& solver : ik_solvers_) {
        solver->setLegGeometry(geometry);
    }
}

void MultiLegKinematicsController::setJointLimits(
    const std::vector<std::vector<JointLimits>>& limits) {

    std::lock_guard<std::mutex> lock(mutex_);

    for (size_t i = 0; i < ik_solvers_.size() && i < limits.size(); ++i) {
        ik_solvers_[i]->setJointLimits(limits[i]);
    }
}

void MultiLegKinematicsController::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& controller : controllers_) {
        controller->reset();
    }
}

// ========== KinematicsUtils ==========

const char* KinematicsUtils::jointIDToString(JointID joint_id) {
    return KinematicsControlLayer::getJointName(joint_id);
}

JointID KinematicsUtils::stringToJointID(const std::string& str) {
    if (str == "left_hip_yaw")     return JointID::LEFT_HIP_YAW;
    if (str == "left_hip_roll")    return JointID::LEFT_HIP_ROLL;
    if (str == "left_hip_pitch")   return JointID::LEFT_HIP_PITCH;
    if (str == "left_knee_pitch")  return JointID::LEFT_KNEE_PITCH;
    if (str == "left_ankle_pitch") return JointID::LEFT_ANKLE_PITCH;
    if (str == "left_ankle_roll")  return JointID::LEFT_ANKLE_ROLL;
    if (str == "right_hip_yaw")    return JointID::RIGHT_HIP_YAW;
    if (str == "right_hip_roll")   return JointID::RIGHT_HIP_ROLL;
    if (str == "right_hip_pitch")  return JointID::RIGHT_HIP_PITCH;
    if (str == "right_knee_pitch") return JointID::RIGHT_KNEE_PITCH;
    if (str == "right_ankle_pitch")return JointID::RIGHT_ANKLE_PITCH;
    if (str == "right_ankle_roll") return JointID::RIGHT_ANKLE_ROLL;
    return JointID::UNKNOWN;
}

std::vector<int> KinematicsUtils::legIDToJointIndices(LegID leg_id) {
    switch (leg_id) {
        case LegID::FL:
        case LegID::LEFT:
            return {0, 1, 2, 3, 4, 5};
        case LegID::FR:
        case LegID::RIGHT:
            return {6, 7, 8, 9, 10, 11};
        case LegID::RL:
            return {12, 13, 14, 15, 16, 17};
        case LegID::RR:
            return {18, 19, 20, 21, 22, 23};
        default:
            return {};
    }
}

bool KinematicsUtils::isLeftLegJoint(JointID joint_id) {
    return static_cast<int>(joint_id) >= 0 && static_cast<int>(joint_id) < 6;
}

bool KinematicsUtils::isRightLegJoint(JointID joint_id) {
    return static_cast<int>(joint_id) >= 6 && static_cast<int>(joint_id) < 12;
}

LegID KinematicsUtils::getLegIDFromJoint(JointID joint_id) {
    if (isLeftLegJoint(joint_id)) {
        return LegID::LEFT;
    } else if (isRightLegJoint(joint_id)) {
        return LegID::RIGHT;
    }
    return LegID::UNKNOWN;
}

double KinematicsUtils::normalizeAngle(double angle) {
    while (angle > M_PI) angle -= 2.0 * M_PI;
    while (angle < -M_PI) angle += 2.0 * M_PI;
    return angle;
}

double KinematicsUtils::angleDifference(double angle1, double angle2) {
    double diff = angle1 - angle2;
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    return diff;
}

JointState KinematicsUtils::interpolateJoint(const JointState& j1,
                                               const JointState& j2,
                                               double t) {
    JointState result;
    result.position = j1.position + (j2.position - j1.position) * t;
    result.velocity = j1.velocity + (j2.velocity - j1.velocity) * t;
    result.acceleration = j1.acceleration + (j2.acceleration - j1.acceleration) * t;
    result.effort = j1.effort + (j2.effort - j1.effort) * t;
    return result;
}

} // namespace gait
} // namespace aurora
