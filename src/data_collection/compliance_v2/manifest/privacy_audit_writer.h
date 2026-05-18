// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <string>

namespace aurora::collector::compliance_v2 {

class PrivacyAuditWriter {
public:
    static void writeEvent(const std::string& component, const std::string& payload);
};

}  // namespace aurora::collector::compliance_v2
