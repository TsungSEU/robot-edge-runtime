// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <memory>

#include "channel/observer.h"
#include "compliance_v2/policy/topic_policy_registry.h"
#include "compliance_v2/runtime/compliance_metrics.h"
#include "compliance_v2/spatial/spatial_privacy_processor.h"
#include "compliance_v2/visual/visual_privacy_processor.h"
#include "rclcpp/rclcpp.hpp"

namespace aurora::collector::compliance_v2 {

class ComplianceFilterV2 : public aurora::collector::Observer {
public:
    ComplianceFilterV2(std::shared_ptr<rclcpp::Node> node, CompliancePolicy policy);

    void setDownstream(const std::shared_ptr<aurora::collector::Observer>& downstream);

    void OnMessageReceived(const std::string& topic,
                           const rclcpp::SerializedMessage& msg) override;

    const CompliancePolicy& policy() const { return registry_.policy(); }
    std::map<std::string, TopicComplianceStats> metricsSnapshot() const { return metrics_.snapshot(); }

private:
    void forwardDecision(const ComplianceDecision& decision,
                         const rclcpp::SerializedMessage& original_msg);
    ComplianceDecision applyFailMode(const TopicPolicy& topic_policy,
                                     ComplianceDecision decision,
                                     const rclcpp::SerializedMessage& original_msg) const;

    std::shared_ptr<rclcpp::Node> node_;
    TopicPolicyRegistry registry_;
    std::shared_ptr<aurora::collector::Observer> downstream_;
    std::unique_ptr<VisualPrivacyProcessor> visual_;
    std::unique_ptr<SpatialPrivacyProcessor> spatial_;
    mutable ComplianceMetrics metrics_;
};

}  // namespace aurora::collector::compliance_v2
