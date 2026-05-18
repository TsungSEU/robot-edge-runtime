// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include "sensor_msgs/msg/point_cloud2.hpp"
#include "compliance_v2/spatial/local_frame_transformer.h"

namespace aurora::collector::compliance_v2 {

class PointCloudTransformer {
public:
    static bool transform(sensor_msgs::msg::PointCloud2& cloud,
                          const LocalFrameTransformer& transformer);
};

}  // namespace aurora::collector::compliance_v2
