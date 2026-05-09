// robot_simulator.h

#ifndef ROBOT_SIMULATOR_H
#define ROBOT_SIMULATOR_H

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_msgs/tf2_msgs/msg/tf_message.hpp>
#include <std_msgs/msg/string.hpp>
#include <memory>
#include <vector>
#include <array>
#include <chrono>
#include <cmath>

// 前向声明性能优化类
namespace aurora::performance {
class OptimizedLegIK;
}

namespace aurora::sim {

/**
 * @brief 腿部几何参数
 */
struct LegGeometry {
    double hip_width;           // 髋关节宽度 (米)
    double upper_leg_length;    // 大腿长度 (米)
    double lower_leg_length;    // 小腿长度 (米)
    double foot_height;         // 脚高度 (米)
    double hip_offset_z;        // 髋关节相对躯干的垂直偏移 (米)
};

/**
 * @brief 足端状态
 */
struct FootState {
    double x, y, z;             // 足端位置 (相对于躯干)
    bool is_stance;             // 是否在支撑相
    double swing_progress;      // 摆动相进度 [0-1]
};

/**
 * @brief 双足机器人模拟器
 *
 * 模拟双足机器人的行走并发布 ROS2 消息
 * 使用真实的步态生成和逆运动学
 */
class RobotSimulator : public rclcpp::Node {
public:
    RobotSimulator();
    ~RobotSimulator() = default;

    void startSimulation();
    void stopSimulation();
    void setTargetPath(const std::vector<std::pair<double, double>>& path);

    std::array<double, 3> getCurrentPosition() const {
        return {robot_state_.x, robot_state_.y, robot_state_.z};
    }

private:
    // ========== 定时器 ==========
    rclcpp::TimerBase::SharedPtr simulation_timer_;

    // ========== ROS2 发布者 ==========
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
    rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr robot_desc_pub_;
    rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr tf_pub_;
    rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr static_tf_pub_;

    // ========== 机器人状态 ==========
    struct RobotState {
        double x, y, z;           // 躯干位置 (米)
        double roll, pitch, yaw;  // 躯干姿态 (弧度)
        double vx, vy, vz;        // 线速度 (米/秒)
        double wx, wy, wz;        // 角速度 (弧度/秒)
        double step_phase;        // 步态相位 [0-2π]
        uint64_t step_count;      // 步数计数
    } robot_state_;

    // ========== 步态参数 ==========
    struct GaitParameters {
        double step_length;       // 步长 (米)
        double step_height;       // 摆动相最大步高 (米)
        double step_duration;     // 单步持续时间 (秒)
        double stance_ratio;      // 支撑相比例 [0-1]
        double walk_velocity;     // 行走速度 (米/秒)
    } gait_params_;

    // ========== 腿部几何 ==========
    LegGeometry leg_geom_;

    // ========== 足端状态 ==========
    FootState left_foot_;
    FootState right_foot_;

    // ========== 关节配置 ==========
    static constexpr int NUM_JOINTS = 12;
    std::vector<std::string> joint_names_;
    std::vector<double> joint_positions_;
    std::vector<double> joint_velocities_;

    // ========== 目标路径 ==========
    std::vector<std::pair<double, double>> target_path_;
    size_t current_waypoint_index_;

    // ========== 控制标志 ==========
    bool simulation_running_;

    // ========== 仿真参数 ==========
    static constexpr double UPDATE_RATE = 50.0;  // Hz
    static constexpr double DT = 1.0 / UPDATE_RATE;
    static constexpr std::chrono::microseconds UPDATE_PERIOD_US =
        std::chrono::microseconds(static_cast<int64_t>(1000000.0 / UPDATE_RATE));

    // ========== 频率控制（解决定时器漂移问题） ==========
    std::chrono::steady_clock::time_point last_update_time_;
    std::chrono::steady_clock::time_point expected_next_time_;
    uint64_t update_count_;

    // ========== 预分配消息缓冲区（避免频繁内存分配） ==========
    nav_msgs::msg::Odometry odom_msg_buffer_;
    sensor_msgs::msg::JointState joint_msg_buffer_;
    sensor_msgs::msg::Imu imu_msg_buffer_;
    tf2_msgs::msg::TFMessage tf_msg_buffer_;
    geometry_msgs::msg::TransformStamped tf_buffer_;

    // ========== 性能优化组件 ==========
    std::unique_ptr<aurora::performance::OptimizedLegIK> optimized_ik_;

    // ========== 内部方法 ==========

    void updateSimulation();
    void publishMessages();
    void publishRobotDescription();
    void publishStaticTransforms();

    // 步态生成
    void updateGait();
    void updateFootTrajectories();
    void updateTorsoFromFeet();

    // 逆运动学
    std::vector<double> computeLegIK(const FootState& foot, bool is_left);

    // 路径跟踪
    void pathTrackingControl();

    // 辅助函数
    double generateSwingHeight(double progress);
    std::array<double, 3> quaternionToEuler(double x, double y, double z, double w);
    std::array<double, 4> eulerToQuaternion(double roll, double pitch, double yaw);
};

} // namespace aurora::sim

#endif // ROBOT_SIMULATOR_H
