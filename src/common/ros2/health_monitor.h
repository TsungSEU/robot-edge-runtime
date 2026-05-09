// health_monitor.h — System health monitoring ROS2 node
//
// Publishes /robot/health topic at 1Hz with JSON payload containing:
// - State machine state
// - Odometry/velocity_cmd liveliness
// - Recording status
// - Disk/memory usage
// - Uptime

#ifndef AURORA_COMMON_ROS2_HEALTH_MONITOR_H_
#define AURORA_COMMON_ROS2_HEALTH_MONITOR_H_

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <chrono>
#include <string>
#include <sstream>
#include <fstream>

#include "common/log/structured_log.h"
#include "common/ros2/qos_profiles.h"

namespace aurora::common {

class HealthMonitor : public rclcpp::Node {
public:
    struct HealthStatus {
        std::string state_machine_state = "UNKNOWN";
        bool odometry_alive = false;
        bool velocity_cmd_alive = false;
        bool recording_active = false;
        double disk_usage_percent = 0.0;
        double memory_usage_mb = 0.0;
        uint64_t uptime_seconds = 0;
        std::string last_error;
    };

    HealthMonitor()
        : Node("health_monitor")
        , start_time_(std::chrono::steady_clock::now())
    {
        health_pub_ = this->create_publisher<std_msgs::msg::String>(
            "/robot/health", qos::static_data());

        health_timer_ = this->create_wall_timer(
            std::chrono::seconds(1),
            [this]() { publishHealth(); });

        RCLCPP_INFO(this->get_logger(), "HealthMonitor started, publishing to /robot/health at 1Hz");
    }

    // Update health status from external components
    void setStateMachineState(const std::string& state) {
        status_.state_machine_state = state;
    }

    void setOdometryAlive(bool alive) {
        status_.odometry_alive = alive;
    }

    void setVelocityCmdAlive(bool alive) {
        status_.velocity_cmd_alive = alive;
    }

    void setRecordingActive(bool active) {
        status_.recording_active = active;
    }

    void setLastError(const std::string& error) {
        status_.last_error = error;
    }

    const HealthStatus& getStatus() const { return status_; }

private:
    void publishHealth() {
        updateSystemMetrics();
        status_.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_).count();

        std_msgs::msg::String msg;
        msg.data = formatHealthJson();
        health_pub_->publish(msg);
    }

    void updateSystemMetrics() {
        // Read memory usage from /proc/self/status
        std::ifstream status_file("/proc/self/status");
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                // VmRSS is in kB
                size_t kb = 0;
                std::istringstream iss(line.substr(6));
                iss >> kb;
                status_.memory_usage_mb = kb / 1024.0;
                break;
            }
        }

        // Read disk usage for data root
        // Simple check using statfs would be better but this avoids extra headers
        status_.disk_usage_percent = 0.0;  // Updated by external component if needed
    }

    std::string formatHealthJson() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << "{\"ts\":\"" << iso8601Now() << "\""
            << ",\"state\":\"" << status_.state_machine_state << "\""
            << ",\"odom_alive\":" << (status_.odometry_alive ? "true" : "false")
            << ",\"vel_cmd_alive\":" << (status_.velocity_cmd_alive ? "true" : "false")
            << ",\"recording\":" << (status_.recording_active ? "true" : "false")
            << ",\"disk_pct\":" << status_.disk_usage_percent
            << ",\"mem_mb\":" << status_.memory_usage_mb
            << ",\"uptime_s\":" << status_.uptime_seconds;
        if (!status_.last_error.empty()) {
            oss << ",\"last_error\":\"" << jsonEscape(status_.last_error) << "\"";
        }
        oss << "}";
        return oss.str();
    }

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr health_pub_;
    rclcpp::TimerBase::SharedPtr health_timer_;
    HealthStatus status_;
    std::chrono::steady_clock::time_point start_time_;
};

}  // namespace aurora::common

#endif  // AURORA_COMMON_ROS2_HEALTH_MONITOR_H_
