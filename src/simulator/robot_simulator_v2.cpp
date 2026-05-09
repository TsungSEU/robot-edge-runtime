// robot_simulator_v2.cpp

#include "robot_simulator_v2.h"
#include "data_collection/common/base.h"
#include "common/utils/utils.h"
#include "common/ros2/qos_profiles.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

namespace aurora {
namespace sim {

using namespace aurora::gait;

RobotSimulatorV2::RobotSimulatorV2()
    : rclcpp::Node("robot_mc")
    , simulation_running_(false)
    , current_waypoint_index_(0)
    , waypoint_wait_time_(0.5)  // Wait 0.5s - enough for DCP to process but not too long to cause blocking
    , waiting_at_waypoint_(false)
    , left_foot_stance_(true)
    , right_foot_stance_(true)
    , fast_trig_(aurora::performance::getFastTrig())
{
    // ========== 初始化腿部几何参数 ==========
    leg_geometry_.hip_width = 0.1;          // 髋关节间距 10cm
    leg_geometry_.upper_leg_length = 0.35;   // 大腿长度 35cm
    leg_geometry_.lower_leg_length = 0.35;   // 小腿长度 35cm
    leg_geometry_.foot_height = 0.05;        // 脚厚度 5cm
    leg_geometry_.hip_offset_z = 0.0;        // 髋关节与躯干同一高度

    // ========== 初始化机器人状态 ==========
    robot_x_ = 0.0;
    robot_y_ = 0.0;
    robot_z_ = leg_geometry_.upper_leg_length + leg_geometry_.lower_leg_length;
    robot_roll_ = 0.0;
    robot_pitch_ = 0.0;
    robot_yaw_ = 0.0;
    robot_vx_ = 0.0;
    robot_vy_ = 0.0;
    robot_vz_ = 0.0;
    robot_wx_ = 0.0;
    robot_wy_ = 0.0;
    robot_wz_ = 0.0;

    // ========== 初始化步态参数 ==========
    gait_params_.step_frequency = 1.25;     // 1.25 Hz 步频
    gait_params_.step_height = 0.05;        // 5cm 步高
    gait_params_.step_length = 0.25;        // 25cm 步长
    gait_params_.stance_duration = 0.48;    // 0.48s 支撑相
    gait_params_.swing_duration = 0.32;     // 0.32s 摆动相
    gait_params_.duty_factor = 0.6;         // 60% 占空比

    // ========== 初始化足端世界坐标位置（绝对坐标，用于真正驱动行走） ==========
    // 左脚起始位置（世界坐标系）- 初始有x偏移以产生步态
    double initial_stance_width = leg_geometry_.hip_width;
    double initial_step_offset = gait_params_.step_length * 0.5;

    left_foot_world_ = {-initial_step_offset * 0.5, initial_stance_width / 2.0, 0.0};
    // 右脚起始位置（世界坐标系）
    right_foot_world_ = {initial_step_offset * 0.5, -initial_stance_width / 2.0, 0.0};

    // ========== 初始化足端目标位置 ==========
    left_foot_target_ = left_foot_world_;
    right_foot_target_ = right_foot_world_;

    // ========== 初始化关节名称 ==========
    joint_names_ = {
        // 左腿 (0-5)
        "left_hip_yaw", "left_hip_roll", "left_hip_pitch",
        "left_knee_pitch", "left_ankle_pitch", "left_ankle_roll",
        // 右腿 (6-11)
        "right_hip_yaw", "right_hip_roll", "right_hip_pitch",
        "right_knee_pitch", "right_ankle_pitch", "right_ankle_roll"
    };

    joint_positions_.resize(NUM_JOINTS, 0.0);
    joint_velocities_.resize(NUM_JOINTS, 0.0);

    // ========== 初始化预分配消息缓冲区 ==========
    joint_msg_buffer_.name = joint_names_;
    joint_msg_buffer_.position.reserve(NUM_JOINTS);
    joint_msg_buffer_.velocity.reserve(NUM_JOINTS);
    joint_msg_buffer_.effort.resize(NUM_JOINTS, 0.0);
    tf_msg_buffer_.transforms.reserve(1);

    // ========== 创建步态协调器 ==========
    gait_coordinator_ = std::make_unique<GaitCoordinator>();
    gait_coordinator_->setNumLegs(2);
    gait_coordinator_->setGaitMode(GaitMode::BIPED_WALK);
    gait_coordinator_->setGaitParameters(gait_params_);
    setupGaitEventCallback();

    // ========== 创建运动学控制器 ==========
    KinematicsControlConfig kinematics_config;
    kinematics_config.update_rate = UPDATE_RATE;
    kinematics_config.max_joint_acceleration = 50.0;

    kinematics_controller_ = std::make_unique<MultiLegKinematicsController>(kinematics_config);
    kinematics_controller_->setNumLegs(2);
    kinematics_controller_->setLegGeometry(leg_geometry_);

    // ========== 初始化速度控制（默认启用） ==========
    feature_flags_.use_velocity_control = true;
    {
        auto vel_config = VelocityLocomotionControllerConfig();
        vel_config.smoothing_config.use_ruckig = false;  // 禁用 Ruckig，避免响应过慢
        vel_config.smoothing_config.smoothing_time = 0.1; // 快速响应（100ms）
        velocity_controller_ = std::make_unique<VelocityLocomotionController>(vel_config);
    }

    // ========== 创建 ROS2 发布者 ==========
    odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/robot/odom", aurora::common::qos::odometry());
    joint_state_pub_ = this->create_publisher<sensor_msgs::msg::JointState>("/robot/joint_states", aurora::common::qos::sensor_data());
    imu_pub_ = this->create_publisher<sensor_msgs::msg::Imu>("/robot/imu", aurora::common::qos::sensor_data());
    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/robot/cmd_vel", aurora::common::qos::velocity_cmd());
    robot_desc_pub_ = this->create_publisher<std_msgs::msg::String>("/robot_description", aurora::common::qos::static_data());
    tf_pub_ = this->create_publisher<tf2_msgs::msg::TFMessage>("/tf", aurora::common::qos::tf_transforms());

    // ========== 创建速度命令订阅者（与cmd_vel发布区分开） ==========
    velocity_cmd_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
        "/robot/velocity_cmd", aurora::common::qos::velocity_cmd(),
        std::bind(&RobotSimulatorV2::velocityCommandCallback, this, std::placeholders::_1));

