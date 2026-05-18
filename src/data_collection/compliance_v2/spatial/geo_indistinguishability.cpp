// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "geo_indistinguishability.h"

#include <cmath>
#include <functional>
#include <sstream>

namespace aurora::collector::compliance_v2 {

GeoIndistinguishability::GeoIndistinguishability(int decimal_places,
                                                 double laplace_scale_degrees,
                                                 uint64_t session_seed)
    : decimal_places_(decimal_places),
      laplace_scale_degrees_(laplace_scale_degrees),
      session_seed_(session_seed) {}

bool GeoIndistinguishability::generalize(double& latitude, double& longitude, double& altitude) const {
    if (!std::isfinite(latitude) || !std::isfinite(longitude)) return false;
    latitude = quantize(latitude + deterministicNoise(latitude, 0x9e3779b97f4a7c15ULL));
    longitude = quantize(longitude + deterministicNoise(longitude, 0xbf58476d1ce4e5b9ULL));
    altitude = 0.0;
    return true;
}

double GeoIndistinguishability::quantize(double value) const {
    const double factor = std::pow(10.0, decimal_places_);
    return std::round(value * factor) / factor;
}

double GeoIndistinguishability::deterministicNoise(double value, uint64_t salt) const {
    std::ostringstream oss;
    oss << session_seed_ << ':' << salt << ':' << quantize(value);
    const auto h = std::hash<std::string>{}(oss.str());
    const double u = (static_cast<double>(h % 1000000U) / 1000000.0) - 0.5;
    const double sign = u < 0.0 ? -1.0 : 1.0;
    return -laplace_scale_degrees_ * sign * std::log1p(-2.0 * std::abs(u));
}

}  // namespace aurora::collector::compliance_v2
