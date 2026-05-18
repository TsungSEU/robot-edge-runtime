//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
//

#include "robot_controller.h"
#include "common/ros2/qos_callbacks.h"

#include <cmath>
#include <algorithm>

namespace aurora::collector {

RobotController::RobotController(
    RobotPositionTracker& position_tracker,
    rclcpp::Node* node,
    const config::VelocityControlParams& params)
    : position_tracker_(position_tracker)
    , params_(params) {

    cmd_vel_pub_ = node->create_publisher<geometry_msgs::msg::Twist>(
        "/robot/velocity_cmd", aurora::common::qos::velocity_cmd());

    feedback_callback_group_ = node->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions sub_options;
    sub_options.callback_group = feedback_callback_group_;

    cmd_vel_feedback_sub_ = node->create_subscription<geometry_msgs::msg::Twist>(
        "/robot/cmd_vel", aurora::common::qos::velocity_cmd(),
        [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
            this->onCmdVelFeedback(msg);
        },
        sub_options);

    aurora::common::qos::installLivelinessCallback<geometry_msgs::msg::Twist>(
        cmd_vel_pub_, "/robot/velocity_cmd");
    aurora::common::qos::installDeadlineCallback<geometry_msgs::msg::Twist>(
        cmd_vel_feedback_sub_, "/robot/cmd_vel");

    AD_INFO(RobotController, "RobotController initialized, pub: /robot/velocity_cmd, sub: /robot/cmd_vel");
}

RobotController::ExecutionResult RobotController::executePathTracking(
    const std::vector<Point>& path,
    CollectionChecker checker,
    WaypointCallback on_reached,
    double wait_timeout) {

    ExecutionResult result;
    auto start_time = std::chrono::steady_clock::now();

    for (size_t i = 0; i < path.size(); ++i) {
        const Point& waypoint = path[i];

        // 等待机器人到达 waypoint
        bool reached = position_tracker_.waitForWaypoint(
            waypoint, 0.3, wait_timeout);

        if (!reached) {
            AD_WARN(RobotController, "Failed to reach waypoint %zu (%.2f, %.2f), skipping",
                    i, waypoint.x, waypoint.y);
            continue;
        }

        result.waypoints_reached++;

        // 尝试采集
        if (checker) {
            auto data_point = checker(waypoint, i);
            if (data_point.has_value()) {
                result.collected_data.push_back(data_point.value());
                AD_INFO(RobotController, "Collected at waypoint %zu (%.2f, %.2f)",
                        i, waypoint.x, waypoint.y);
            }
        }

        // waypoint 到达回调
        if (on_reached) {
            on_reached(waypoint, i);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration<double>(end_time - start_time).count();

    AD_INFO(RobotController, "Path tracking completed: %zu/%zu waypoints, %zu data points",
            result.waypoints_reached, path.size(), result.collected_data.size());

    return result;
}

RobotController::ExecutionResult RobotController::executeVelocityCommands(
    const std::vector<Point>& path,
    CollectionChecker checker,
    WaypointCallback on_reached) {

    ExecutionResult result;
    if (path.size() < 2) {
        AD_WARN(RobotController, "No valid path for velocity execution");
        return result;
    }

    AD_INFO(RobotController, "Closed-loop velocity tracking with %zu waypoints",
            path.size());

    auto start_time = std::chrono::steady_clock::now();

    const double wp_tolerance = params_.waypoint_tolerance;
    const double wp_timeout = 15.0;
    const int trail_update_interval = 3;
    const int max_loops = 5000;

    size_t current_wp = 1;
    auto wp_start_time = std::chrono::steady_clock::now();
    int last_log_wp = -1;

    for (int loop = 0; loop < max_loops && current_wp < path.size(); ++loop) {
        Point robot_pos = position_tracker_.getCurrentPosition();
        double robot_yaw = position_tracker_.getCurrentYaw();
        const Point& target = path[current_wp];

        double dx = target.x - robot_pos.x;
        double dy = target.y - robot_pos.y;
        double dist = std::sqrt(dx * dx + dy * dy);

        // 进度日志
        if (static_cast<int>(current_wp) != last_log_wp) {
            last_log_wp = static_cast<int>(current_wp);
            AD_INFO(RobotController, "Navigating to waypoint %zu/%zu (%.2f, %.2f), dist=%.2fm",
                    current_wp, path.size() - 1, target.x, target.y, dist);
        }

        // 单个 waypoint 超时
        double wp_elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - wp_start_time).count();
        if (wp_elapsed > wp_timeout) {
            AD_WARN(RobotController, "Waypoint %zu timeout (%.1fs), skipping",
                    current_wp, wp_elapsed);
            current_wp++;
            wp_start_time = std::chrono::steady_clock::now();
            continue;
        }

        // 到达当前 waypoint
        if (dist < wp_tolerance) {
            result.waypoints_reached++;

            // 尝试采集
            if (checker) {
                auto data_point = checker(target, current_wp);
                if (data_point.has_value()) {
                    result.collected_data.push_back(data_point.value());
                    AD_INFO(RobotController, "Collected at waypoint %zu (%.2f, %.2f)",
                            current_wp, target.x, target.y);
                } else {
                    // cooldown 后重试一次
                    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                    data_point = checker(target, current_wp);
                    if (data_point.has_value()) {
                        result.collected_data.push_back(data_point.value());
                        AD_INFO(RobotController, "Collected at waypoint %zu after retry",
                                current_wp);
                    }
                }
            }

            // waypoint 到达回调
            if (on_reached) {
                on_reached(target, current_wp);
            }

            current_wp++;
            wp_start_time = std::chrono::steady_clock::now();
            continue;
        }

        // 计算速度指令
        double target_yaw = std::atan2(dy, dx);
        double yaw_error = target_yaw - robot_yaw;
        while (yaw_error > M_PI) yaw_error -= 2.0 * M_PI;
        while (yaw_error < -M_PI) yaw_error += 2.0 * M_PI;

        double alignment = std::cos(yaw_error);
        double forward = alignment > 0.0
                       ? std::min(params_.max_forward, dist * 2.0) * alignment
                       : 0.0;

        double angular = std::clamp(params_.kp_angular * yaw_error,
                                    -params_.max_angular, params_.max_angular);

        sendVelocityCommand(forward, 0.0, angular);

        auto fb = getVelocityFeedback();
        if (fb.initialized) {
            double vel_error = std::abs(forward - fb.forward);
            if (vel_error > 0.2) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_vel_warn_time_ > std::chrono::seconds(2)) {
                    last_vel_warn_time_ = now;
                    AD_WARN(RobotController, "Velocity tracking error: cmd=%.3f actual=%.3f",
                            forward, fb.forward);
                }
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(params_.control_period_ms)));
    }

