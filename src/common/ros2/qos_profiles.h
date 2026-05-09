// qos_profiles.h — ROS2 QoS profile constants for Aurora Edge Runtime
//
// Centralized QoS definitions for all ROS2 topics.
// Each profile is tuned for its specific use case:
// - Sensor data: high-frequency, deadline-critical
// - Odometry: reliable delivery, medium deadline
// - Velocity commands: critical control path, strict liveliness
// - Visualization: best-effort, loss-tolerant

#ifndef AURORA_COMMON_ROS2_QOS_PROFILES_H_
#define AURORA_COMMON_ROS2_QOS_PROFILES_H_

#include <rclcpp/qos.hpp>
#include <chrono>

namespace aurora::common::qos {

// Sensor data: IMU, joint states — high frequency, must not stall
// Deadline 100ms corresponds to 10Hz minimum update rate
inline rclcpp::QoS sensor_data() {
    return rclcpp::QoS(rclcpp::KeepLast(50))
        .reliable()
        .deadline(std::chrono::milliseconds(100))
        .liveliness(rclcpp::LivelinessPolicy::Automatic)
        .liveliness_lease_duration(std::chrono::milliseconds(300));
}

// Odometry: reliable delivery for control loop feedback
// Deadline 200ms allows for intermittent delays without false alarms
inline rclcpp::QoS odometry() {
    return rclcpp::QoS(rclcpp::KeepLast(10))
        .reliable()
        .deadline(std::chrono::milliseconds(200))
        .liveliness(rclcpp::LivelinessPolicy::Automatic)
        .liveliness_lease_duration(std::chrono::milliseconds(600));
}

// Velocity commands: critical control path — robot motion depends on this
// ManualByTopic liveliness ensures publisher is explicitly asserting activity
inline rclcpp::QoS velocity_cmd() {
    return rclcpp::QoS(rclcpp::KeepLast(5))
        .reliable()
        .deadline(std::chrono::milliseconds(500))
        .liveliness(rclcpp::LivelinessPolicy::ManualByTopic)
        .liveliness_lease_duration(std::chrono::milliseconds(1000));
}

// TF transforms: high-frequency, reliable for RViz compatibility
// RViz TransformListener requires RELIABLE; BEST_EFFORT causes incompatible QoS
inline rclcpp::QoS tf_transforms() {
    return rclcpp::QoS(rclcpp::KeepLast(50))
        .reliable()
        .deadline(std::chrono::milliseconds(100));
}

// Static data: robot description, parameters — reliable, transient-local for late joiners
inline rclcpp::QoS static_data() {
    return rclcpp::QoS(rclcpp::KeepLast(1))
        .reliable()
        .transient_local();
}

// Visualization markers: reliable for RViz compatibility
// RViz display plugins require RELIABLE; BEST_EFFORT causes incompatible QoS
inline rclcpp::QoS visualization() {
    return rclcpp::QoS(rclcpp::KeepLast(10))
        .reliable();
}

// Channel data collection: reliable, larger depth for burst data
inline rclcpp::QoS channel_data() {
    return rclcpp::QoS(rclcpp::KeepLast(100))
        .reliable();
}

}  // namespace aurora::common::qos

#endif  // AURORA_COMMON_ROS2_QOS_PROFILES_H_
