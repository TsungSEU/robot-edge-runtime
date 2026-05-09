// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <string>

#include "trigger/strategy/strategy_config.h"

namespace aurora::collector::compliance {

struct ComplianceConfig {
    bool geo_enabled = false;
    double geo_radius = 10.0;

    bool image_enabled = false;
    int image_blur_kernel = 25;
    bool image_depth = false;

    static ComplianceConfig fromStrategy(const Strategy& s) {
        ComplianceConfig cfg;
        if (!s.enableMasking) return cfg;
        cfg.geo_enabled = true;
        cfg.geo_radius = s.maskingConfig.geospatialOffsetRadius;
        cfg.image_enabled = true;
        cfg.image_blur_kernel = s.maskingConfig.imageBlurKernelSize;
        cfg.image_depth = s.maskingConfig.imageDesensitizeDepth;
        return cfg;
    }
};

}  // namespace aurora::collector::compliance
