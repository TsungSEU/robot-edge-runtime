// robot_simulator.cpp
// 特点：
// 1. 基于真实双足步态：摆动相和支撑相分离
// 2. 正确的逆运动学：考虑腿部几何约束
// 3. 躯干位置由足端位置决定（非滑动）

#include "robot_simulator.h"
#include "data_collection/common/base.h"
#include "common/performance_utils.h"
#include "common/ros2/qos_profiles.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

namespace aurora { namespace sim {

RobotSimulator::RobotSimulator()
    : rclcpp::Node("robot_simulator")
    , simulation_running_(false)
    , current_waypoint_index_(0) {

    // ========== 初始化腿部几何参数 (基于MuJoCo XML实际尺寸) ==========
    // 从bipedal_robot_full.xml中的关节偏移量计算：
    // - hip_to_knee: 0.1774 + 0.1976 ≈ 0.375m
    // - knee_to_ankle: 0.385m
    // - ankle_to_sole: 0.064m (foot_offset)
    leg_geom_ = {
        .hip_width = 0.1,          // 髋关节间距 10cm
        .upper_leg_length = 0.375,  // 大腿长度 37.5cm (从hip到knee)
        .lower_leg_length = 0.385,  // 小腿长度 38.5cm (从knee到ankle)
        .foot_height = 0.064,       // 脚踝到脚底 6.4cm
        .hip_offset_z = 0.008       // 髋关节相对躯干的垂直偏移
    };

    // ========== 初始化机器人状态 ==========
    // 计算正确的初始高度（基于MuJoCo XML实际几何）：
    // 诊断脚本确认：当qpos=0时，脚在z=-0.907m位置
    // 因此base_link应该在z=0.907m处，使脚正好在地面上
    robot_state_ = {
        .x = 0.0, .y = 0.0, .z = 0.907,  // 躯干高度（诊断确认）
        .roll = 0.0, .pitch = 0.0, .yaw = 0.0,
        .vx = 0.0, .vy = 0.0, .vz = 0.0,
        .wx = 0.0, .wy = 0.0, .wz = 0.0,
        .step_phase = 0.0,
        .step_count = 0
    };

    // ========== 初始化步态参数 ==========
    gait_params_ = {
        .step_length = 0.25,         // 步长 25cm (更自然的步幅)
        .step_height = 0.05,         // 摆动相抬脚 5cm
        .step_duration = 0.8,        // 单步 0.8秒 (1.25Hz 步频)
        .stance_ratio = 0.6,         // 支撑相占 60%
        .walk_velocity = 0.3         // 行走速度 0.3 m/s
    };

    // ========== 初始化足端状态 ==========
    // 初始站立姿态：双脚分开与髋同宽，在躯干正下方
    // z值：脚在髋关节下方，距离为 upper_leg + lower_leg + foot_height
    double foot_depth = -(leg_geom_.upper_leg_length + leg_geom_.lower_leg_length + leg_geom_.foot_height);

    left_foot_ = {
        .x = 0.0,
        .y = leg_geom_.hip_width / 2.0,
        .z = foot_depth,  // 约-0.824m
        .is_stance = true,
        .swing_progress = 0.0
    };

    right_foot_ = {
        .x = 0.0,
        .y = -leg_geom_.hip_width / 2.0,
        .z = foot_depth,  // 约-0.824m
        .is_stance = true,
        .swing_progress = 0.0
    };

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

    // ========== 初始化频率控制 ==========
    update_count_ = 0;
    last_update_time_ = std::chrono::steady_clock::now();
    expected_next_time_ = last_update_time_ + UPDATE_PERIOD_US;

    // ========== 初始化性能优化组件 ==========
    // 使用优化的逆运动学求解器
    optimized_ik_ = std::make_unique<aurora::performance::OptimizedLegIK>(
        leg_geom_.upper_leg_length,
        leg_geom_.lower_leg_length,
        leg_geom_.hip_width
    );

    // ========== 创建 ROS2 发布者 ==========
    odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/robot/odom", aurora::common::qos::odometry());
    joint_state_pub_ = this->create_publisher<sensor_msgs::msg::JointState>("/joint_states", aurora::common::qos::sensor_data());
    imu_pub_ = this->create_publisher<sensor_msgs::msg::Imu>("/robot/imu", aurora::common::qos::sensor_data());
    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/robot/cmd_vel", aurora::common::qos::velocity_cmd());
    robot_desc_pub_ = this->create_publisher<std_msgs::msg::String>("/robot_description", aurora::common::qos::static_data());
    tf_pub_ = this->create_publisher<tf2_msgs::msg::TFMessage>("/tf", aurora::common::qos::tf_transforms());
    static_tf_pub_ = this->create_publisher<tf2_msgs::msg::TFMessage>("/tf", aurora::common::qos::tf_transforms());

    // 发布 URDF 和静态 TF
    publishRobotDescription();
    publishStaticTransforms();

    // ========== 创建仿真定时器 ==========
    simulation_timer_ = this->create_wall_timer(
        std::chrono::duration<double>(DT),
        [this]() { this->updateSimulation(); });

    RCLCPP_INFO(this->get_logger(), "Robot Simulator initialized (Realistic Gait Mode)");
    RCLCPP_INFO(this->get_logger(), "  Leg Geometry: upper=%.2fm, lower=%.2fm, hip_width=%.2fm",
               leg_geom_.upper_leg_length, leg_geom_.lower_leg_length, leg_geom_.hip_width);
    RCLCPP_INFO(this->get_logger(), "  Gait: step_length=%.2fm, step_height=%.2fm, duration=%.2fs, velocity=%.2fm/s",
               gait_params_.step_length, gait_params_.step_height, gait_params_.step_duration, gait_params_.walk_velocity);
    RCLCPP_INFO(this->get_logger(), "  Torso height: %.3fm (matching MuJoCo model)", robot_state_.z);
    RCLCPP_INFO(this->get_logger(), "  Update rate: %.1f Hz", UPDATE_RATE);
}

void RobotSimulator::startSimulation() {
    simulation_running_ = true;

    // 重置频率控制
    update_count_ = 0;
    last_update_time_ = std::chrono::steady_clock::now();
    expected_next_time_ = last_update_time_ + UPDATE_PERIOD_US;

    RCLCPP_INFO(this->get_logger(), "Simulation started");
}

void RobotSimulator::stopSimulation() {
    simulation_running_ = false;
    RCLCPP_INFO(this->get_logger(), "Simulation stopped");
}

void RobotSimulator::setTargetPath(const std::vector<std::pair<double, double>>& path) {
    target_path_ = path;
    current_waypoint_index_ = 0;
    RCLCPP_INFO(this->get_logger(), "Target path set with %zu waypoints", path.size());
}

void RobotSimulator::updateSimulation() {
    if (!simulation_running_) {
        return;
    }

    // ========== 频率控制：记录实际执行时间 ==========
    auto now = std::chrono::steady_clock::now();

    // 检测是否错过了预期的执行时间
    auto time_since_expected = std::chrono::duration_cast<std::chrono::microseconds>(
        now - expected_next_time_).count();

    // 如果延迟超过周期的50%，记录警告
    if (time_since_expected > UPDATE_PERIOD_US.count() / 2 && update_count_ > 10) {
        static int warning_count = 0;
        if (++warning_count < 10) {  // 限制警告次数
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                "Simulation update delayed by %.2f ms (expected: %.2f ms)",
                time_since_expected / 1000.0, UPDATE_PERIOD_US.count() / 1000.0);
        }
    }

