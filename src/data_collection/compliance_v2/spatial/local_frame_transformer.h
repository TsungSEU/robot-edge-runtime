// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <cstdint>
#include <random>

namespace aurora::collector::compliance_v2 {

struct LocalFrameTransform {
    double offset_x = 0.0;
    double offset_y = 0.0;
    double rotation_rad = 0.0;
};

class LocalFrameTransformer {
public:
    LocalFrameTransformer(double radius_meters, uint64_t session_seed);

    void transformPoint(double& x, double& y) const;
    void rotateYawQuaternion(double& x, double& y, double& z, double& w) const;

    const char* scope() const { return "session_se2"; }
    double radius() const { return radius_meters_; }

private:
    double radius_meters_ = 0.0;
    LocalFrameTransform transform_;
};

}  // namespace aurora::collector::compliance_v2