    // 停止机器人
    sendStopCommand();

    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration<double>(end_time - start_time).count();

    AD_INFO(RobotController, "Velocity execution completed: %zu/%zu waypoints, %zu data points",
            current_wp, path.size(), result.collected_data.size());

    return result;
}

void RobotController::sendVelocityCommand(double forward, double lateral, double angular) {
    geometry_msgs::msg::Twist cmd_msg;
    cmd_msg.linear.x = forward;
    cmd_msg.linear.y = lateral;
    cmd_msg.angular.z = angular;
    cmd_vel_pub_->publish(cmd_msg);
}

void RobotController::sendStopCommand() {
    geometry_msgs::msg::Twist stop_msg;
    stop_msg.linear.x = 0.0;
    stop_msg.linear.y = 0.0;
    stop_msg.angular.z = 0.0;
    cmd_vel_pub_->publish(stop_msg);
}

void RobotController::onCmdVelFeedback(const geometry_msgs::msg::Twist::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(feedback_mutex_);
    velocity_feedback_.forward = msg->linear.x;
    velocity_feedback_.lateral = msg->linear.y;
    velocity_feedback_.angular = msg->angular.z;
    velocity_feedback_.last_update = std::chrono::steady_clock::now();
    velocity_feedback_.initialized = true;
}

RobotController::VelocityFeedback RobotController::getVelocityFeedback() const {
    std::lock_guard<std::mutex> lock(feedback_mutex_);
    return velocity_feedback_;
}

} // namespace aurora::collector
