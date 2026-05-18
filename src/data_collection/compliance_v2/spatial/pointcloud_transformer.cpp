// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "pointcloud_transformer.h"

#include <algorithm>
#include <cstring>

namespace aurora::collector::compliance_v2 {
namespace {

int fieldOffset(const sensor_msgs::msg::PointCloud2& cloud, const char* name) {
    for (const auto& field : cloud.fields) {
        if (field.name == name && field.datatype == sensor_msgs::msg::PointField::FLOAT32) {
            return static_cast<int>(field.offset);
        }
    }
    return -1;
}

float readFloat(const uint8_t* ptr) {
    float value = 0.0F;
    std::memcpy(&value, ptr, sizeof(float));
    return value;
}

void writeFloat(uint8_t* ptr, float value) {
    std::memcpy(ptr, &value, sizeof(float));
}

}  // namespace

bool PointCloudTransformer::transform(sensor_msgs::msg::PointCloud2& cloud,
                                      const LocalFrameTransformer& transformer) {
    if (cloud.is_bigendian || cloud.point_step == 0) return false;
    const int x_offset = fieldOffset(cloud, "x");
    const int y_offset = fieldOffset(cloud, "y");
    if (x_offset < 0 || y_offset < 0) return false;

    const size_t points = static_cast<size_t>(cloud.width) * cloud.height;
    for (size_t i = 0; i < points; ++i) {
        const size_t base = i * cloud.point_step;
        if (base + static_cast<size_t>(std::max(x_offset, y_offset)) + sizeof(float) > cloud.data.size()) {
            return false;
        }
        double x = readFloat(&cloud.data[base + x_offset]);
        double y = readFloat(&cloud.data[base + y_offset]);
        transformer.transformPoint(x, y);
        writeFloat(&cloud.data[base + x_offset], static_cast<float>(x));
        writeFloat(&cloud.data[base + y_offset], static_cast<float>(y));
    }
    return true;
}

}  // namespace aurora::collector::compliance_v2
