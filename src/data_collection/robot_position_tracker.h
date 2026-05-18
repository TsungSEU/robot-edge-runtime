//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
//

#ifndef ROBOT_POSITION_TRACKER_H
#define ROBOT_POSITION_TRACKER_H

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>

#include "rl_planning_infer/maps/costmap.h"  // For Point
#include "common/ros2/qos_profiles.h"
#include "common/log/logger.h"

namespace aurora::collector {

/**
 * @brief 机器人位置跟踪器
 *
 * 从 DataCollectionPlanner 中提取的里程计跟踪和 waypoint 等待逻辑。
 * 管理 /robot/odom 订阅、位置状态、waypoint 到达判定。
 */
class RobotPositionTracker {
public:
    explicit RobotPositionTracker(rclcpp::Node* node);

    ~RobotPositionTracker() = default;

    /**
     * @brief 获取当前机器人位置（线程安全）
     */
    Point getCurrentPosition() const;

    /**
     * @brief 获取当前机器人航向角（线程安全）
     */
    double getCurrentYaw() const;

    /**
     * @brief 里程计是否已初始化
     */
    bool isInitialized() const;

    /**
     * @brief 里程计数据是否已接收
     */
    bool isOdomReceived() const { return odometry_received_.load(); }

    /**
     * @brief 等待机器人到达指定 waypoint
     * @param waypoint 目标位置
     * @param tolerance 到达判定容差 (米)
     * @param timeout_seconds 超时时间 (秒)
     * @return true 到达, false 超时
     */
    bool waitForWaypoint(const Point& waypoint, double tolerance, double timeout_seconds);

    /**
     * @brief 注册位置更新回调（用于 TriggerManager 等）
     */
    using PositionCallback = std::function<void(const Point&)>;
    void setPositionCallback(PositionCallback cb) { position_callback_ = std::move(cb); }

private:
    /**
     * @brief 里程计回调
     */
    void onOdometry(const nav_msgs::msg::Odometry::SharedPtr msg);

    // --- 内部状态 ---
    struct Position {
        double x = 0.0;
        double y = 0.0;
        double yaw = 0.0;
        bool initialized = false;
        std::chrono::steady_clock::time_point last_update;
    } position_;
    mutable std::mutex position_mutex_;

    std::atomic<bool> odometry_received_{false};

    // Waypoint 等待同步
    std::condition_variable waypoint_cv_;
    mutable std::mutex waypoint_mutex_;
    Point target_waypoint_{0.0, 0.0};
    double waypoint_reach_tolerance_ = 0.3;
    std::atomic<bool> waypoint_reached_{false};
    std::atomic<bool> waypoint_waiting_{false};

    // 位置更新回调
    PositionCallback position_callback_;

    // ROS2 subscriber
    rclcpp::CallbackGroup::SharedPtr odom_callback_group_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
};

} // namespace aurora::collector

#endif // ROBOT_POSITION_TRACKER_H
