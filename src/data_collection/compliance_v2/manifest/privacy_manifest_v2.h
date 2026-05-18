// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <nlohmann/json.hpp>

#include "compliance_v2/policy/compliance_policy.h"

namespace aurora::collector::compliance_v2 {

class PrivacyManifestV2 {
public:
    static nlohmann::json buildTemplate(const CompliancePolicy& policy);
};

}  // namespace aurora::collector::compliance_v2