    // 发布 URDF 和静态 TF
    publishRobotDescription();
    publishStaticTransforms();

    // ========== 创建仿真定时器 ==========
    static constexpr int TIMER_PERIOD_MS = static_cast<int>(1000.0 / UPDATE_RATE);
    simulation_timer_ = this->create_wall_timer(
        std::chrono::milliseconds(TIMER_PERIOD_MS),
        [this]() { this->updateSimulation(); });

    RCLCPP_INFO(this->get_logger(), "Robot Simulator V2 initialized (Gait Control with Proper Kinematics, Velocity Command Mode)");
    RCLCPP_INFO(this->get_logger(), "  Leg Geometry: upper=%.2fm, lower=%.2fm, hip_width=%.2fm",
               leg_geometry_.upper_leg_length, leg_geometry_.lower_leg_length, leg_geometry_.hip_width);
    RCLCPP_INFO(this->get_logger(), "  Gait Mode: %s", gait_coordinator_->getCurrentGaitModeName());
    RCLCPP_INFO(this->get_logger(), "  Step Frequency: %.2f Hz", gait_params_.step_frequency);
    RCLCPP_INFO(this->get_logger(), "  Update rate: %.1f Hz", UPDATE_RATE);
}

void RobotSimulatorV2::startSimulation() {
    simulation_running_ = true;
    gait_coordinator_->start();

    // 初始化足端世界坐标位置（起步姿态）
    // 关键：两脚应该在x方向上有一些错开，这样才能产生前进运动
    double initial_stance_width = leg_geometry_.hip_width;
    double initial_step_offset = gait_params_.step_length * 0.5;  // 初始步态相位偏移

    left_foot_world_ = {-initial_step_offset * 0.5, initial_stance_width / 2.0, 0.0};
    right_foot_world_ = {initial_step_offset * 0.5, -initial_stance_width / 2.0, 0.0};
    left_foot_target_ = left_foot_world_;
    right_foot_target_ = right_foot_world_;
    left_foot_stance_ = true;
    right_foot_stance_ = true;

    // 初始化躯干位置（在两脚中心）
    robot_x_ = 0.0;
    robot_y_ = 0.0;
    robot_z_ = leg_geometry_.upper_leg_length + leg_geometry_.lower_leg_length;
    robot_yaw_ = 0.0;

    // 初始化站立姿态的关节角度
    std::vector<FootPosition> initial_foot_positions(2);
    initial_foot_positions[0] = FootPosition(0.0, leg_geometry_.hip_width / 2.0,
                                              -(leg_geometry_.upper_leg_length +
                                               leg_geometry_.lower_leg_length +
                                               leg_geometry_.foot_height));
    initial_foot_positions[1] = FootPosition(0.0, -leg_geometry_.hip_width / 2.0,
                                              -(leg_geometry_.upper_leg_length +
                                               leg_geometry_.lower_leg_length +
                                               leg_geometry_.foot_height));

    auto joint_states = kinematics_controller_->update(DT, initial_foot_positions);

    for (size_t leg = 0; leg < 2; ++leg) {
        for (size_t j = 0; j < 6; ++j) {
            joint_positions_[leg * 6 + j] = joint_states[leg][j].position;
        }
    }

    RCLCPP_INFO(this->get_logger(), "Simulation started");
}

void RobotSimulatorV2::stopSimulation() {
    simulation_running_ = false;
    gait_coordinator_->stop();
    RCLCPP_INFO(this->get_logger(), "Simulation stopped");
}

void RobotSimulatorV2::setTargetPath(const std::vector<std::pair<double, double>>& path) {
    std::lock_guard<std::mutex> lock(path_mutex_);
    target_path_ = path;
    current_waypoint_index_ = 0;
    waiting_at_waypoint_ = false;  // Reset wait state for new path
    RCLCPP_INFO(this->get_logger(), "Target path set with %zu waypoints", path.size());
}

void RobotSimulatorV2::setGaitMode(GaitMode mode) {
    gait_coordinator_->setGaitMode(mode);
    RCLCPP_INFO(this->get_logger(), "Gait mode changed to: %s",
                gait_coordinator_->getCurrentGaitModeName());
}

void RobotSimulatorV2::setGaitParameters(const GaitParameters& params) {
    gait_params_ = params;
    gait_coordinator_->setGaitParameters(params);
    RCLCPP_INFO(this->get_logger(), "Gait parameters updated");
}

std::string RobotSimulatorV2::getGaitStateInfo() const {
    std::ostringstream oss;
    oss << "Gait State Info:\n";
    oss << "  Mode: " << gait_coordinator_->getCurrentGaitModeName() << "\n";
    oss << "  Phase: " << gait_coordinator_->getGlobalPhase() << "\n";
    oss << "  Stance Legs: " << gait_coordinator_->getStanceLegCount();

    // 获取所有腿的状态
    const auto& states = gait_coordinator_->getAllStates();
    for (size_t i = 0; i < states.size(); ++i) {
        oss << "\n  Leg " << i << ": " << GaitUtils::gaitStateToString(states[i].state)
              << " phase=" << states[i].phase;
    }

    return oss.str();
}

void RobotSimulatorV2::setupGaitEventCallback() {
    gait_coordinator_->setEventCallback(
        [this](LegID leg_id, GaitEvent event, const LegGaitState& state) {
            // Commented out DEBUG logs to prevent spam (called at 50Hz during gait events)
            /*
            switch (event) {
                case GaitEvent::SWING_TRIGGER:
                    RCLCPP_DEBUG(this->get_logger(), "[%s] Swing trigger",
                                GaitUtils::legIDToString(leg_id));
                    break;
                case GaitEvent::CONTACT_DETECTED:
                    RCLCPP_DEBUG(this->get_logger(), "[%s] Contact detected at phase=%.2f",
                                GaitUtils::legIDToString(leg_id), state.phase);
                    // 记录足端着地位置
                    if (leg_id == LegID::LEFT) {
                        RCLCPP_DEBUG(this->get_logger(), "  Left foot at world: (%.2f, %.2f)",
                                    left_foot_world_.x, left_foot_world_.y);
                    } else {
                        RCLCPP_DEBUG(this->get_logger(), "  Right foot at world: (%.2f, %.2f)",
                                    right_foot_world_.x, right_foot_world_.y);
                    }
                    break;
                default:
                    break;
            }
            */
            (void)leg_id; (void)event; (void)state;  // Suppress unused warnings
        }
    );
}

void RobotSimulatorV2::updateSimulation() {
    if (!simulation_running_) {
        return;
    }

    // 1. 更新步态状态（无共享状态，无需加锁）
    updateGait();

    // 2. 运动控制（先更新朝向和速度，再用新朝向更新位置）
    {
        std::scoped_lock lock(state_mutex_, path_mutex_, error_mutex_);
        if (!target_path_.empty()) {
            pathTrackingControl();
        } else if (feature_flags_.use_velocity_control && velocity_controller_) {
            velocityControlUpdate();
        }
    }

    // 3-5. 更新机器人状态（使用已更新的朝向）
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        updateTorsoFromFeet();
        updateFootWorldPositions();
        updateJointAngles();
    }

