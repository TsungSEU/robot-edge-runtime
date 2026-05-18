// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace aurora::collector::compliance_v2 {

enum class PrivacyDomain {
    PassThrough,
    Visual,
    SpatialLocalFrame,
    SpatialGps,
    Identity
};

enum class FailMode {
    FailClosed,
    Quarantine,
    RawForwardExplicit
};

struct VisualPolicy {
    bool enabled = false;
    std::string mode = "roi_redaction";
    std::vector<std::string> detectors{"face", "license_plate", "text", "qr_code", "screen"};
    std::string redaction_method = "solid_fill";
    std::vector<uint8_t> fill_color{0, 0, 0};
    double expand_ratio = 0.25;
    std::string on_detector_unavailable = "full_frame_mosaic";
    std::string on_processing_error = "drop_frame";
    std::string on_overload = "sample_and_redact";
    int fallback_mosaic_block_size = 16;
};

struct SpatialPolicy {
    bool enabled = false;
    std::string transform_scope = "session";
    std::string local_transform = "session_se2";
    double local_radius_meters = 10.0;
    std::string gps_policy = "geohash_6_laplace";
    int gps_decimal_places = 4;
    double gps_laplace_scale_degrees = 0.00001;
    bool remove_raw_coordinates = true;
    bool transform_values_logged = false;
};

struct TopicPolicy {
    std::string topic;
    std::string message_type;
    PrivacyDomain domain = PrivacyDomain::PassThrough;
    bool required = false;
    bool raw_forward_allowed = false;
    FailMode fail_mode = FailMode::FailClosed;
};

struct CompliancePolicy {
    std::string policy_id = "visual-spatial-v2.1";
    std::string policy_version = "visual-spatial-v2.1";
    std::string policy_hash = "sha256:unconfigured";
    FailMode default_fail_mode = FailMode::FailClosed;
    VisualPolicy visual;
    SpatialPolicy spatial;
    std::unordered_map<std::string, TopicPolicy> topics;
};

inline std::string toString(PrivacyDomain domain) {
    switch (domain) {
        case PrivacyDomain::PassThrough: return "pass_through";
        case PrivacyDomain::Visual: return "visual";
        case PrivacyDomain::SpatialLocalFrame: return "spatial_local_frame";
        case PrivacyDomain::SpatialGps: return "spatial_gps";
        case PrivacyDomain::Identity: return "identity";
    }
    return "unknown";
}

inline std::string toString(FailMode fail_mode) {
    switch (fail_mode) {
        case FailMode::FailClosed: return "fail_closed";
        case FailMode::Quarantine: return "quarantine";
        case FailMode::RawForwardExplicit: return "raw_forward_explicit";
    }
    return "unknown";
}

}  // namespace aurora::collector::compliance_v2
