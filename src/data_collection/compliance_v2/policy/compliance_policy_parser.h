// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include "compliance_v2/policy/compliance_policy.h"
#include "trigger/strategy/strategy_config.h"

namespace aurora::collector::compliance_v2 {

class CompliancePolicyParser {
public:
    static CompliancePolicy fromStrategy(const aurora::collector::Strategy& strategy);
};

}  // namespace aurora::collector::compliance_v2
