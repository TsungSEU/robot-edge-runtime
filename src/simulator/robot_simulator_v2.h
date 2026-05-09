// robot_simulator_v2.h
// 使用新的步态状态机、足端轨迹生成器和运动学控制层
#ifndef ROBOT_SIMULATOR_V2_H
#define ROBOT_SIMULATOR_V2_H

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_msgs/tf2_msgs/msg/tf_message.hpp>
#include <std_msgs/msg/string.hpp>

#include "gait/gait_state_machine.h"
#include "gait/foot_trajectory_generator.h"
#include "gait/kinematics_control_layer.h"

// AgiBot X2集成
#include "gait/velocity_locomotion_controller.h"
#include "gait/motion_mode_manager.h"
#include "gait/safety_system.h"
#include "gait/ground_contact_model.h"
#include "gait/adaptive_gait_controller.h"
#include "gait/preset_motion_library.h"

#include <memory>
#include <vector>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <mutex>
#include <shared_mutex>

#include "common/performance_utils.h"

namespace aurora::sim {

/**
 * @brief 2D 点结构
 */
struct Point {
    double x, y;
    Point(double x = 0, double y = 0) : x(x), y(y) {}
};

/**
 * @brief 采集点误差记录
 * 记录规划位置 vs 实际到达位置的误差
 */
struct CollectionError {
    size_t waypoint_index;       // 对应的规划 waypoint 索引
    Point planned;               // 规划位置
    Point actual;                // 实际到达位置
    double position_error;       // 位置误差（米）
    double yaw_error;            // 航向角误差（弧度）
    uint64_t timestamp_us;       // 时间戳（微秒）

    CollectionError() : waypoint_index(0), position_error(0), yaw_error(0), timestamp_us(0) {}
};

/**
 * @brief 双足机器人模拟器V2
 *
 * - GaitCoordinator: 步态状态机和协调
 * - FootTrajectoryGenerator: 足端轨迹生成
 * - MultiLegKinematicsController: 运动学控制
 */
class RobotSimulatorV2 : public rclcpp::Node {
public:
    RobotSimulatorV2();
    ~RobotSimulatorV2() override = default;

    void startSimulation();
    void stopSimulation();
    void setTargetPath(const std::vector<std::pair<double, double>>& path);

    /**
     * @brief 设置步态模式
     */
    void setGaitMode(aurora::gait::GaitMode mode);

    /**
     * @brief 设置步态参数
     */
    void setGaitParameters(const aurora::gait::GaitParameters& params);

    /**
     * @brief 获取当前机器人位置
     */
    std::array<double, 3> getCurrentPosition() const;

    /**
     * @brief 获取步态状态信息
     */
    std::string getGaitStateInfo() const;

    /**
     * @brief 获取采集点误差记录
     */
    const std::vector<CollectionError>& getCollectionErrors() const;

    /**
     * @brief 获取误差统计摘要
     * @return {平均误差, 最大误差, 成功到达点数, 总点数}
     */
    std::tuple<double, double, size_t, size_t> getErrorStatistics() const;

    /**
     * @brief 清空误差记录
     */
    void clearCollectionErrors();

    /**
     * @brief 设置 waypoint 到达容差
     */
    void setWaypointTolerance(double tolerance) {
        waypoint_tolerance_ = tolerance;
    }

    // ========== Zhiyuan集成功能 ==========

    /**
     * @brief 启用速度控制接口
     */
    void enableVelocityControl(bool enable);

    /**
     * @brief 发送速度命令
     */
    void sendVelocityCommand(double forward, double lateral, double angular);

    /**
     * @brief 设置运动模式
     */
    bool setMotionMode(aurora::gait::MotionMode mode);

    /**
     * @brief 获取安全系统状态
     */
    aurora::gait::SafetySystemStatus getSafetyStatus() const;

    /**
     * @brief 触发紧急停止
     */
    void triggerEmergencyStop(const std::string& reason = "Manual");

    /**
     * @brief 执行预设动作
     */
    bool executePresetMotion(const std::string& motion_name);

