// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

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

        const auto q = odom.pose.pose.orientation;
        const double half_rotation = rotation_rad_ * 0.5;
        const double rz = std::sin(half_rotation);
        const double rw = std::cos(half_rotation);

        // Left-multiply by a yaw-only rotation so roll/pitch information is
        // preserved while the odom frame is consistently rotated.
        odom.pose.pose.orientation.x = rw * q.x - rz * q.y;
        odom.pose.pose.orientation.y = rw * q.y + rz * q.x;
        odom.pose.pose.orientation.z = rw * q.z + rz * q.w;
        odom.pose.pose.orientation.w = rw * q.w - rz * q.z;

        const double norm = std::sqrt(
            odom.pose.pose.orientation.x * odom.pose.pose.orientation.x +
            odom.pose.pose.orientation.y * odom.pose.pose.orientation.y +
            odom.pose.pose.orientation.z * odom.pose.pose.orientation.z +
            odom.pose.pose.orientation.w * odom.pose.pose.orientation.w);
        if (norm > 0.0) {
            odom.pose.pose.orientation.x /= norm;
            odom.pose.pose.orientation.y /= norm;
            odom.pose.pose.orientation.z /= norm;
            odom.pose.pose.orientation.w /= norm;
        }

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
