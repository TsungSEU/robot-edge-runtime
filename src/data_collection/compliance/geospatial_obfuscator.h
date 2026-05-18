// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace aurora::collector::compliance {

class GeospatialObfuscator {
public:
    GeospatialObfuscator(double radius_meters, uint64_t session_seed);

    bool obfuscate(std::vector<uint8_t>& cdr_buffer);

    double radius() const { return radius_meters_; }
    const char* transformScope() const { return "session_se2"; }

private:
    double radius_meters_;
    double offset_x_;
    double offset_y_;
    double rotation_rad_;
};

}  // namespace aurora::collector::compliance
