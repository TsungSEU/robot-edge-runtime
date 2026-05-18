//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
//

#ifndef ROBOT_CONTROLLER_H
#define ROBOT_CONTROLLER_H

#include <vector>
#include <functional>
#include <chrono>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <mutex>

#include "rl_planning_infer/maps/costmap.h"
#include "data_collection/common/types.h"
#include "data_collection/common/runtime_config.h"
#include "data_collection/robot_position_tracker.h"
#include "common/ros2/qos_profiles.h"
#include "common/log/logger.h"

namespace aurora::collector {

/**
 * @brief 机器人控制器
 *
 * 从 DataCollectionPlanner 的 3 个 execute 方法中提取的统一执行模板。
 * 支持 path_tracking（waypoint 等待）和 velocity_cmd（速度控制）两种模式。
 *
 * 消除 ~400 行重复代码，将 waypoint 迭代、采集判断、可视化更新
 * 统一到 executePath() 模板中。
 */
class RobotController {
public:
    /**
     * @brief 执行结果
     */
    struct ExecutionResult {
        std::vector<DataPoint> collected_data;
        double execution_time = 0.0;
        double distance_traveled = 0.0;
        int waypoints_reached = 0;
    };

    /**
     * @brief 采集判断回调
     * @param waypoint 当前 waypoint
     * @param index waypoint 索引
     * @return 采集到的数据点（nullopt 表示跳过）
     */
    using CollectionChecker = std::function<std::optional<DataPoint>(const Point&, size_t)>;

    /**
     * @brief Waypoint 到达回调（用于反馈、可视化等）
     */
    using WaypointCallback = std::function<void(const Point&, size_t)>;

    RobotController(
        RobotPositionTracker& position_tracker,
        rclcpp::Node* node,
        const config::VelocityControlParams& params);

    ~RobotController() = default;

    /**
     * @brief 统一执行路径（path_tracking 模式）
     *
     * 逐 waypoint 等待机器人到达，在每个 waypoint 尝试采集。
     * 替代原 executeWithFeedback() 和 executeHumanoidWithFeedback()。
     *
     * @param path 规划的路径
     * @param checker 采集判断回调
     * @param on_reached waypoint 到达后的回调
     * @param wait_timeout waypoint 等待超时
     * @return 执行结果
     */
    ExecutionResult executePathTracking(
        const std::vector<Point>& path,
        CollectionChecker checker,
        WaypointCallback on_reached,
        double wait_timeout);

    /**
     * @brief 统一执行路径（velocity_cmd 模式）
     *
     * 闭环速度控制，自动计算朝向和速度指令驱动机器人。
     * 替代原 executeWithVelocityCommands()。
     *
     * @param path 规划的路径
     * @param checker 采集判断回调
     * @param on_reached waypoint 到达后的回调
     * @return 执行结果
     */
    ExecutionResult executeVelocityCommands(
        const std::vector<Point>& path,
        CollectionChecker checker,
        WaypointCallback on_reached);

    /**
     * @brief 发送速度指令
     */
    void sendVelocityCommand(double forward, double lateral, double angular);

    /**
     * @brief 发送停止指令
     */
    void sendStopCommand();

    struct VelocityFeedback {
        double forward = 0.0;
        double lateral = 0.0;
        double angular = 0.0;
        std::chrono::steady_clock::time_point last_update;
        bool initialized = false;
    };

    VelocityFeedback getVelocityFeedback() const;

private:
    RobotPositionTracker& position_tracker_;
    config::VelocityControlParams params_;

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;

    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_feedback_sub_;
    rclcpp::CallbackGroup::SharedPtr feedback_callback_group_;
    VelocityFeedback velocity_feedback_;
    mutable std::mutex feedback_mutex_;
    std::chrono::steady_clock::time_point last_vel_warn_time_;

    void onCmdVelFeedback(const geometry_msgs::msg::Twist::SharedPtr msg);
};

} // namespace aurora::collector

#endif // ROBOT_CONTROLLER_H
