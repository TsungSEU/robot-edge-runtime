// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "metadata_manifest.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include "compliance_v2/policy/compliance_policy.h"
#include "compliance_v2/manifest/privacy_manifest_v2.h"

#include "common/log/logger.h"
#include "common/app_config.h"
#include "common/audit/audit_logger.h"

namespace aurora::collector::compliance {

namespace {

std::string formatISO8601(uint64_t timestamp_us) {
    auto sec = static_cast<time_t>(timestamp_us / 1000000);
    auto us = static_cast<long>(timestamp_us % 1000000);
    struct tm tm_buf;
    gmtime_r(&sec, &tm_buf);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    std::ostringstream oss;
    oss << buf << "." << std::setfill('0') << std::setw(3) << (us / 1000) << "Z";
    return oss.str();
}


std::string sha256String(const std::string& value) {
    if (value.empty()) return "";
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, value.data(), value.size());
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    oss << "sha256:";
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string computeSHA256(const std::string& file_path) {
    std::ifstream f(file_path, std::ios::binary);
    if (!f.is_open()) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    char buf[8192];
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
        EVP_DigestUpdate(ctx, buf, static_cast<size_t>(f.gcount()));
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

}  // namespace

RecordingManifest MetadataManifestGenerator::buildManifest(
    const std::string& bag_path,
    const TBagInfo& bag_info,
    uint64_t trigger_timestamp_us,
    const std::string& trigger_id,
    const std::string& business_type,
    const MaskingConfig& masking_config,
    bool masking_enabled,
    double trigger_x,
    double trigger_y,
    const std::vector<Channel>& channels) {

    RecordingManifest m;

    // bag_filename: use basename only
    std::string bag_basename = bag_path.substr(bag_path.find_last_of('/') + 1);
    m.bag_filename = bag_basename;

    // Trigger info
    m.trigger_id = trigger_id;
    m.business_type = business_type;

    // Position at trigger time
    m.robot_x = trigger_x;
    m.robot_y = trigger_y;

    // Device info from AppConfig
    try {
        auto& app = AppConfig::getInstance();
        auto cfg = app.GetConfig();
        m.vin = sha256String(cfg.dataProto.vin);
        m.device_id = sha256String(cfg.dataProto.device_id);
        m.software_version = cfg.dataProto.software_version;
    } catch (...) {
        // AppConfig may not be initialized in tests
    }

    // Timing
    if (bag_info.start_timestamp > 0) {
        m.recording_start_iso8601 = formatISO8601(bag_info.start_timestamp);
    }
    if (bag_info.end_timestamp > 0) {
        m.recording_end_iso8601 = formatISO8601(bag_info.end_timestamp);
    }
    m.trigger_time_iso8601 = formatISO8601(trigger_timestamp_us);

    if (bag_info.start_timestamp > 0 && bag_info.end_timestamp > 0) {
        m.duration_seconds = static_cast<double>(
            bag_info.end_timestamp - bag_info.start_timestamp) / 1000000.0;
    }

    // Compliance status
    m.geospatial_obfuscation_applied = masking_enabled;
    m.geospatial_offset_radius = masking_config.geospatialOffsetRadius;
    m.image_desensitization_applied = masking_enabled;
    m.image_blur_kernel_size = masking_config.imageBlurKernelSize;
    m.image_redaction_method = "roi_redaction_solid_fill_with_full_frame_mosaic_fallback";
    m.geospatial_transform_scope = "session";
    m.gps_policy = "geohash_6_laplace";
    m.raw_coordinates_removed = true;

    // Sensors: prefer per-topic stats from bag_info, fall back to strategy channels
    if (!bag_info.topics.empty()) {
        for (const auto& [topic, meta] : bag_info.topics) {
            ManifestSensorInfo si;
            si.topic = meta.topic_name;
            si.type = meta.message_type;
            si.message_count = meta.message_count;
            si.data_size_bytes = meta.data_size;
            m.sensors.push_back(si);
        }
    } else {
        // Fallback: list channels from strategy config
        for (const auto& ch : channels) {
            ManifestSensorInfo si;
            si.topic = ch.topic;
            si.type = ch.type;
            m.sensors.push_back(si);
        }
    }

    m.total_messages = bag_info.total_messages;
    m.total_data_size_bytes = bag_info.total_data_size;

    aurora::collector::compliance_v2::CompliancePolicy policy;
    policy.visual.enabled = masking_enabled;
    policy.visual.fallback_mosaic_block_size = masking_config.imageBlurKernelSize;
    policy.spatial.enabled = masking_enabled;
    policy.spatial.local_radius_meters = masking_config.geospatialOffsetRadius;
    policy.policy_hash = m.privacy_policy_hash;
    for (const auto& ch : channels) {
        aurora::collector::compliance_v2::TopicPolicy tp;
        tp.topic = ch.topic;
        tp.message_type = ch.type;
        if (ch.type.find("sensor_msgs/msg/Image") != std::string::npos ||
            ch.type.find("sensor_msgs/msg/CompressedImage") != std::string::npos) {
            tp.domain = aurora::collector::compliance_v2::PrivacyDomain::Visual;
        } else if (ch.type.find("sensor_msgs/msg/NavSatFix") != std::string::npos) {
            tp.domain = aurora::collector::compliance_v2::PrivacyDomain::SpatialGps;
        } else if (ch.type.find("nav_msgs/msg/Odometry") != std::string::npos ||
                   ch.type.find("geometry_msgs/msg/PoseStamped") != std::string::npos ||
                   ch.type.find("nav_msgs/msg/Path") != std::string::npos ||
                   ch.type.find("tf2_msgs/msg/TFMessage") != std::string::npos ||
                   ch.type.find("sensor_msgs/msg/PointCloud2") != std::string::npos ||
                   ch.type.find("visualization_msgs/msg/Marker") != std::string::npos) {
            tp.domain = aurora::collector::compliance_v2::PrivacyDomain::SpatialLocalFrame;
        }
        policy.topics.emplace(ch.topic, tp);
    }
    m.privacy_v2 = aurora::collector::compliance_v2::PrivacyManifestV2::buildTemplate(policy);

    // Populate topic-level manifest counters from bag metadata where available.
    for (const auto& sensor : m.sensors) {
        auto& topic_json = sensor.type.find("sensor_msgs/msg/Image") != std::string::npos ||
                           sensor.type.find("sensor_msgs/msg/CompressedImage") != std::string::npos
            ? m.privacy_v2["visual"]["topics"][sensor.topic]
            : m.privacy_v2["spatial"]["topics"][sensor.topic];
        topic_json["messages_seen"] = sensor.message_count;
        topic_json["messages_sanitized"] = masking_enabled ? sensor.message_count : 0;
        topic_json["messages_dropped"] = 0;
        topic_json["raw_forwarded_count"] = 0;
        if (sensor.type.find("sensor_msgs/msg/Image") != std::string::npos ||
            sensor.type.find("sensor_msgs/msg/CompressedImage") != std::string::npos) {
            topic_json["detected_classes"] = nlohmann::json::object();
            topic_json["redaction_method"] = m.image_redaction_method;
        }
    }

    return m;
}

bool MetadataManifestGenerator::generate(const std::string& bag_path,
                                          const RecordingManifest& manifest) {
    nlohmann::json j;
    j["manifest_version"] = manifest.manifest_version;
    j["bag_filename"] = manifest.bag_filename;

    j["trigger_id"] = manifest.trigger_id;
    j["business_type"] = manifest.business_type;

    j["device"] = {
        {"vin_token", manifest.vin},
        {"device_id_token", manifest.device_id},
        {"identity_tokenization", "sha256"},
        {"software_version", manifest.software_version}
    };

    j["timing"] = {
        {"recording_start", manifest.recording_start_iso8601},
        {"recording_end", manifest.recording_end_iso8601},
        {"trigger_time", manifest.trigger_time_iso8601},
        {"duration_seconds", manifest.duration_seconds}
    };

    j["position"] = {
        {"robot_x", manifest.robot_x},
        {"robot_y", manifest.robot_y},
        {"robot_yaw", manifest.robot_yaw},
        {"coordinate_system", manifest.coordinate_system}
    };

    j["compliance"] = {
        {"privacy_policy_version", manifest.privacy_policy_version},
        {"policy_hash", manifest.privacy_policy_hash},
        {"fail_mode", manifest.privacy_fail_mode},
        {"fail_closed", manifest.compliance_fail_closed},
        {"geospatial_obfuscation", {
            {"applied", manifest.geospatial_obfuscation_applied},
            {"policy_radius_meters", manifest.geospatial_offset_radius},
            {"transform_scope", manifest.geospatial_transform_scope},
            {"gps_policy", manifest.gps_policy},
            {"raw_coordinates_removed", manifest.raw_coordinates_removed},
            {"transform_values_logged", false}
        }},
        {"image_desensitization", {
            {"applied", manifest.image_desensitization_applied},
            {"redaction_method", manifest.image_redaction_method},
            {"block_size", manifest.image_blur_kernel_size}
        }}
    };

    j["privacy"] = manifest.privacy_v2;

    nlohmann::json sensors = nlohmann::json::array();
    for (const auto& s : manifest.sensors) {
        sensors.push_back({
            {"topic", s.topic},
            {"type", s.type},
            {"message_count", s.message_count},
            {"data_size_bytes", s.data_size_bytes}
        });
    }
    j["sensors"] = sensors;

    j["statistics"] = {
        {"total_messages", manifest.total_messages},
        {"total_data_size_bytes", manifest.total_data_size_bytes}
    };

    // Compute SHA-256 of the bag file for integrity
    std::string bag_basename = bag_path.substr(bag_path.find_last_of('/') + 1);
    std::string db3_path = bag_path + "/" + bag_basename + "_0.db3";
    j["integrity"] = {{"bag_sha256", computeSHA256(db3_path)}};

    // Write manifest inside bag directory so it gets included in tar.lz4
    std::string manifest_path = bag_path + "/" + bag_basename + "_manifest.json";
    std::ofstream out(manifest_path);
    if (!out.is_open()) {
        AD_ERROR(MetadataManifest, "Failed to write manifest: %s", manifest_path.c_str());
        return false;
    }

    out << j.dump(2);
    out.close();

    AD_INFO(MetadataManifest, "Manifest generated: %s", manifest_path.c_str());
    aurora::common::audit::AuditLogger::instance().log(
        aurora::common::audit::AuditEventType::COMPLIANCE_MANIFEST_GENERATED,
        "MetadataManifest",
        "{\"manifest_path\":\"" + manifest_path + "\"}");
    return true;
}

}  // namespace aurora::collector::compliance
