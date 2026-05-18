// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <memory>
#include <string>

#include "compliance_v2/policy/compliance_policy.h"
#include "compliance_v2/runtime/compliance_decision.h"
#include "compliance_v2/visual/pii_detector.h"
#include "compliance_v2/visual/roi_redactor.h"
#include "image_desensitizer.h"

namespace aurora::collector::compliance_v2 {

class VisualPrivacyProcessor {
public:
    explicit VisualPrivacyProcessor(VisualPolicy policy,
                                    std::unique_ptr<IPiiDetector> detector = std::make_unique<UnavailablePiiDetector>());

    ComplianceDecision process(const TopicPolicy& topic_policy,
                               const rclcpp::SerializedMessage& msg);

    const VisualPolicy& policy() const { return policy_; }

private:
    ComplianceDecision fallbackMosaic(const TopicPolicy& topic_policy,
                                      const rclcpp::SerializedMessage& msg,
                                      const std::string& reason);

    VisualPolicy policy_;
    std::unique_ptr<IPiiDetector> detector_;
    RoiRedactor redactor_;
    aurora::collector::compliance::ImageDesensitizer mosaic_fallback_;
};

}  // namespace aurora::collector::compliance_v2
