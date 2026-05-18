// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "compliance_v2/runtime/compliance_decision.h"

namespace aurora::collector::compliance_v2 {

struct TopicComplianceStats {
    uint64_t messages_seen = 0;
    uint64_t messages_sanitized = 0;
    uint64_t messages_dropped = 0;
    uint64_t messages_quarantined = 0;
    uint64_t raw_forwarded_count = 0;
    std::map<std::string, uint64_t> detected_classes;
    std::map<std::string, uint64_t> errors;
    std::string redaction_method;
};

class ComplianceMetrics {
public:
    void record(const ComplianceDecision& decision) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = topics_[decision.topic];
        stats.messages_seen++;
        switch (decision.action) {
            case ComplianceAction::ForwardSanitized:
                stats.messages_sanitized++;
                stats.redaction_method = decision.reason;
                break;
            case ComplianceAction::ForwardRawExplicit:
                stats.raw_forwarded_count++;
                break;
            case ComplianceAction::Drop:
                stats.messages_dropped++;
                stats.errors[decision.reason]++;
                break;
            case ComplianceAction::Quarantine:
                stats.messages_quarantined++;
                stats.errors[decision.reason]++;
                break;
            case ComplianceAction::PassThrough:
                break;
        }
        for (const auto& cls : decision.detected_classes) {
            stats.detected_classes[cls]++;
        }
    }

    std::map<std::string, TopicComplianceStats> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return topics_;
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, TopicComplianceStats> topics_;
};

}  // namespace aurora::collector::compliance_v2