    /**
     * @brief 启用自适应步态
     */
    void enableAdaptiveGait(bool enable);

    /**
     * @brief 更新环境因素（用于自适应步态）
     */
    void updateEnvironmentFactors(const aurora::gait::EnvironmentFactors& factors);

private:
    // ========== 定时器 ==========
    rclcpp::TimerBase::SharedPtr simulation_timer_;
    static constexpr double UPDATE_RATE = 50.0;  // Hz
    static constexpr double DT = 1.0 / UPDATE_RATE;

    // ========== 预分配消息缓冲区（避免频繁内存分配） ==========
    nav_msgs::msg::Odometry odom_msg_buffer_;
    sensor_msgs::msg::JointState joint_msg_buffer_;
    sensor_msgs::msg::Imu imu_msg_buffer_;
    tf2_msgs::msg::TFMessage tf_msg_buffer_;
    geometry_msgs::msg::TransformStamped tf_buffer_;

    // ========== ROS2 发布者 ==========
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
    rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr robot_desc_pub_;
    rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr tf_pub_;
    rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr static_tf_pub_;

    // ========== 步态控制模块 ==========
    std::unique_ptr<aurora::gait::GaitCoordinator> gait_coordinator_;
    std::unique_ptr<aurora::gait::MultiLegKinematicsController> kinematics_controller_;

    // ========== Zhiyuan集成模块（可选） ==========
    // 功能标志
    struct FeatureFlags {
        bool use_velocity_control;
        bool use_motion_modes;
        bool use_safety_system;
        bool use_ground_contact_model;
        bool use_adaptive_gait;
        bool use_preset_motions;

        FeatureFlags()
            : use_velocity_control(false)
            , use_motion_modes(false)
            , use_safety_system(true)
            , use_ground_contact_model(false)
            , use_adaptive_gait(false)
            , use_preset_motions(false)
        {}
    } feature_flags_;

    // 速度控制
    std::unique_ptr<aurora::gait::VelocityLocomotionController> velocity_controller_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr velocity_cmd_sub_;

    // 运动模式管理
    std::unique_ptr<aurora::gait::MotionModeManager> motion_mode_manager_;
    aurora::gait::MotionMode current_motion_mode_;

    // 安全系统
    std::unique_ptr<aurora::gait::SafetySystem> safety_system_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr safety_status_pub_;

    // 地面接触模型
    std::unique_ptr<aurora::gait::GroundContactModel> ground_contact_model_;
    std::unique_ptr<aurora::gait::ContactHistoryRecorder> contact_history_recorder_;

    // 自适应步态控制器
    std::unique_ptr<aurora::gait::AdaptiveGaitController> adaptive_gait_controller_;
    aurora::gait::EnvironmentFactors current_environment_factors_;

    // 预设动作库
    std::unique_ptr<aurora::gait::PresetMotionLibrary> preset_motion_library_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr preset_motion_sub_;

    // ========== 步态配置 ==========
    aurora::gait::GaitParameters gait_params_;
    aurora::gait::LegGeometry leg_geometry_;

    // ========== 机器人状态 ==========
    double robot_x_, robot_y_, robot_z_;      // 躯干位置
    double robot_roll_, robot_pitch_, robot_yaw_;  // 躯干姿态
    double robot_vx_, robot_vy_, robot_vz_;   // 线速度
    double robot_wx_, robot_wy_, robot_wz_;   // 角速度

    // ========== 足端状态（相对于躯干） ==========
    aurora::gait::FootPosition left_foot_position_;
    aurora::gait::FootPosition right_foot_position_;

    // ========== 足端世界坐标位置 ==========
    aurora::gait::FootPosition left_foot_world_;
    aurora::gait::FootPosition right_foot_world_;

    // ========== 足端轨迹目标 ==========
    aurora::gait::FootPosition left_foot_target_;
    aurora::gait::FootPosition right_foot_target_;

    // ========== 支撑状态 ==========
    bool left_foot_stance_;
    bool right_foot_stance_;

