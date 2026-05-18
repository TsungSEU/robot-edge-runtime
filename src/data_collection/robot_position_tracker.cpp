//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#include "robot_position_tracker.h"
#include "common/ros2/qos_callbacks.h"

#include <cmath>
#include <iostream>

namespace aurora::collector {

RobotPositionTracker::RobotPositionTracker(rclcpp::Node* node) {
    // 使用隔离的 callback group 避免阻塞其他回调
    odom_callback_group_ = node->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions sub_options;
    sub_options.callback_group = odom_callback_group_;

    odom_sub_ = node->create_subscription<nav_msgs::msg::Odometry>(
        "/robot/odom", aurora::common::qos::odometry(),
        [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
            this->onOdometry(msg);
        },
        sub_options);

    aurora::common::qos::installDeadlineCallback<nav_msgs::msg::Odometry>(
        odom_sub_, "/robot/odom");

    AD_INFO(RobotPositionTracker, "Odometry subscriber created: /robot/odom");
}

void RobotPositionTracker::onOdometry(const nav_msgs::msg::Odometry::SharedPtr msg) {
    bool should_notify = false;
    Point current_pos;
    {
        std::lock_guard<std::mutex> lock(position_mutex_);

        position_.x = msg->pose.pose.position.x;
        position_.y = msg->pose.pose.position.y;

        // 四元数转欧拉角 (yaw)
        double qx = msg->pose.pose.orientation.x;
        double qy = msg->pose.pose.orientation.y;
        double qz = msg->pose.pose.orientation.z;
        double qw = msg->pose.pose.orientation.w;
        double siny_cosp = 2.0 * (qw * qz + qx * qy);
        double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
        position_.yaw = std::atan2(siny_cosp, cosy_cosp);

        position_.last_update = std::chrono::steady_clock::now();
        position_.initialized = true;
        odometry_received_.store(true);

        current_pos = Point(position_.x, position_.y);

        // Waypoint 到达判定
        if (waypoint_waiting_.load()) {
            double dx = position_.x - target_waypoint_.x;
            double dy = position_.y - target_waypoint_.y;
            double distance = std::sqrt(dx * dx + dy * dy);
            // 默认容差在 waitForWaypoint 调用时设置
            if (distance <= waypoint_reach_tolerance_) {
                waypoint_reached_.store(true);
                should_notify = true;
            }
        }
    }

    // 通知 TriggerManager 等外部监听器
    if (position_callback_) {
        position_callback_(current_pos);
    }

    if (should_notify) {
        waypoint_cv_.notify_one();
    }
}

Point RobotPositionTracker::getCurrentPosition() const {
    std::lock_guard<std::mutex> lock(position_mutex_);

    if (!position_.initialized) {
        static bool logged_once = false;
        if (!logged_once) {
            AD_WARN(RobotPositionTracker, "Robot position not initialized yet, returning (0, 0)");
            logged_once = true;
        }
        return Point(0.0, 0.0);
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - position_.last_update).count();

    static std::chrono::steady_clock::time_point last_stale_log;
    if (elapsed > 1) {
        auto time_since_last_log = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_stale_log).count();
        if (time_since_last_log > 10) {
            AD_WARN(RobotPositionTracker, "Robot position data is stale (%ld seconds old)", elapsed);
            last_stale_log = now;
        }
    }

    return Point(position_.x, position_.y);
}

double RobotPositionTracker::getCurrentYaw() const {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return position_.yaw;
}

bool RobotPositionTracker::isInitialized() const {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return position_.initialized;
}

bool RobotPositionTracker::waitForWaypoint(const Point& waypoint, double tolerance, double timeout_seconds) {
    if (!odom_sub_) {
        AD_ERROR(RobotPositionTracker, "Odometry subscriber is null! Cannot wait for waypoint.");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(position_mutex_);
        if (position_.initialized) {
            auto now = std::chrono::steady_clock::now();
            auto odom_age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - position_.last_update).count();
            AD_DEBUG(RobotPositionTracker, "Current robot position: (%.2f, %.2f), odometry age: %ld ms",
                    position_.x, position_.y, odom_age);
        } else {
            AD_WARN(RobotPositionTracker, "Robot position not initialized yet");
        }
    }

    // 设置等待目标
    {
        std::lock_guard<std::mutex> lock(waypoint_mutex_);
        target_waypoint_ = waypoint;
        waypoint_reach_tolerance_ = tolerance;
        waypoint_reached_.store(false);
        waypoint_waiting_.store(true);
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_seconds);

    std::unique_lock<std::mutex> lock(waypoint_mutex_);
    bool reached = waypoint_cv_.wait_until(lock, deadline, [this]() {
        return waypoint_reached_.load();
    });

    waypoint_waiting_.store(false);
    waypoint_reached_.store(false);

    if (reached) {
        Point current_pos = getCurrentPosition();
        double dx = current_pos.x - waypoint.x;
        double dy = current_pos.y - waypoint.y;
        double distance = std::sqrt(dx * dx + dy * dy);
        AD_INFO(RobotPositionTracker, "Robot reached waypoint (%.2f, %.2f), distance: %.3f m",
                waypoint.x, waypoint.y, distance);
        return true;
    } else {
        {
            std::lock_guard<std::mutex> pos_lock(position_mutex_);
            if (position_.initialized) {
                auto now = std::chrono::steady_clock::now();
                auto odom_age = std::chrono::duration_cast<std::chrono::seconds>(
                    now - position_.last_update).count();
                if (odom_age > 2) {
                    AD_ERROR(RobotPositionTracker, "Odometry data is stale! Last update %ld seconds ago.",
                            odom_age);
                }
            }
        }
        AD_WARN(RobotPositionTracker, "Timeout waiting for waypoint (%.2f, %.2f)",
                waypoint.x, waypoint.y);
        return false;
    }
}

} // namespace aurora::collector
