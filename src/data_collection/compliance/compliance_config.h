// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <string>

#include "trigger/strategy/strategy_config.h"
#include "compliance_v2/policy/compliance_policy_parser.h"

namespace aurora::collector::compliance {

struct ComplianceConfig {
    bool geo_enabled = false;
    double geo_radius = 10.0;

    bool image_enabled = false;
    int image_blur_kernel = 25;
    bool image_depth = false;

    ::aurora::collector::compliance_v2::CompliancePolicy v2_policy;

    static ComplianceConfig fromStrategy(const Strategy& s) {
        ComplianceConfig cfg;
        cfg.v2_policy = ::aurora::collector::compliance_v2::CompliancePolicyParser::fromStrategy(s);
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
