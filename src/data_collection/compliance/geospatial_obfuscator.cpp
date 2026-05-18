// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#include "geospatial_obfuscator.h"

#include <cmath>
#include <cstring>

#include <rclcpp/rclcpp.hpp>
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/serialization.hpp"

#include "common/log/logger.h"

namespace aurora::collector::compliance {

GeospatialObfuscator::GeospatialObfuscator(double radius_meters, uint64_t session_seed)
    : radius_meters_(radius_meters) {
    // Generate a deterministic offset for the entire session
    std::mt19937 gen(static_cast<std::mt19937::result_type>(session_seed));
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radius_dist(0.5 * radius_meters, radius_meters);
    std::uniform_real_distribution<double> rotation_dist(-M_PI, M_PI);

    double theta = angle_dist(gen);
    double r = radius_dist(gen);
    offset_x_ = r * std::cos(theta);
    offset_y_ = r * std::sin(theta);
    rotation_rad_ = rotation_dist(gen);

    // Do not log the actual transform. It is privacy-sensitive and would make
    // the obfuscation reversible from logs.
    AD_INFO(GeospatialObfuscator, "Initialized session spatial transform: radius=%.1fm", radius_meters_);
}

bool GeospatialObfuscator::obfuscate(std::vector<uint8_t>& cdr_buffer) {
    try {
        // Deserialize CDR buffer into Odometry message
        rclcpp::SerializedMessage serialized_msg(cdr_buffer.size());
        auto& rcl_msg = serialized_msg.get_rcl_serialized_message();
        std::memcpy(rcl_msg.buffer, cdr_buffer.data(), cdr_buffer.size());
        rcl_msg.buffer_length = cdr_buffer.size();

        rclcpp::Serialization<nav_msgs::msg::Odometry> serializer;
        nav_msgs::msg::Odometry odom;
        serializer.deserialize_message(&serialized_msg, &odom);

        // Apply a session-scoped SE(2) transform instead of a plain offset.
        // This preserves local geometry for learning while avoiding direct raw
        // odom coordinates in recorded data.
        const double x = odom.pose.pose.position.x;
        const double y = odom.pose.pose.position.y;
        const double cos_r = std::cos(rotation_rad_);
        const double sin_r = std::sin(rotation_rad_);
        odom.pose.pose.position.x = cos_r * x - sin_r * y + offset_x_;
        odom.pose.pose.position.y = sin_r * x + cos_r * y + offset_y_;

        const auto& q = odom.pose.pose.orientation;
        const double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
        const double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
        const double yaw = std::atan2(siny_cosp, cosy_cosp) + rotation_rad_;
        odom.pose.pose.orientation.x = 0.0;
        odom.pose.pose.orientation.y = 0.0;
        odom.pose.pose.orientation.z = std::sin(yaw * 0.5);
        odom.pose.pose.orientation.w = std::cos(yaw * 0.5);

        // Re-serialize back to CDR
        rclcpp::SerializedMessage output_msg(cdr_buffer.size() + 64);
        serializer.serialize_message(&odom, &output_msg);

        auto& out_rcl = output_msg.get_rcl_serialized_message();
        cdr_buffer.assign(out_rcl.buffer, out_rcl.buffer + out_rcl.buffer_length);
        return true;
    } catch (const std::exception& e) {
        AD_WARN(GeospatialObfuscator, "Failed to obfuscate odometry: %s", e.what());
        return false;
    }
}

}  // namespace aurora::collector::compliance
