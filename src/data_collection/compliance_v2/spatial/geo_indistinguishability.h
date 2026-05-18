// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <cstdint>
#include <string>

namespace aurora::collector::compliance_v2 {

class GeoIndistinguishability {
public:
    GeoIndistinguishability(int decimal_places, double laplace_scale_degrees, uint64_t session_seed);

    bool generalize(double& latitude, double& longitude, double& altitude) const;
    std::string policyId() const { return "geohash_6_laplace"; }

private:
    double quantize(double value) const;
    double deterministicNoise(double value, uint64_t salt) const;

    int decimal_places_ = 4;
    double laplace_scale_degrees_ = 0.00001;
    uint64_t session_seed_ = 0;
};

}  // namespace aurora::collector::compliance_v2