    // 更新预期执行时间（向后对齐到最近的周期边界）
    if (time_since_expected > 0) {
        // 已经错过了预期时间，计算需要跳过多少个周期
        int periods_to_skip = static_cast<int>(time_since_expected / UPDATE_PERIOD_US.count()) + 1;
        expected_next_time_ += UPDATE_PERIOD_US * periods_to_skip;
    } else {
        // 还在预期时间之前，正常前进一个周期
        expected_next_time_ += UPDATE_PERIOD_US;
    }

    update_count_++;

    // 1. 更新步态相位
    updateGait();

    // 2. 更新足端轨迹 (核心改进：基于步态而非简单正弦波)
    updateFootTrajectories();

    // 3. 从足端位置更新躯干位置 (核心改进：躯干由支撑脚决定)
    updateTorsoFromFeet();

    // 4. 路径跟踪控制
    if (!target_path_.empty()) {
        pathTrackingControl();
    }

    // 5. 计算逆运动学，获取关节角度 (性能优化版本)
    double left_joints[6], right_joints[6];
    optimized_ik_->compute(left_foot_.x, left_foot_.y, left_foot_.z, true, left_joints);
    optimized_ik_->compute(right_foot_.x, right_foot_.y, right_foot_.z, false, right_joints);

    for (int i = 0; i < 6; ++i) {
        joint_positions_[i] = left_joints[i];
        joint_positions_[i + 6] = right_joints[i];
    }

