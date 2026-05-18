// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <string>

namespace aurora::collector::compliance_v2 {

class PrivacyAuditWriter {
public:
    static void writeEvent(const std::string& component, const std::string& payload);
};

}  // namespace aurora::collector::compliance_v2
