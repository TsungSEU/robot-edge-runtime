// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#include "compliance_filter.h"

#include <vector>

#include "common/log/logger.h"

namespace aurora::collector::compliance {

ComplianceFilter::ComplianceFilter(std::shared_ptr<rclcpp::Node> node,
                                   const ComplianceConfig& config)
    : node_(std::move(node)), config_(config) {
    if (config_.geo_enabled) {
        auto seed = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        geo_ = std::make_unique<GeospatialObfuscator>(config_.geo_radius, seed);
    }
    if (config_.image_enabled) {
        image_ = std::make_unique<ImageDesensitizer>(config_.image_blur_kernel);
    }
    AD_INFO(ComplianceFilter, "Initialized: geo=%d image=%d",
            config_.geo_enabled, config_.image_enabled);
}

void ComplianceFilter::setDownstream(const std::shared_ptr<Observer>& downstream) {
    downstream_ = downstream;
}

bool ComplianceFilter::isOdomTopic(const std::string& topic) const {
    return topic.find("/odom") != std::string::npos;
}

bool ComplianceFilter::isImageTopic(const std::string& topic) const {
    return topic.find("/camera/") != std::string::npos;
}

bool ComplianceFilter::isDepthTopic(const std::string& topic) const {
    return topic.find("/depth") != std::string::npos;
}

void ComplianceFilter::OnMessageReceived(const std::string& topic,
                                          const rclcpp::SerializedMessage& msg) {
    if (!downstream_) return;

    try {
        const auto& rcl_msg = msg.get_rcl_serialized_message();
        std::vector<uint8_t> buffer(rcl_msg.buffer, rcl_msg.buffer + rcl_msg.buffer_length);
        bool modified = false;

        // Geospatial obfuscation for odometry topics
        if (config_.geo_enabled && geo_ && isOdomTopic(topic)) {
            modified = geo_->obfuscate(buffer);
        }

        // Image desensitization for camera topics
        if (config_.image_enabled && image_ && isImageTopic(topic)) {
            if (!isDepthTopic(topic) || config_.image_depth) {
                modified = image_->desensitize(buffer, topic) || modified;
            }
        }

        if (modified) {
            // Forward with modified buffer
            rclcpp::SerializedMessage filtered_msg(buffer.size());
            auto& out_rcl = filtered_msg.get_rcl_serialized_message();
            if (out_rcl.buffer_capacity < buffer.size()) {
                out_rcl.buffer = reinterpret_cast<uint8_t*>(
                    rcutils_uint8_array_resize(&out_rcl, buffer.size()));
            }
            std::memcpy(out_rcl.buffer, buffer.data(), buffer.size());
            out_rcl.buffer_length = buffer.size();
            downstream_->OnMessageReceived(topic, filtered_msg);
        } else {
            // Forward unmodified
            downstream_->OnMessageReceived(topic, msg);
        }
    } catch (const std::exception& e) {
        AD_ERROR(ComplianceFilter,
                 "Compliance filter error on %s: %s, forwarding raw",
                 topic.c_str(), e.what());
        downstream_->OnMessageReceived(topic, msg);
    }
}

}  // namespace aurora::collector::compliance
