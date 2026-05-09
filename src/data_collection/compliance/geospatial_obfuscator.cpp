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

    double theta = angle_dist(gen);
    double r = radius_dist(gen);
    offset_x_ = r * std::cos(theta);
    offset_y_ = r * std::sin(theta);

    AD_INFO(GeospatialObfuscator, "Initialized: radius=%.1fm, offset=(%.3f, %.3f)",
            radius_meters_, offset_x_, offset_y_);
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

        // Apply deterministic offset
        odom.pose.pose.position.x += offset_x_;
        odom.pose.pose.position.y += offset_y_;

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
