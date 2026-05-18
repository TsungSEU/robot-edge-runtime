// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "compliance_policy_parser.h"

#include <utility>

#include <algorithm>
#include <functional>
#include <sstream>

namespace aurora::collector::compliance_v2 {
namespace {

bool containsToken(const std::string& text, const std::string& token) {
    return text.find(token) != std::string::npos;
}

bool isVisualType(const std::string& type) {
    return containsToken(type, "sensor_msgs/msg/Image") ||
           containsToken(type, "sensor_msgs/msg/CompressedImage");
}

bool isGpsType(const std::string& type) {
    return containsToken(type, "sensor_msgs/msg/NavSatFix");
}

bool isSpatialType(const std::string& type) {
    return containsToken(type, "nav_msgs/msg/Odometry") ||
           containsToken(type, "geometry_msgs/msg/PoseStamped") ||
           containsToken(type, "nav_msgs/msg/Path") ||
           containsToken(type, "tf2_msgs/msg/TFMessage") ||
           containsToken(type, "sensor_msgs/msg/PointCloud2") ||
           containsToken(type, "visualization_msgs/msg/Marker");
}

std::string weakPolicyHash(const aurora::collector::Strategy& strategy) {
    std::ostringstream oss;
    oss << strategy.businessType << ':' << strategy.enableMasking << ':'
        << strategy.maskingConfig.geospatialOffsetRadius << ':'
        << strategy.maskingConfig.imageBlurKernelSize << ':'
        << strategy.cyclone.channels.size();
    std::hash<std::string> hasher;
    std::ostringstream out;
    out << "sha256:legacy-adapter-" << std::hex << hasher(oss.str());
    return out.str();
}

}  // namespace

CompliancePolicy CompliancePolicyParser::fromStrategy(const aurora::collector::Strategy& strategy) {
    CompliancePolicy policy;
    policy.policy_hash = weakPolicyHash(strategy);
    policy.visual.enabled = strategy.enableMasking;
    policy.visual.fallback_mosaic_block_size = std::max(2, strategy.maskingConfig.imageBlurKernelSize);
    policy.spatial.enabled = strategy.enableMasking;
    policy.spatial.local_radius_meters = strategy.maskingConfig.geospatialOffsetRadius;

    if (!strategy.enableMasking) return policy;

    for (const auto& channel : strategy.cyclone.channels) {
        TopicPolicy topic_policy;
        topic_policy.topic = channel.topic;
        topic_policy.message_type = channel.type;
        topic_policy.required = true;
        topic_policy.fail_mode = FailMode::FailClosed;

        if (isVisualType(channel.type)) {
            topic_policy.domain = PrivacyDomain::Visual;
        } else if (isGpsType(channel.type)) {
            topic_policy.domain = PrivacyDomain::SpatialGps;
        } else if (isSpatialType(channel.type)) {
            topic_policy.domain = PrivacyDomain::SpatialLocalFrame;
        } else {
            topic_policy.domain = PrivacyDomain::PassThrough;
            topic_policy.required = false;
        }
        policy.topics.emplace(channel.topic, topic_policy);
    }

    return policy;
}

}  // namespace aurora::collector::compliance_v2