    // 6. 发布消息
    publishMessages();
}

// ========== 步态生成 (核心改进) ==========

void RobotSimulator::updateGait() {
    // 步态相位随时间增加
    double phase_increment = (2.0 * M_PI * DT) / gait_params_.step_duration;
    robot_state_.step_phase += phase_increment;

    if (robot_state_.step_phase >= 2.0 * M_PI) {
        robot_state_.step_phase -= 2.0 * M_PI;
        robot_state_.step_count++;
    }
}

void RobotSimulator::updateFootTrajectories() {
    // 双足步态：左右脚交替摆动
    // 当 phase ∈ [0, π): 左脚摆动，右脚支撑
    // 当 phase ∈ [π, 2π): 右脚摆动，左脚支撑
    //
    // 改进的步态设计：
    // - 摆动脚：从后方(-step_length/2)迈到前方(+step_length/2)
    // - 支撑脚：固定在地面，躯干相对于它向前移动
    // - 躯干高度：动态变化，模拟真实行走的重心起伏

    // 获取快速三角函数实例（性能优化）
    auto& trig = aurora::performance::getFastTrig();

    double phase = robot_state_.step_phase;
    bool left_swinging = (phase < M_PI);
    double swing_progress = left_swinging ? (phase / M_PI) : ((phase - M_PI) / M_PI);

    // 前进步长方向 (基于当前朝向)
    double forward_dir = robot_state_.yaw;

    // 计算当前躯干高度（与updateTorsoFromFeet中的计算一致）
    // 性能优化：使用快速 sin
    double torso_height_variation = gait_params_.step_height * 0.3 * trig.fastSin(phase);
    double base_torso_height = leg_geom_.upper_leg_length + leg_geom_.lower_leg_length;
    double current_torso_height = base_torso_height + torso_height_variation - leg_geom_.foot_height * 0.5;

    // 脚到髋关节的垂直距离（负值表示在髋下方）
    double foot_depth = -(current_torso_height + leg_geom_.foot_height * 0.5);

    // 性能优化：预先计算 cos(forward_dir)
    double cos_forward = trig.fastCos(forward_dir);

    // ===== 左脚轨迹 =====
    if (left_swinging) {
        // 摆动相：脚抬起并从后向前移动
        left_foot_.is_stance = false;
        left_foot_.swing_progress = swing_progress;

        // X: 从后方迈到前方
        double step_x = (swing_progress - 0.5) * gait_params_.step_length;
        left_foot_.x = step_x * cos_forward;

        // Y: 保持髋宽位置
        left_foot_.y = leg_geom_.hip_width / 2.0;

        // Z: 抬脚轨迹（在摆动相中间抬起）
        left_foot_.z = foot_depth + generateSwingHeight(swing_progress);

    } else {
        // 支撑相：脚固定在地面上
        left_foot_.is_stance = true;
        left_foot_.swing_progress = 0.0;

        double stance_progress = (phase - M_PI) / M_PI;
        double step_x = (0.5 - stance_progress) * gait_params_.step_length;
        left_foot_.x = step_x * cos_forward;
        left_foot_.y = leg_geom_.hip_width / 2.0;
        left_foot_.z = foot_depth;  // 贴地
    }

    // ===== 右脚轨迹 (相位差 π) =====
    if (!left_swinging) {
        // 摆动相
        right_foot_.is_stance = false;
        right_foot_.swing_progress = swing_progress;

        double step_x = (swing_progress - 0.5) * gait_params_.step_length;
        right_foot_.x = step_x * cos_forward;
        right_foot_.y = -leg_geom_.hip_width / 2.0;
        right_foot_.z = foot_depth + generateSwingHeight(swing_progress);

    } else {
        // 支撑相
        right_foot_.is_stance = true;
        right_foot_.swing_progress = 0.0;

        double stance_progress = phase / M_PI;
        double step_x = (0.5 - stance_progress) * gait_params_.step_length;
        right_foot_.x = step_x * cos_forward;
        right_foot_.y = -leg_geom_.hip_width / 2.0;
        right_foot_.z = foot_depth;
    }
}

void RobotSimulator::updateTorsoFromFeet() {
    // 核心改进：躯干位置由支撑脚决定，而非直接滑动
    // 同时添加躯干高度变化，使摆动腿膝盖能自然弯曲

    // 获取快速三角函数实例（性能优化）
    auto& trig = aurora::performance::getFastTrig();

    // 计算躯干高度变化（模拟真实行走时的重心起伏）
    // 在摆动相中间，躯干轻微下沉，使摆动腿膝盖能弯曲
    double phase = robot_state_.step_phase;
    // 性能优化：使用快速 sin
    double torso_height_variation = gait_params_.step_height * 0.3 * trig.fastSin(phase);
    // 基础躯干高度
    double base_torso_height = leg_geom_.upper_leg_length + leg_geom_.lower_leg_length;
    // 躯干高度：基础高度 + 变化量 - 脚厚
    robot_state_.z = base_torso_height + torso_height_variation - leg_geom_.foot_height * 0.5;

    // 获取支撑脚
    FootState* stance_foot = nullptr;
    if (left_foot_.is_stance) stance_foot = &left_foot_;
    if (right_foot_.is_stance) stance_foot = &right_foot_;

    if (stance_foot) {
        // 在支撑相期间，躯干相对于支撑脚的位置
        // 支撑脚在世界坐标系固定，躯干向前移动

        // 计算躯干相对支撑脚的前移
        bool left_swinging = (phase < M_PI);
        double stance_progress = left_swinging ? ((phase - M_PI) / M_PI) : (phase / M_PI);

        // 躯干在支撑相中向前移动半个步长
        double torso_offset = stance_progress * gait_params_.step_length * 0.5;

        // 性能优化：使用快速三角函数同时计算 sin 和 cos
        double cos_yaw, sin_yaw;
        trig.fastSinCos(robot_state_.yaw, sin_yaw, cos_yaw);

        // 更新躯干位置（相对于世界坐标系）
        // 这里简化处理：实际应该积分速度
        robot_state_.x += torso_offset * cos_yaw * DT * 2.0;
        robot_state_.y += torso_offset * sin_yaw * DT * 2.0;

        // 速度估算
        robot_state_.vx = gait_params_.walk_velocity * cos_yaw;
        robot_state_.vy = gait_params_.walk_velocity * sin_yaw;
        robot_state_.vz = 0.0;
    }
}

// ========== 逆运动学 (核心改进) ==========

std::vector<double> RobotSimulator::computeLegIK(const FootState& foot, bool is_left) {
    // 基于腿部几何的正确逆运动学
    // 使用解析解法 (解析 IK 比数值迭代快且稳定)

    std::vector<double> joints(6, 0.0);

    // 足端相对于髋关节的位置
    double hip_offset_y = is_left ? (leg_geom_.hip_width / 2.0) : (-leg_geom_.hip_width / 2.0);

    double dx = foot.x;  // 前后
    double dy = hip_offset_y - foot.y;  // 左右 (符号修正：髋-脚，使外展角正确)
    double dz = foot.z - leg_geom_.hip_offset_z;  // 上下 (负值向下)

    // ===== 关节 0: hip_yaw (髋关节偏航) =====
    // 使脚指向目标方向
    joints[0] = std::atan2(dy, std::sqrt(dx*dx + dz*dz));

    // 旋转到 hip_yaw 坐标系
    double cos_yaw = std::cos(joints[0]);
    double sin_yaw = std::sin(joints[0]);
    double dx_rot = dx * cos_yaw + dz * sin_yaw;
    double dz_rot = -dx * sin_yaw + dz * cos_yaw;

    // ===== 关节 1: hip_roll (髋关节外展) =====
    // 使用 atan2(侧向, -纵向) 使脚在下方时 roll=0
    double lateral_dist = dy;  // 侧向距离
    joints[1] = std::atan2(lateral_dist, -dz_rot);

    // 在 sagittal 平面内的距离
    double r = std::sqrt(lateral_dist*lateral_dist + dz_rot*dz_rot);

    // ===== 关节 2 & 3: hip_pitch & knee_pitch (髋关节和膝关节) =====
    // 使用余弦定理求解两关节腿

    double L1 = leg_geom_.upper_leg_length;
    double L2 = leg_geom_.lower_leg_length;

    // 从髋关节到脚踝的距离
    double D = std::sqrt(dx_rot*dx_rot + r*r);

    // 限制在可达范围内
    D = std::clamp(D, std::abs(L1 - L2) + 0.001, L1 + L2 - 0.001);

    // 使用余弦定理求膝关节角度
    // L2² = L1² + D² - 2*L1*D*cos(π - hip_pitch)
    double cos_knee = (L1*L1 + D*D - L2*L2) / (2.0 * L1 * D);
    cos_knee = std::clamp(cos_knee, -1.0, 1.0);
    double knee_angle_internal = std::acos(cos_knee);

    // 膝关节角度：URDF中膝关节限制为 [-2.5, 0]，负值表示向后弯曲（正常）
    // 因此我们需要取负值
    joints[3] = -knee_angle_internal;

    // 髋关节俯仰角
    double alpha = std::atan2(dx_rot, r);
    double cos_hip = (L1*L1 + D*D - L2*L2) / (2.0 * L1 * D);
    cos_hip = std::clamp(cos_hip, -1.0, 1.0);
    double beta = std::acos(cos_hip);

    // 髋关节俯仰：正值表示向前抬腿
    joints[2] = alpha + beta;

    // ===== 关节 4 & 5: ankle_pitch & ankle_roll (踝关节) =====
    // 保持脚水平
    joints[4] = -(joints[2] + joints[3]);  // pitch: 抵消腿部倾角
    joints[5] = -joints[1];                 // roll: 抵消腿部外展

    return joints;
}

// ========== 路径跟踪 ==========

void RobotSimulator::pathTrackingControl() {
    if (current_waypoint_index_ >= target_path_.size()) {
        // 到达终点，停止
        gait_params_.walk_velocity = 0.0;
        return;
    }

    auto [target_x, target_y] = target_path_[current_waypoint_index_];

    double dx = target_x - robot_state_.x;
    double dy = target_y - robot_state_.y;
    double distance = std::sqrt(dx * dx + dy * dy);
    double target_yaw = std::atan2(dy, dx);

    // 到达阈值
    if (distance < 0.2) {
        current_waypoint_index_++;
        RCLCPP_INFO(this->get_logger(), "Waypoint %zu/%zu reached",
                   current_waypoint_index_, target_path_.size());
        return;
    }

    // 转向控制
    double yaw_error = target_yaw - robot_state_.yaw;
    while (yaw_error > M_PI) yaw_error -= 2.0 * M_PI;
    while (yaw_error < -M_PI) yaw_error += 2.0 * M_PI;

    double Kp_yaw = 2.0;
    robot_state_.wz = Kp_yaw * yaw_error;
    robot_state_.wz = std::clamp(robot_state_.wz, -1.5, 1.5);

    robot_state_.yaw += robot_state_.wz * DT;

    // 根据转角调整速度
    double speed_factor = 1.0 - std::abs(yaw_error) / M_PI;
    speed_factor = std::max(speed_factor, 0.3);
    // 基础速度与步态参数中的walk_velocity一致
    gait_params_.walk_velocity = 0.3 * speed_factor;  // 基础速度 0.3 m/s
}

// ========== 辅助函数 ==========

double RobotSimulator::generateSwingHeight(double progress) {
    // 生成平滑的抬脚轨迹
    // 使用正弦平方: h = H * sin²(π * progress)
    // 在起落点速度为0，轨迹平滑
    // 性能优化：使用快速 sin
    auto& trig = aurora::performance::getFastTrig();
    double sin_val = trig.fastSin(M_PI * progress);
    return gait_params_.step_height * sin_val * sin_val;
}

std::array<double, 4> RobotSimulator::eulerToQuaternion(double roll, double pitch, double yaw) {
    // 性能优化：使用快速三角函数
    auto& trig = aurora::performance::getFastTrig();

    double cy, sy, cp, sp, cr, sr;
    trig.fastSinCos(yaw * 0.5, sy, cy);
    trig.fastSinCos(pitch * 0.5, sp, cp);
    trig.fastSinCos(roll * 0.5, sr, cr);

    double qw = cr * cp * cy + sr * sp * sy;
    double qx = sr * cp * cy - cr * sp * sy;
    double qy = cr * sp * cy + sr * cp * sy;
    double qz = cr * cp * sy - sr * sp * cy;

    return {qx, qy, qz, qw};
}

std::array<double, 3> RobotSimulator::quaternionToEuler(double x, double y, double z, double w) {
    double roll = std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y));
    double pitch = std::asin(2.0 * (w * y - z * x));
    double yaw = std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));

    return {roll, pitch, yaw};
}

