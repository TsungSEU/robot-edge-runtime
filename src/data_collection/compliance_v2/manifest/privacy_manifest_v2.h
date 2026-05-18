// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <nlohmann/json.hpp>

#include "compliance_v2/policy/compliance_policy.h"

namespace aurora::collector::compliance_v2 {

class PrivacyManifestV2 {
public:
    static nlohmann::json buildTemplate(const CompliancePolicy& policy);
};

}  // namespace aurora::collector::compliance_v2
