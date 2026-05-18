// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "nav_msgs/msg/path.hpp"
#include "tf2_msgs/msg/tf_message.hpp"
#include "visualization_msgs/msg/marker.hpp"
#include "compliance_v2/spatial/local_frame_transformer.h"

namespace aurora::collector::compliance_v2 {

class TfPosePathTransformer {
public:
    static void transform(nav_msgs::msg::Odometry& odom, const LocalFrameTransformer& transformer);
    static void transform(geometry_msgs::msg::PoseStamped& pose, const LocalFrameTransformer& transformer);
    static void transform(nav_msgs::msg::Path& path, const LocalFrameTransformer& transformer);
    static void transform(tf2_msgs::msg::TFMessage& tf, const LocalFrameTransformer& transformer);
    static void transform(visualization_msgs::msg::Marker& marker, const LocalFrameTransformer& transformer);
};

}  // namespace aurora::collector::compliance_v2
