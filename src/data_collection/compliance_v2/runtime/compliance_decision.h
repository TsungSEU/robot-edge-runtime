// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"

namespace aurora::collector::compliance_v2 {

enum class ComplianceAction {
    ForwardSanitized,
    ForwardRawExplicit,
    Drop,
    Quarantine,
    PassThrough
};

struct ComplianceDecision {
    ComplianceAction action = ComplianceAction::PassThrough;
    std::string topic;
    std::string policy_id;
    std::string processor;
    std::string reason;
    std::vector<std::string> detected_classes;
    rclcpp::SerializedMessage message;

    static ComplianceDecision passThrough(const std::string& topic_name) {
        ComplianceDecision d;
        d.action = ComplianceAction::PassThrough;
        d.topic = topic_name;
        return d;
    }

    static ComplianceDecision drop(const std::string& topic_name,
                                   const std::string& why,
                                   const std::string& processor_name = {}) {
        ComplianceDecision d;
        d.action = ComplianceAction::Drop;
        d.topic = topic_name;
        d.reason = why;
        d.processor = processor_name;
        return d;
    }
};

inline const char* toString(ComplianceAction action) {
    switch (action) {
        case ComplianceAction::ForwardSanitized: return "forwarded_sanitized";
        case ComplianceAction::ForwardRawExplicit: return "forwarded_raw_explicit";
        case ComplianceAction::Drop: return "dropped";
        case ComplianceAction::Quarantine: return "quarantined";
        case ComplianceAction::PassThrough: return "pass_through";
    }
    return "unknown";
}

}  // namespace aurora::collector::compliance_v2