    // 6. 发布消息（publishMessages内部会获取锁并释放，避免在这里持有锁调用阻塞的publish）
    publishMessages();
}

void RobotSimulatorV2::updateGait() {
    // 更新步态状态机
    auto events = gait_coordinator_->update(DT);
}

void RobotSimulatorV2::updateFootWorldPositions() {
    // 核心改进：正确实现双足行走的足端轨迹
    // 关键原则：
    // 1. 支撑腿的脚固定在地面（世界坐标系不变）
    // 2. 摆动腿的脚按轨迹移动到新的着地位置
    // 3. 在摆动结束时，将脚的世界位置保存为新的目标位置

    const auto& leg_states = gait_coordinator_->getAllStates();
    if (leg_states.size() < 2) return;

    // leg_states[0] 是 LEFT, leg_states[1] 是 RIGHT
    const auto& left_state = leg_states[0];
    const auto& right_state = leg_states[1];

    double global_phase = gait_coordinator_->getGlobalPhase();
    double step_length = gait_params_.step_length;

    // 性能优化：使用快速三角函数查找表
    double cos_yaw, sin_yaw;
    fast_trig_.fastSinCos(robot_yaw_, sin_yaw, cos_yaw);

    // 从相位计算摆动进度
    // 摆动相开始于 phase = 2π * duty_factor，结束于 2π
    double swing_start_phase = 2.0 * M_PI * gait_params_.duty_factor;

    // 髋关节宽度
    double hip_half_width = leg_geometry_.hip_width / 2.0;

    // ===== 左脚轨迹 (LEFT, phase_offset = 0) =====
    left_foot_stance_ = (left_state.state == GaitState::STANCE ||
                         left_state.state == GaitState::CONTACT ||
                         left_state.state == GaitState::EARLY_CONTACT);

    if (left_foot_stance_) {
        // 支撑相：脚固定在地面，保持不变（世界坐标）
        left_foot_world_.z = 0.0;

        // 当刚进入支撑相（CONTACT）时，记录脚的着地位置
        if (left_state.state == GaitState::CONTACT) {
            left_foot_target_.x = left_foot_world_.x;
            left_foot_target_.y = left_foot_world_.y;
        }
    } else {
        // 摆动相：从相位计算摆动进度
        double phase_in_swing = global_phase - swing_start_phase;
        if (phase_in_swing < 0) phase_in_swing += 2.0 * M_PI;

        double swing_progress = phase_in_swing / (2.0 * M_PI * (1.0 - gait_params_.duty_factor));
        swing_progress = std::clamp(swing_progress, 0.0, 1.0);

        // 摆动轨迹：从当前位置到新的着地位置
        // 新着地位置 = 当前躯干位置 + 步长偏移 + 髋关节宽度偏移
        double new_x = robot_x_ + step_length * cos_yaw - hip_half_width * sin_yaw;
        double new_y = robot_y_ + step_length * sin_yaw + hip_half_width * cos_yaw;

        // 使用摆动轨迹（三次样条曲线）
        double s = swing_progress;
        double s_smooth = s * s * (3 - 2 * s);  // 平滑插值

        // 水平位置：从起点到终点
        left_foot_world_.x = left_foot_target_.x + (new_x - left_foot_target_.x) * s_smooth;
        left_foot_world_.y = left_foot_target_.y + (new_y - left_foot_target_.y) * s_smooth;

        // 垂直位置：抛物线抬脚轨迹
        left_foot_world_.z = gait_params_.step_height * 4.0 * s * (1.0 - s);

        // 摆动结束时更新目标位置（着地）
        if (swing_progress >= 0.98) {
            left_foot_target_.x = left_foot_world_.x;
            left_foot_target_.y = left_foot_world_.y;
            left_foot_world_.z = 0.0;
        }
    }

    // ===== 右脚轨迹 (RIGHT, phase_offset = π) =====
    right_foot_stance_ = (right_state.state == GaitState::STANCE ||
                          right_state.state == GaitState::CONTACT ||
                          right_state.state == GaitState::EARLY_CONTACT);

    if (right_foot_stance_) {
        // 支撑相：脚固定在地面
        right_foot_world_.z = 0.0;

        if (right_state.state == GaitState::CONTACT) {
            right_foot_target_.x = right_foot_world_.x;
            right_foot_target_.y = right_foot_world_.y;
        }
    } else {
        // 摆动相：右腿相位偏移为π
        double right_phase = GaitUtils::normalizePhase(global_phase + M_PI);
        double phase_in_swing = right_phase - swing_start_phase;
        if (phase_in_swing < 0) phase_in_swing += 2.0 * M_PI;

        double swing_progress = phase_in_swing / (2.0 * M_PI * (1.0 - gait_params_.duty_factor));
        swing_progress = std::clamp(swing_progress, 0.0, 1.0);

        double new_x = robot_x_ + step_length * cos_yaw - hip_half_width * sin_yaw;
        double new_y = robot_y_ + step_length * sin_yaw - hip_half_width * cos_yaw;

        double s = swing_progress;
        double s_smooth = s * s * (3 - 2 * s);

        right_foot_world_.x = right_foot_target_.x + (new_x - right_foot_target_.x) * s_smooth;
        right_foot_world_.y = right_foot_target_.y + (new_y - right_foot_target_.y) * s_smooth;
        right_foot_world_.z = gait_params_.step_height * 4.0 * s * (1.0 - s);

        if (swing_progress >= 0.98) {
            right_foot_target_.x = right_foot_world_.x;
            right_foot_target_.y = right_foot_world_.y;
            right_foot_world_.z = 0.0;
        }
    }
}

