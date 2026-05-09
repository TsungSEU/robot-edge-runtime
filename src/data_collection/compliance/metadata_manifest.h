// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>

#include "recorder/ros2bag_recorder.h"
#include "trigger/strategy/strategy_config.h"
#include "compliance_config.h"

namespace aurora::collector::compliance {

struct ManifestSensorInfo {
    std::string topic;
    std::string type;
    uint64_t message_count = 0;
    size_t data_size_bytes = 0;
};

struct RecordingManifest {
    std::string manifest_version = "1.0";
    std::string bag_filename;

    // Trigger info
    std::string trigger_id;
    std::string business_type;

    // Device info
    std::string vin;
    std::string device_id;
    std::string software_version;

    // Timing
    std::string recording_start_iso8601;
    std::string recording_end_iso8601;
    std::string trigger_time_iso8601;
    double duration_seconds = 0.0;

    // Position at trigger time
    double robot_x = 0.0;
    double robot_y = 0.0;
    double robot_yaw = 0.0;
    std::string coordinate_system = "odom";

    // Compliance
    bool geospatial_obfuscation_applied = false;
    double geospatial_offset_radius = 0.0;
    bool image_desensitization_applied = false;
    int image_blur_kernel_size = 0;

    // Sensors
    std::vector<ManifestSensorInfo> sensors;
    uint64_t total_messages = 0;
    size_t total_data_size_bytes = 0;

    // Integrity
    std::string bag_sha256;
};

class MetadataManifestGenerator {
public:
    static bool generate(const std::string& bag_path, const RecordingManifest& manifest);

    static RecordingManifest buildManifest(
        const std::string& bag_path,
        const TBagInfo& bag_info,
        uint64_t trigger_timestamp_us,
        const std::string& trigger_id,
        const std::string& business_type,
        const MaskingConfig& masking_config,
        bool masking_enabled,
        double trigger_x = 0.0,
        double trigger_y = 0.0,
        const std::vector<Channel>& channels = {});
};

}  // namespace aurora::collector::compliance
