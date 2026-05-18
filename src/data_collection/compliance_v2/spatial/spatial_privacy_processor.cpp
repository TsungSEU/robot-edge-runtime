// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "spatial_privacy_processor.h"

#include <cstring>
#include <type_traits>
#include <utility>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/serialization.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "tf2_msgs/msg/tf_message.hpp"
#include "visualization_msgs/msg/marker.hpp"

#include "common/log/logger.h"
#include "compliance_v2/spatial/pointcloud_transformer.h"
#include "compliance_v2/spatial/tf_pose_path_transformer.h"

namespace aurora::collector::compliance_v2 {
namespace {

bool hasType(const std::string& message_type, const std::string& type) {
    return message_type.find(type) != std::string::npos;
}

}  // namespace

SpatialPrivacyProcessor::SpatialPrivacyProcessor(SpatialPolicy policy, uint64_t session_seed)
    : policy_(std::move(policy)),
      local_transformer_(policy_.local_radius_meters, session_seed),
      gps_privacy_(policy_.gps_decimal_places, policy_.gps_laplace_scale_degrees, session_seed) {
    AD_INFO(SpatialPrivacyProcessorV2,
            "Initialized session-scoped spatial privacy transform: scope=%s radius=%.1fm gps_policy=%s",
            policy_.transform_scope.c_str(), policy_.local_radius_meters, policy_.gps_policy.c_str());
}

ComplianceDecision SpatialPrivacyProcessor::process(const TopicPolicy& topic_policy,
                                                     const rclcpp::SerializedMessage& msg) {
    if (!policy_.enabled) return ComplianceDecision::passThrough(topic_policy.topic);

    try {
        if (topic_policy.domain == PrivacyDomain::SpatialGps ||
            hasType(topic_policy.message_type, "sensor_msgs/msg/NavSatFix")) {
            return transformSerialized<sensor_msgs::msg::NavSatFix>(
                topic_policy, msg,
                [this](sensor_msgs::msg::NavSatFix& gps) {
                    if (!gps_privacy_.generalize(gps.latitude, gps.longitude, gps.altitude)) return false;
                    gps.position_covariance_type = sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_UNKNOWN;
                    gps.position_covariance = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
                    return true;
                },
                "SpatialPrivacyProcessorV2:gps");
        }
        if (hasType(topic_policy.message_type, "nav_msgs/msg/Odometry")) {
            return transformSerialized<nav_msgs::msg::Odometry>(
                topic_policy, msg,
                [this](nav_msgs::msg::Odometry& odom) {
                    TfPosePathTransformer::transform(odom, local_transformer_);
                    return true;
                },
                "SpatialPrivacyProcessorV2:odometry");
        }
        if (hasType(topic_policy.message_type, "geometry_msgs/msg/PoseStamped")) {
            return transformSerialized<geometry_msgs::msg::PoseStamped>(
                topic_policy, msg,
                [this](geometry_msgs::msg::PoseStamped& pose) {
                    TfPosePathTransformer::transform(pose, local_transformer_);
                    return true;
                },
                "SpatialPrivacyProcessorV2:pose");
        }
        if (hasType(topic_policy.message_type, "nav_msgs/msg/Path")) {
            return transformSerialized<nav_msgs::msg::Path>(
                topic_policy, msg,
                [this](nav_msgs::msg::Path& path) {
                    TfPosePathTransformer::transform(path, local_transformer_);
                    return true;
                },
                "SpatialPrivacyProcessorV2:path");
        }
        if (hasType(topic_policy.message_type, "tf2_msgs/msg/TFMessage")) {
            return transformSerialized<tf2_msgs::msg::TFMessage>(
                topic_policy, msg,
                [this](tf2_msgs::msg::TFMessage& tf) {
                    TfPosePathTransformer::transform(tf, local_transformer_);
                    return true;
                },
                "SpatialPrivacyProcessorV2:tf");
        }
        if (hasType(topic_policy.message_type, "sensor_msgs/msg/PointCloud2")) {
            return transformSerialized<sensor_msgs::msg::PointCloud2>(
                topic_policy, msg,
                [this](sensor_msgs::msg::PointCloud2& cloud) {
                    return PointCloudTransformer::transform(cloud, local_transformer_);
                },
                "SpatialPrivacyProcessorV2:pointcloud");
        }
        if (hasType(topic_policy.message_type, "visualization_msgs/msg/Marker")) {
            return transformSerialized<visualization_msgs::msg::Marker>(
                topic_policy, msg,
                [this](visualization_msgs::msg::Marker& marker) {
                    TfPosePathTransformer::transform(marker, local_transformer_);
                    return true;
                },
                "SpatialPrivacyProcessorV2:marker");
        }
    } catch (const std::exception& e) {
        AD_WARN(SpatialPrivacyProcessorV2, "Spatial processing failed for %s: %s",
                topic_policy.topic.c_str(), e.what());
    }

    return ComplianceDecision::drop(topic_policy.topic,
                                    "unsupported_spatial_message_type",
                                    "SpatialPrivacyProcessorV2");
}

template <typename MessageT, typename TransformFn>
ComplianceDecision SpatialPrivacyProcessor::transformSerialized(const TopicPolicy& topic_policy,
                                                                const rclcpp::SerializedMessage& msg,
                                                                TransformFn&& transform_fn,
                                                                const std::string& processor_name) {
    rclcpp::Serialization<MessageT> serializer;
    MessageT typed_msg;
    const auto& in_rcl = msg.get_rcl_serialized_message();
    rclcpp::SerializedMessage input_msg(in_rcl.buffer_length);
    auto& input_rcl = input_msg.get_rcl_serialized_message();
    std::memcpy(input_rcl.buffer, in_rcl.buffer, in_rcl.buffer_length);
    input_rcl.buffer_length = in_rcl.buffer_length;
    serializer.deserialize_message(&input_msg, &typed_msg);
    if (!transform_fn(typed_msg)) {
        return ComplianceDecision::drop(topic_policy.topic, "transform_failed", processor_name);
    }

    rclcpp::SerializedMessage output_msg(in_rcl.buffer_length + 128);
    serializer.serialize_message(&typed_msg, &output_msg);

    ComplianceDecision decision;
    decision.action = ComplianceAction::ForwardSanitized;
    decision.topic = topic_policy.topic;
    decision.policy_id = policy_.local_transform;
    decision.processor = processor_name;
    decision.reason = topic_policy.domain == PrivacyDomain::SpatialGps ? policy_.gps_policy : policy_.local_transform;
    decision.message = std::move(output_msg);
    return decision;
}

}  // namespace aurora::collector::compliance_v2
