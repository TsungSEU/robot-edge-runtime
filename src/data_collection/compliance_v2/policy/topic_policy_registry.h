// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <utility>

#include <optional>
#include <string>

#include "compliance_v2/policy/compliance_policy.h"

namespace aurora::collector::compliance_v2 {

class TopicPolicyRegistry {
public:
    TopicPolicyRegistry() = default;
    explicit TopicPolicyRegistry(CompliancePolicy policy) : policy_(std::move(policy)) {}

    const CompliancePolicy& policy() const { return policy_; }

    std::optional<TopicPolicy> resolve(const std::string& topic,
                                       const std::string& message_type = {}) const {
        auto it = policy_.topics.find(topic);
        if (it == policy_.topics.end()) return std::nullopt;
        if (!message_type.empty() && !it->second.message_type.empty() &&
            it->second.message_type != message_type) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    CompliancePolicy policy_;
};

}  // namespace aurora::collector::compliance_v2