void RobotSimulatorV2::updateTorsoFromFeet() {
    // 核心改进：从足端世界位置正确计算躯干位置
    // 关键原则：
    // 1. 躯干持续向前移动（基于行走速度）
    // 2. 双脚支撑时，躯干在两脚之间
    // 3. 单脚支撑时，躯干在支撑脚附近但持续前进

    double phase = gait_coordinator_->getGlobalPhase();

    // 计算躯干高度（基于腿部几何）
    double leg_extension = leg_geometry_.upper_leg_length + leg_geometry_.lower_leg_length;
    double torso_height = leg_extension;

    // 性能优化：使用快速三角函数查找表
    // 添加步态高度变化（重心起伏）
    double height_variation = gait_params_.step_height * 0.1 * fast_trig_.fastSin(phase * 2);
    robot_z_ = torso_height + height_variation;

    // 计算行走速度
    double cos_yaw, sin_yaw;
    fast_trig_.fastSinCos(robot_yaw_, sin_yaw, cos_yaw);

    if (feature_flags_.use_velocity_control && velocity_controller_) {
        // 速度命令模式：直接使用速度命令驱动机器人
        auto cmd = velocity_controller_->getCurrentVelocityCommand();
        robot_x_ += (cmd.forward * cos_yaw - cmd.lateral * sin_yaw) * DT;
        robot_y_ += (cmd.forward * sin_yaw + cmd.lateral * cos_yaw) * DT;
        robot_vx_ = cmd.forward * cos_yaw - cmd.lateral * sin_yaw;
        robot_vy_ = cmd.forward * sin_yaw + cmd.lateral * cos_yaw;
    } else {
        // 路径跟踪模式：基于步态参数计算速度
        double walk_velocity = gait_params_.step_frequency * gait_params_.step_length;
        double forward_progress = walk_velocity * DT;
        robot_x_ += forward_progress * cos_yaw;
        robot_y_ += forward_progress * sin_yaw;
        robot_vx_ = walk_velocity * cos_yaw;
        robot_vy_ = walk_velocity * sin_yaw;
    }
    robot_vz_ = 0.0;

    // 计算躯干姿态（步态引起的俯仰变化）
    double pitch_variation = 0.03 * fast_trig_.fastSin(phase);  // 3度俯仰变化
    robot_pitch_ = pitch_variation;

    // 横滚变化（重心转移）
    double roll_variation = 0.02 * fast_trig_.fastCos(phase);
    robot_roll_ = roll_variation;
}