    // ========== 关节状态 ==========
    static constexpr int NUM_JOINTS = 12;
    std::vector<std::string> joint_names_;
    std::vector<double> joint_positions_;
    std::vector<double> joint_velocities_;

    // ========== 目标路径 ==========
    std::vector<std::pair<double, double>> target_path_;
    size_t current_waypoint_index_;

    // ========== Waypoint wait mechanism ==========
    // 等待DCP采集数据的时间（秒），在每个航点到达后等待
    double waypoint_wait_time_ = 0.0;  // 默认0秒（不等待），可设置为3-5秒
    rclcpp::Time last_waypoint_reach_time_;
    bool waiting_at_waypoint_;

    // ========== 误差记录 ==========
    std::vector<CollectionError> collection_errors_;
    double waypoint_tolerance_ = 0.15;  // waypoint 到达容差（米）
    size_t max_collection_errors_ = 1000;  // 最大误差记录数（防止内存无限增长）

    // ========== 步态事件回调 ==========
    void setupGaitEventCallback();

    // ========== 控制标志 ==========
    bool simulation_running_;

    // ========== 线程安全保护 ==========
    // 保护机器人位置、姿态、速度等状态变量
    mutable std::mutex state_mutex_;
    // 保护目标路径和当前路点索引
    mutable std::mutex path_mutex_;
    // 保护采集误差记录
    mutable std::mutex error_mutex_;

    // ========== 性能优化：快速三角函数查找表 ==========
    // 使用引用避免重复创建单例调用
    aurora::performance::FastTrigonometry& fast_trig_;

    // ========== 内部方法 ==========

    void updateSimulation();
    void publishMessages();
    void publishRobotDescription();
    void publishStaticTransforms();

    /**
     * @brief 更新步态
     */
    void updateGait();

    /**
     * @brief 更新足端世界坐标位置（核心：实现关节驱动行走）
     */
    void updateFootWorldPositions();

    /**
     * @brief 从足端位置计算躯干位置
     */
    void updateTorsoFromFeet();

    /**
     * @brief 更新关节角度
     */
    void updateJointAngles();

    /**
     * @brief 路径跟踪控制（waypoint模式）
     */
    void pathTrackingControl();

    /**
     * @brief 速度命令控制（velocity模式）
     * 将速度命令直接映射到步态参数，无waypoint到达检查
     */
    void velocityControlUpdate();

    /**
     * @brief 生成摆动目标位置
     */
    aurora::gait::FootPosition generateSwingTarget(aurora::gait::LegID leg_id);

    /**
     * @brief 辅助函数 - 四元数转欧拉角
     */
    std::array<double, 3> quaternionToEuler(double x, double y, double z, double w);

    /**
     * @brief 辅助函数 - 欧拉角转四元数
     */
    std::array<double, 4> eulerToQuaternion(double roll, double pitch, double yaw);

    // ========== Zhiyuan集成方法 ==========

    /**
     * @brief 初始化Zhiyuan集成组件
     */
    void initializeZhiyuanComponents();

    /**
     * @brief 速度命令回调
     */
    void velocityCommandCallback(const geometry_msgs::msg::Twist::SharedPtr msg);

    /**
     * @brief 预设动作命令回调
     */
    void presetMotionCallback(const std_msgs::msg::String::SharedPtr msg);

    /**
     * @brief 更新安全系统
     */
    void updateSafetySystem();

    /**
     * @brief 更新地面接触
     */
    void updateGroundContact();

    /**
     * @brief 更新自适应步态
     */
    void updateAdaptiveGait();

    /**
     * @brief 处理安全事件
     */
    void handleSafetyEvent(aurora::gait::SafetyEvent event, const std::string& message);

    /**
     * @brief 处理运动模式事件
     */
    void handleMotionModeEvent(
        aurora::gait::MotionMode from,
        aurora::gait::MotionMode to,
        aurora::gait::ModeEvent event,
        aurora::gait::ModeTransitionResult result);
};

} // namespace aurora::sim

#endif // ROBOT_SIMULATOR_V2_H