// ========== 消息发布 ==========

void RobotSimulator::publishMessages() {
    rclcpp::Time now = this->now();

    // 性能优化：预先计算四元数，避免重复计算
    auto quat = eulerToQuaternion(robot_state_.roll, robot_state_.pitch, robot_state_.yaw);

    // 使用预分配的消息缓冲区，避免频繁的内存分配

    // 1. Odometry (使用预分配缓冲区)
    odom_msg_buffer_.header.stamp = now;
    odom_msg_buffer_.header.frame_id = "odom";
    odom_msg_buffer_.child_frame_id = "base_link";

    odom_msg_buffer_.pose.pose.position.x = robot_state_.x;
    odom_msg_buffer_.pose.pose.position.y = robot_state_.y;
    odom_msg_buffer_.pose.pose.position.z = robot_state_.z;

    odom_msg_buffer_.pose.pose.orientation.x = quat[0];
    odom_msg_buffer_.pose.pose.orientation.y = quat[1];
    odom_msg_buffer_.pose.pose.orientation.z = quat[2];
    odom_msg_buffer_.pose.pose.orientation.w = quat[3];

    odom_msg_buffer_.twist.twist.linear.x = robot_state_.vx;
    odom_msg_buffer_.twist.twist.linear.y = robot_state_.vy;
    odom_msg_buffer_.twist.twist.linear.z = robot_state_.vz;
    odom_msg_buffer_.twist.twist.angular.x = robot_state_.wx;
    odom_msg_buffer_.twist.twist.angular.y = robot_state_.wy;
    odom_msg_buffer_.twist.twist.angular.z = robot_state_.wz;

    odom_pub_->publish(odom_msg_buffer_);

    // 2. TF (使用预分配缓冲区)
    tf_buffer_.header.stamp = now;
    tf_buffer_.header.frame_id = "odom";
    tf_buffer_.child_frame_id = "base_link";

    tf_buffer_.transform.translation.x = robot_state_.x;
    tf_buffer_.transform.translation.y = robot_state_.y;
    tf_buffer_.transform.translation.z = robot_state_.z;
    tf_buffer_.transform.rotation.x = quat[0];
    tf_buffer_.transform.rotation.y = quat[1];
    tf_buffer_.transform.rotation.z = quat[2];
    tf_buffer_.transform.rotation.w = quat[3];

    tf_msg_buffer_.transforms.clear();
    tf_msg_buffer_.transforms.push_back(tf_buffer_);
    tf_pub_->publish(tf_msg_buffer_);

    // 3. JointState (使用预分配缓冲区)
    joint_msg_buffer_.header.stamp = now;
    joint_msg_buffer_.position.assign(joint_positions_.begin(), joint_positions_.end());
    joint_msg_buffer_.velocity.assign(joint_velocities_.begin(), joint_velocities_.end());
    joint_state_pub_->publish(joint_msg_buffer_);

    // 4. IMU (使用预分配缓冲区)
    imu_msg_buffer_.header.stamp = now;
    imu_msg_buffer_.header.frame_id = "imu_link";
    imu_msg_buffer_.orientation = odom_msg_buffer_.pose.pose.orientation;
    imu_msg_buffer_.angular_velocity.x = robot_state_.wx;
    imu_msg_buffer_.angular_velocity.y = robot_state_.wy;
    imu_msg_buffer_.angular_velocity.z = robot_state_.wz;
    imu_msg_buffer_.linear_acceleration.x = 0.0;
    imu_msg_buffer_.linear_acceleration.y = 0.0;
    imu_msg_buffer_.linear_acceleration.z = 9.81;

    imu_pub_->publish(imu_msg_buffer_);

    // 5. cmd_vel
    geometry_msgs::msg::Twist cmd_vel_msg;
    cmd_vel_msg.linear.x = robot_state_.vx;
    cmd_vel_msg.linear.y = robot_state_.vy;
    cmd_vel_msg.angular.z = robot_state_.wz;
    cmd_vel_pub_->publish(cmd_vel_msg);
}

void RobotSimulator::publishRobotDescription() {
    std::string urdf_path = std::string(aurora::collector::getInstallRootPath()) + "/config/bipedal_robot.urdf";
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

void RobotSimulator::publishStaticTransforms() {
    rclcpp::Time now = this->now();
    tf2_msgs::msg::TFMessage tf_msg;

    // 只发布 odom -> base_link 的变换
    // 其他关节变换由 robot_state_publisher 从 URDF 和 JointState 计算
    // 不发布腿部静态变换，因为它们会与 URDF 中的关节定义冲突

    (void)tf_msg;  // 保留变量以避免警告
    (void)now;     // 保留变量以避免警告

    // 不再发布静态变换，让 robot_state_publisher 处理
    RCLCPP_INFO(this->get_logger(), "Static transforms disabled - using robot_state_publisher for URDF joints");
}

} } // namespace aurora::sim