void RobotSimulatorV2::updateJointAngles() {
    // 关键改进：将足端世界坐标转换为相对于躯干的坐标
    // 然后调用IK求解关节角度

    // 计算足端相对于躯干的位置
    FootPosition left_foot_relative, right_foot_relative;

    // 性能优化：使用快速三角函数查找表
    // 旋转矩阵：从世界坐标系到躯干坐标系
    double cos_yaw, sin_yaw;
    fast_trig_.fastSinCos(-robot_yaw_, sin_yaw, cos_yaw);

    // 左脚相对位置
    double dx_l = left_foot_world_.x - robot_x_;
    double dy_l = left_foot_world_.y - robot_y_;
    left_foot_relative.x = dx_l * cos_yaw - dy_l * sin_yaw;
    left_foot_relative.y = dx_l * sin_yaw + dy_l * cos_yaw;
    left_foot_relative.z = left_foot_world_.z - robot_z_;

    // 右脚相对位置
    double dx_r = right_foot_world_.x - robot_x_;
    double dy_r = right_foot_world_.y - robot_y_;
    right_foot_relative.x = dx_r * cos_yaw - dy_r * sin_yaw;
    right_foot_relative.y = dx_r * sin_yaw + dy_r * cos_yaw;
    right_foot_relative.z = right_foot_world_.z - robot_z_;

    // 使用运动学控制器计算关节角度
    std::vector<FootPosition> foot_positions = {
        left_foot_relative,
        right_foot_relative
    };

    auto all_joint_states = kinematics_controller_->update(DT, foot_positions);

    // 更新关节位置（带平滑）
    for (size_t leg = 0; leg < 2 && leg < all_joint_states.size(); ++leg) {
        for (size_t j = 0; j < 6 && (leg * 6 + j) < joint_positions_.size(); ++j) {
            double new_angle = all_joint_states[leg][j].position;

            // 平滑关节角度变化
            double max_joint_velocity = 5.0;  // rad/s
            double angle_diff = new_angle - joint_positions_[leg * 6 + j];
            double max_change = max_joint_velocity * DT;

            if (std::abs(angle_diff) > max_change) {
                new_angle = joint_positions_[leg * 6 + j] + std::copysign(max_change, angle_diff);
            }

            joint_positions_[leg * 6 + j] = new_angle;
            joint_velocities_[leg * 6 + j] = all_joint_states[leg][j].velocity;
        }
    }
}

