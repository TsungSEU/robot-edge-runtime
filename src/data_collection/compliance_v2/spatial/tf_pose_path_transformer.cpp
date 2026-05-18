// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#include "tf_pose_path_transformer.h"

namespace aurora::collector::compliance_v2 {
namespace {

void transformPose(geometry_msgs::msg::Pose& pose, const LocalFrameTransformer& transformer) {
    transformer.transformPoint(pose.position.x, pose.position.y);
    transformer.rotateYawQuaternion(pose.orientation.x,
                                    pose.orientation.y,
                                    pose.orientation.z,
                                    pose.orientation.w);
}

}  // namespace

void TfPosePathTransformer::transform(nav_msgs::msg::Odometry& odom,
                                      const LocalFrameTransformer& transformer) {
    transformPose(odom.pose.pose, transformer);
}

void TfPosePathTransformer::transform(geometry_msgs::msg::PoseStamped& pose,
                                      const LocalFrameTransformer& transformer) {
    transformPose(pose.pose, transformer);
}

void TfPosePathTransformer::transform(nav_msgs::msg::Path& path,
                                      const LocalFrameTransformer& transformer) {
    for (auto& pose : path.poses) {
        transform(pose, transformer);
    }
}

void TfPosePathTransformer::transform(tf2_msgs::msg::TFMessage& tf,
                                      const LocalFrameTransformer& transformer) {
    for (auto& transform_stamped : tf.transforms) {
        transformer.transformPoint(transform_stamped.transform.translation.x,
                                   transform_stamped.transform.translation.y);
        transformer.rotateYawQuaternion(transform_stamped.transform.rotation.x,
                                        transform_stamped.transform.rotation.y,
                                        transform_stamped.transform.rotation.z,
                                        transform_stamped.transform.rotation.w);
    }
}

void TfPosePathTransformer::transform(visualization_msgs::msg::Marker& marker,
                                      const LocalFrameTransformer& transformer) {
    transformPose(marker.pose, transformer);
    for (auto& point : marker.points) {
        transformer.transformPoint(point.x, point.y);
    }
}

}  // namespace aurora::collector::compliance_v2
