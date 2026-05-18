// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <string>

#include "compliance_v2/policy/compliance_policy.h"
#include "compliance_v2/runtime/compliance_decision.h"
#include "compliance_v2/spatial/geo_indistinguishability.h"
#include "compliance_v2/spatial/local_frame_transformer.h"

namespace aurora::collector::compliance_v2 {

class SpatialPrivacyProcessor {
public:
    SpatialPrivacyProcessor(SpatialPolicy policy, uint64_t session_seed);

    ComplianceDecision process(const TopicPolicy& topic_policy,
                               const rclcpp::SerializedMessage& msg);

    const SpatialPolicy& policy() const { return policy_; }

private:
    template <typename MessageT, typename TransformFn>
    ComplianceDecision transformSerialized(const TopicPolicy& topic_policy,
                                           const rclcpp::SerializedMessage& msg,
                                           TransformFn&& transform_fn,
                                           const std::string& processor_name);

    SpatialPolicy policy_;
    LocalFrameTransformer local_transformer_;
    GeoIndistinguishability gps_privacy_;
};

}  // namespace aurora::collector::compliance_v2
