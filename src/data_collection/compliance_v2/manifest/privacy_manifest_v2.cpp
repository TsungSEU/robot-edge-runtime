// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "privacy_manifest_v2.h"

namespace aurora::collector::compliance_v2 {

nlohmann::json PrivacyManifestV2::buildTemplate(const CompliancePolicy& policy) {
    nlohmann::json transformed_topics = nlohmann::json::array();
    for (const auto& [topic, topic_policy] : policy.topics) {
        if (topic_policy.domain == PrivacyDomain::SpatialLocalFrame ||
            topic_policy.domain == PrivacyDomain::SpatialGps) {
            transformed_topics.push_back(topic);
        }
    }

    return {
        {"policy_version", policy.policy_version},
        {"policy_hash", policy.policy_hash},
        {"fail_mode", toString(policy.default_fail_mode)},
        {"visual", {
            {"enabled", policy.visual.enabled},
            {"mode", policy.visual.mode},
            {"detectors", policy.visual.detectors},
            {"redaction_method", policy.visual.redaction_method},
            {"fallback", {
                {"onDetectorUnavailable", policy.visual.on_detector_unavailable},
                {"onProcessingError", policy.visual.on_processing_error},
                {"onOverload", policy.visual.on_overload}
            }}
        }},
        {"spatial", {
            {"enabled", policy.spatial.enabled},
            {"transform_scope", policy.spatial.transform_scope},
            {"transformed_topics", transformed_topics},
            {"gps_policy", policy.spatial.gps_policy},
            {"raw_coordinates_removed", policy.spatial.remove_raw_coordinates},
            {"transform_values_logged", policy.spatial.transform_values_logged}
        }},
        {"errors", nlohmann::json::array()}
    };
}

}  // namespace aurora::collector::compliance_v2
