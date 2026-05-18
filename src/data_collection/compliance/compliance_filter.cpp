// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "compliance_filter.h"

#include <vector>

#include "common/log/logger.h"

namespace aurora::collector::compliance {

ComplianceFilter::ComplianceFilter(std::shared_ptr<rclcpp::Node> node,
                                   const ComplianceConfig& config)
    : node_(std::move(node)), config_(config) {
    if (!config_.v2_policy.topics.empty()) {
        v2_filter_ = std::make_unique<::aurora::collector::compliance_v2::ComplianceFilterV2>(node_, config_.v2_policy);
        AD_INFO(ComplianceFilter, "Initialized Compliance Pipeline V2: policy=%s topics=%zu",
                config_.v2_policy.policy_version.c_str(), config_.v2_policy.topics.size());
        return;
    }

    if (config_.geo_enabled) {
        auto seed = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        geo_ = std::make_unique<GeospatialObfuscator>(config_.geo_radius, seed);
    }
    if (config_.image_enabled) {
        image_ = std::make_unique<ImageDesensitizer>(config_.image_blur_kernel);
    }
    AD_INFO(ComplianceFilter, "Initialized legacy compliance fallback: geo=%d image=%d",
            config_.geo_enabled, config_.image_enabled);
}

void ComplianceFilter::setDownstream(const std::shared_ptr<Observer>& downstream) {
    downstream_ = downstream;
    if (v2_filter_) {
        v2_filter_->setDownstream(downstream);
    }
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
    if (v2_filter_) {
        v2_filter_->OnMessageReceived(topic, msg);
        return;
    }

    if (!downstream_) return;

    try {
        const auto& rcl_msg = msg.get_rcl_serialized_message();
        std::vector<uint8_t> buffer(rcl_msg.buffer, rcl_msg.buffer + rcl_msg.buffer_length);
        bool modified = false;
        const bool requires_geo = config_.geo_enabled && geo_ && isOdomTopic(topic);
        const bool requires_image = config_.image_enabled && image_ && isImageTopic(topic) &&
                                    (!isDepthTopic(topic) || config_.image_depth);

        // Geospatial obfuscation for odometry topics
        if (requires_geo) {
            const bool geo_ok = geo_->obfuscate(buffer);
            if (!geo_ok) {
                AD_ERROR(ComplianceFilter,
                         "Geospatial transform failed on %s, dropping raw message",
                         topic.c_str());
                return;
            }
            modified = true;
        }

        // Image desensitization for camera topics
        if (requires_image) {
            const bool image_ok = image_->desensitize(buffer, topic);
            if (!image_ok) {
                AD_ERROR(ComplianceFilter,
                         "Image desensitization failed on %s, dropping raw message",
                         topic.c_str());
                return;
            }
            modified = true;
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
        const bool compliance_topic =
            (config_.geo_enabled && isOdomTopic(topic)) ||
            (config_.image_enabled && isImageTopic(topic) &&
             (!isDepthTopic(topic) || config_.image_depth));
        AD_ERROR(ComplianceFilter,
                 "Compliance filter error on %s: %s, %s",
                 topic.c_str(), e.what(),
                 compliance_topic ? "dropping raw message" : "forwarding raw");
        if (!compliance_topic) {
            downstream_->OnMessageReceived(topic, msg);
        }
    }
}

}  // namespace aurora::collector::compliance