void RobotSimulatorV2::pathTrackingControl() {
    if (current_waypoint_index_ >= target_path_.size()) {
        return;
    }

    // Check if we're waiting at a waypoint for DCP to collect data
    if (waiting_at_waypoint_) {
        auto now = this->now();
        double elapsed = (now - last_waypoint_reach_time_).seconds();
        if (elapsed < waypoint_wait_time_) {
            // Still waiting, don't process next waypoint yet
            return;
        }
        // Wait time over, proceed to next waypoint
        waiting_at_waypoint_ = false;
    }

    auto [target_x, target_y] = target_path_[current_waypoint_index_];

    double dx = target_x - robot_x_;
    double dy = target_y - robot_y_;
    double distance = std::sqrt(dx * dx + dy * dy);

    // 性能优化：使用快速 atan2
    double target_yaw = aurora::performance::fastAtan2(dy, dx);

    // 到达阈值 - 记录误差
    if (distance < waypoint_tolerance_) {
        // 记录到达 waypoint 时的误差
        CollectionError error;
        error.waypoint_index = current_waypoint_index_;
        error.planned = Point(target_x, target_y);
        error.actual = Point(robot_x_, robot_y_);
        error.position_error = distance;

        // 计算航向角误差
        double yaw_error = target_yaw - robot_yaw_;
        while (yaw_error > M_PI) yaw_error -= 2.0 * M_PI;
        while (yaw_error < -M_PI) yaw_error += 2.0 * M_PI;
        error.yaw_error = std::abs(yaw_error);
        error.timestamp_us = common::GetCurrentTimestamp();

        collection_errors_.push_back(error);

        RCLCPP_INFO(this->get_logger(), "Waypoint %zu/%zu reached - planned:(%.2f,%.2f) actual:(%.2f,%.2f) err:%.3fm",
                   current_waypoint_index_ + 1, target_path_.size(),
                   target_x, target_y, robot_x_, robot_y_, distance);

        current_waypoint_index_++;

        // Enter wait state if wait time is configured
        if (waypoint_wait_time_ > 0.0) {
            waiting_at_waypoint_ = true;
            last_waypoint_reach_time_ = this->now();
            RCLCPP_INFO(this->get_logger(), "Waiting %.1f seconds for DCP data collection...", waypoint_wait_time_);
        }
        return;
    }

    // 转向控制
    double yaw_error = target_yaw - robot_yaw_;
    while (yaw_error > M_PI) yaw_error -= 2.0 * M_PI;
    while (yaw_error < -M_PI) yaw_error += 2.0 * M_PI;

    double Kp_yaw = 2.0;
    robot_wz_ = Kp_yaw * yaw_error;
    robot_wz_ = std::clamp(robot_wz_, -1.5, 1.5);

    robot_yaw_ += robot_wz_ * DT;

    // 根据转角调整步长
    double speed_factor = 1.0 - std::abs(yaw_error) / M_PI;
    speed_factor = std::max(speed_factor, 0.3);
    gait_params_.step_length = 0.25 * speed_factor;
    gait_coordinator_->setGaitParameters(gait_params_);
}

void RobotSimulatorV2::velocityControlUpdate() {
    if (!velocity_controller_) return;

    // 更新速度控制器，输出转换后的步态参数
    GaitParameters params = velocity_controller_->update(DT);
    gait_params_ = params;  // 同步更新本地步态参数（供 updateFootWorldPositions 使用）
    gait_coordinator_->setGaitParameters(params);

    // 直接设置朝向角速度（来自速度命令）
    auto cmd = velocity_controller_->getCurrentVelocityCommand();
    robot_wz_ = std::clamp(cmd.angular, -1.5, 1.5);
    robot_yaw_ += robot_wz_ * DT;
}

void RobotSimulatorV2::enableVelocityControl(bool enable) {
    feature_flags_.use_velocity_control = enable;
    if (enable && !velocity_controller_) {
        velocity_controller_ = std::make_unique<VelocityLocomotionController>();
    }
    RCLCPP_INFO(this->get_logger(), "Velocity control %s", enable ? "enabled" : "disabled");
}

void RobotSimulatorV2::sendVelocityCommand(double forward, double lateral, double angular) {
    if (!feature_flags_.use_velocity_control || !velocity_controller_) {
        RCLCPP_WARN(this->get_logger(), "Velocity control not enabled");
        return;
    }
    velocity_controller_->setVelocityCommand(VelocityCommand(forward, lateral, angular));
}

