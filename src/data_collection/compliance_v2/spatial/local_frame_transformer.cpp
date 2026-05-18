// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "local_frame_transformer.h"

#include <cmath>

namespace aurora::collector::compliance_v2 {

LocalFrameTransformer::LocalFrameTransformer(double radius_meters, uint64_t session_seed)
    : radius_meters_(radius_meters) {
    std::mt19937 gen(static_cast<std::mt19937::result_type>(session_seed));
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radius_dist(0.5 * radius_meters, radius_meters);
    std::uniform_real_distribution<double> rotation_dist(-M_PI, M_PI);

    const double theta = angle_dist(gen);
    const double r = radius_dist(gen);
    transform_.offset_x = r * std::cos(theta);
    transform_.offset_y = r * std::sin(theta);
    transform_.rotation_rad = rotation_dist(gen);
}

void LocalFrameTransformer::transformPoint(double& x, double& y) const {
    const double cos_r = std::cos(transform_.rotation_rad);
    const double sin_r = std::sin(transform_.rotation_rad);
    const double raw_x = x;
    const double raw_y = y;
    x = cos_r * raw_x - sin_r * raw_y + transform_.offset_x;
    y = sin_r * raw_x + cos_r * raw_y + transform_.offset_y;
}

void LocalFrameTransformer::rotateYawQuaternion(double& x, double& y, double& z, double& w) const {
    const double half_rotation = transform_.rotation_rad * 0.5;
    const double rz = std::sin(half_rotation);
    const double rw = std::cos(half_rotation);

    const double qx = rw * x - rz * y;
    const double qy = rw * y + rz * x;
    const double qz = rw * z + rz * w;
    const double qw = rw * w - rz * z;
    const double norm = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (norm > 0.0) {
        x = qx / norm;
        y = qy / norm;
        z = qz / norm;
        w = qw / norm;
    }
}

}  // namespace aurora::collector::compliance_v2
