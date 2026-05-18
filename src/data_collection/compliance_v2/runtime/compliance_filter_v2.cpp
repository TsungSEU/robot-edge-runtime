// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "compliance_filter_v2.h"

#include <utility>

#include <chrono>

#include "common/log/logger.h"

namespace aurora::collector::compliance_v2 {

ComplianceFilterV2::ComplianceFilterV2(std::shared_ptr<rclcpp::Node> node, CompliancePolicy policy)
    : node_(std::move(node)), registry_(std::move(policy)) {
    const auto seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    if (registry_.policy().visual.enabled) {
        visual_ = std::make_unique<VisualPrivacyProcessor>(registry_.policy().visual);
    }
    if (registry_.policy().spatial.enabled) {
        spatial_ = std::make_unique<SpatialPrivacyProcessor>(registry_.policy().spatial, seed);
    }
    AD_INFO(ComplianceFilterV2,
            "Initialized policy=%s visual=%d spatial=%d topics=%zu fail_mode=%s",
            registry_.policy().policy_version.c_str(), registry_.policy().visual.enabled,
            registry_.policy().spatial.enabled, registry_.policy().topics.size(),
            toString(registry_.policy().default_fail_mode).c_str());
}

void ComplianceFilterV2::setDownstream(const std::shared_ptr<aurora::collector::Observer>& downstream) {
    downstream_ = downstream;
}

void ComplianceFilterV2::OnMessageReceived(const std::string& topic,
                                           const rclcpp::SerializedMessage& msg) {
    if (!downstream_) return;

    auto topic_policy = registry_.resolve(topic);
    if (!topic_policy.has_value()) {
        downstream_->OnMessageReceived(topic, msg);
        return;
    }

    ComplianceDecision decision = ComplianceDecision::passThrough(topic);
    try {
        switch (topic_policy->domain) {
            case PrivacyDomain::Visual:
                decision = visual_ ? visual_->process(*topic_policy, msg)
                                   : ComplianceDecision::drop(topic, "visual_processor_unavailable", "ComplianceFilterV2");
                break;
            case PrivacyDomain::SpatialLocalFrame:
            case PrivacyDomain::SpatialGps:
                decision = spatial_ ? spatial_->process(*topic_policy, msg)
                                    : ComplianceDecision::drop(topic, "spatial_processor_unavailable", "ComplianceFilterV2");
                break;
            case PrivacyDomain::PassThrough:
                decision = ComplianceDecision::passThrough(topic);
                break;
            case PrivacyDomain::Identity:
                decision = ComplianceDecision::drop(topic, "identity_policy_not_implemented", "ComplianceFilterV2");
                break;
        }
    } catch (const std::exception& e) {
        decision = ComplianceDecision::drop(topic, e.what(), "ComplianceFilterV2");
    }

    decision = applyFailMode(*topic_policy, std::move(decision), msg);
    metrics_.record(decision);
    forwardDecision(decision, msg);
}

ComplianceDecision ComplianceFilterV2::applyFailMode(const TopicPolicy& topic_policy,
                                                     ComplianceDecision decision,
                                                     const rclcpp::SerializedMessage& original_msg) const {
    if (decision.action != ComplianceAction::Drop && decision.action != ComplianceAction::Quarantine) {
        return decision;
    }

    if (topic_policy.fail_mode == FailMode::RawForwardExplicit && topic_policy.raw_forward_allowed) {
        ComplianceDecision raw;
        raw.action = ComplianceAction::ForwardRawExplicit;
        raw.topic = topic_policy.topic;
        raw.policy_id = registry_.policy().policy_id;
        raw.processor = decision.processor;
        raw.reason = "raw_forward_allowed_by_explicit_policy_after:" + decision.reason;
        return raw;
    }

    if (topic_policy.fail_mode == FailMode::Quarantine) {
        decision.action = ComplianceAction::Quarantine;
        return decision;
    }

    return decision;
}

void ComplianceFilterV2::forwardDecision(const ComplianceDecision& decision,
                                         const rclcpp::SerializedMessage& original_msg) {
    switch (decision.action) {
        case ComplianceAction::ForwardSanitized:
            downstream_->OnMessageReceived(decision.topic, decision.message);
            break;
        case ComplianceAction::ForwardRawExplicit:
            AD_WARN(ComplianceFilterV2, "Forwarding raw by explicit policy: topic=%s reason=%s",
                    decision.topic.c_str(), decision.reason.c_str());
            downstream_->OnMessageReceived(decision.topic, original_msg);
            break;
        case ComplianceAction::PassThrough:
            downstream_->OnMessageReceived(decision.topic, original_msg);
            break;
        case ComplianceAction::Drop:
            AD_ERROR(ComplianceFilterV2, "Dropping message fail-closed: topic=%s reason=%s",
                     decision.topic.c_str(), decision.reason.c_str());
            break;
        case ComplianceAction::Quarantine:
            AD_ERROR(ComplianceFilterV2, "Quarantining message: topic=%s reason=%s",
                     decision.topic.c_str(), decision.reason.c_str());
            break;
    }
}

}  // namespace aurora::collector::compliance_v2