void RobotSimulatorV2::velocityCommandCallback(const geometry_msgs::msg::Twist::SharedPtr msg) {
    if (!feature_flags_.use_velocity_control || !velocity_controller_) return;
    velocity_controller_->setVelocityCommand(
        VelocityCommand(msg->linear.x, msg->linear.y, msg->angular.z));

    // 首次收到速度命令时输出连接确认
    static bool first_cmd_logged = false;
    if (!first_cmd_logged) {
        first_cmd_logged = true;
        RCLCPP_INFO(this->get_logger(),
                    "Velocity command connection established: fx=%.2f ly=%.2f az=%.2f",
                    msg->linear.x, msg->linear.y, msg->angular.z);
    }
}

FootPosition RobotSimulatorV2::generateSwingTarget(LegID leg_id) {
    bool is_left = (leg_id == LegID::LEFT || leg_id == LegID::FL);

    // 目标位置：当前躯干位置 + 步长偏移
    double target_x = gait_params_.step_length * 0.5;  // 前进半个步长
    double target_y = is_left ? (leg_geometry_.hip_width / 2.0) :
                              (-leg_geometry_.hip_width / 2.0);
    double target_z = -(leg_geometry_.upper_leg_length +
                        leg_geometry_.lower_leg_length +
                        leg_geometry_.foot_height);

    // 性能优化：使用快速三角函数查找表
    // 旋转到当前朝向
    double cos_yaw, sin_yaw;
    fast_trig_.fastSinCos(robot_yaw_, sin_yaw, cos_yaw);

    FootPosition target;
    target.x = target_x * cos_yaw - target_y * sin_yaw;
    target.y = target_x * sin_yaw + target_y * cos_yaw;
    target.z = target_z;

    return target;
}

std::array<double, 3> RobotSimulatorV2::quaternionToEuler(double x, double y, double z, double w) {
    // 性能优化：使用快速 atan2
    double roll = aurora::performance::fastAtan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y));
    double pitch = std::asin(2.0 * (w * y - z * x));
    double yaw = aurora::performance::fastAtan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));
    return {roll, pitch, yaw};
}

std::array<double, 4> RobotSimulatorV2::eulerToQuaternion(double roll, double pitch, double yaw) {
    // 性能优化：使用快速三角函数查找表
    double cy, sy, cp, sp, cr, sr;
    fast_trig_.fastSinCos(yaw * 0.5, sy, cy);
    fast_trig_.fastSinCos(pitch * 0.5, sp, cp);
    fast_trig_.fastSinCos(roll * 0.5, sr, cr);

    double qw = cr * cp * cy + sr * sp * sy;
    double qx = sr * cp * cy - cr * sp * sy;
    double qy = cr * sp * cy + sr * cp * sy;
    double qz = cr * cp * sy - sr * sp * cy;

    return {qx, qy, qz, qw};
}

