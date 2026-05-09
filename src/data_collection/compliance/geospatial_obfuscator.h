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

    double offsetX() const { return offset_x_; }
    double offsetY() const { return offset_y_; }
    double radius() const { return radius_meters_; }

private:
    double radius_meters_;
    double offset_x_;
    double offset_y_;
};

}  // namespace aurora::collector::compliance