void RobotSimulatorV2::publishMessages() {
    rclcpp::Time now = this->now();

    // 步骤1：快速复制需要的数据（持有锁的时间尽可能短）
    double pub_x, pub_y, pub_z;
    double pub_roll, pub_pitch, pub_yaw;
    double pub_vx, pub_vy, pub_vz;
    double pub_wx, pub_wy, pub_wz;

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        pub_x = robot_x_;
        pub_y = robot_y_;
        pub_z = robot_z_;
        pub_roll = robot_roll_;
        pub_pitch = robot_pitch_;
        pub_yaw = robot_yaw_;
        pub_vx = robot_vx_;
        pub_vy = robot_vy_;
        pub_vz = robot_vz_;
        pub_wx = robot_wx_;
        pub_wy = robot_wy_;
        pub_wz = robot_wz_;

        // 直接复制到预分配的缓冲区，避免中间vector
        joint_msg_buffer_.position.assign(joint_positions_.begin(), joint_positions_.end());
        joint_msg_buffer_.velocity.assign(joint_velocities_.begin(), joint_velocities_.end());
    }

    // 步骤2：在无锁状态下调用publish（使用预分配的消息缓冲区）
    // 1. Odometry (使用预分配缓冲区)
    odom_msg_buffer_.header.stamp = now;
    odom_msg_buffer_.header.frame_id = "odom";
    odom_msg_buffer_.child_frame_id = "base_link";

    odom_msg_buffer_.pose.pose.position.x = pub_x;
    odom_msg_buffer_.pose.pose.position.y = pub_y;
    odom_msg_buffer_.pose.pose.position.z = pub_z;

    auto quat = eulerToQuaternion(pub_roll, pub_pitch, pub_yaw);
    odom_msg_buffer_.pose.pose.orientation.x = quat[0];
    odom_msg_buffer_.pose.pose.orientation.y = quat[1];
    odom_msg_buffer_.pose.pose.orientation.z = quat[2];
    odom_msg_buffer_.pose.pose.orientation.w = quat[3];

    odom_msg_buffer_.twist.twist.linear.x = pub_vx;
    odom_msg_buffer_.twist.twist.linear.y = pub_vy;
    odom_msg_buffer_.twist.twist.linear.z = pub_vz;
    odom_msg_buffer_.twist.twist.angular.x = pub_wx;
    odom_msg_buffer_.twist.twist.angular.y = pub_wy;
    odom_msg_buffer_.twist.twist.angular.z = pub_wz;

    odom_pub_->publish(odom_msg_buffer_);

    // 2. TF (使用预分配缓冲区)
    tf_buffer_.header.stamp = now;
    tf_buffer_.header.frame_id = "odom";
    tf_buffer_.child_frame_id = "base_link";

    tf_buffer_.transform.translation.x = pub_x;
    tf_buffer_.transform.translation.y = pub_y;
    tf_buffer_.transform.translation.z = pub_z;
    tf_buffer_.transform.rotation.x = quat[0];
    tf_buffer_.transform.rotation.y = quat[1];
    tf_buffer_.transform.rotation.z = quat[2];
    tf_buffer_.transform.rotation.w = quat[3];

    tf_msg_buffer_.transforms.clear();
    tf_msg_buffer_.transforms.push_back(tf_buffer_);
    tf_pub_->publish(tf_msg_buffer_);

    // 3. JointState (使用预分配缓冲区)
    joint_msg_buffer_.header.stamp = now;
    joint_state_pub_->publish(joint_msg_buffer_);

    // 4. IMU (使用预分配缓冲区)
    imu_msg_buffer_.header.stamp = now;
    imu_msg_buffer_.header.frame_id = "imu_link";
    imu_msg_buffer_.orientation = odom_msg_buffer_.pose.pose.orientation;
    imu_msg_buffer_.angular_velocity.x = pub_wx;
    imu_msg_buffer_.angular_velocity.y = pub_wy;
    imu_msg_buffer_.angular_velocity.z = pub_wz;
    imu_msg_buffer_.linear_acceleration.x = 0.0;
    imu_msg_buffer_.linear_acceleration.y = 0.0;
    imu_msg_buffer_.linear_acceleration.z = 9.81;

    imu_pub_->publish(imu_msg_buffer_);

    // 5. cmd_vel
    geometry_msgs::msg::Twist cmd_vel_msg;
    cmd_vel_msg.linear.x = pub_vx;
    cmd_vel_msg.linear.y = pub_vy;
    cmd_vel_msg.angular.z = pub_wz;
    cmd_vel_pub_->publish(cmd_vel_msg);
}

void RobotSimulatorV2::publishRobotDescription() {
    std::string urdf_path = std::string(aurora::collector::getInstallRootPath()) +
                             "/config/bipedal_robot.urdf";
    std::ifstream file(urdf_path);
    if (!file.is_open()) {
        RCLCPP_WARN(this->get_logger(), "Failed to load URDF from: %s", urdf_path.c_str());
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string urdf_content = buffer.str();

    std_msgs::msg::String msg;
    msg.data = urdf_content;
    robot_desc_pub_->publish(msg);

    RCLCPP_INFO(this->get_logger(), "Loaded URDF from: %s", urdf_path.c_str());
}

void RobotSimulatorV2::publishStaticTransforms() {
    RCLCPP_INFO(this->get_logger(), "Static transforms handled by robot_state_publisher");
}

std::tuple<double, double, size_t, size_t> RobotSimulatorV2::getErrorStatistics() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    if (collection_errors_.empty()) {
        return {0.0, 0.0, 0, 0};
    }

    double sum_error = 0.0;
    double max_error = 0.0;
    size_t within_tolerance = 0;

    for (const auto& error : collection_errors_) {
        sum_error += error.position_error;
        if (error.position_error > max_error) {
            max_error = error.position_error;
        }
        if (error.position_error < waypoint_tolerance_) {
            within_tolerance++;
        }
    }

    double avg_error = sum_error / collection_errors_.size();
    return {avg_error, max_error, within_tolerance, collection_errors_.size()};
}

std::array<double, 3> RobotSimulatorV2::getCurrentPosition() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return {robot_x_, robot_y_, robot_z_};
}

const std::vector<CollectionError>& RobotSimulatorV2::getCollectionErrors() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return collection_errors_;
}

void RobotSimulatorV2::clearCollectionErrors() {
    std::lock_guard<std::mutex> lock(error_mutex_);
    collection_errors_.clear();
}

} } // namespace aurora::sim
