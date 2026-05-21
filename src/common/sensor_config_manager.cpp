//
// Created by xucong on 26-4-9.
// Copyright (c) 2026 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#include "sensor_config_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

// 简单的 JSON 解析（避免依赖额外的 JSON 库）
namespace {
    std::string extractJsonStringValue(const std::string& json, const std::string& key) {
        std::string search_key = "\"" + key + "\"";
        size_t pos = json.find(search_key);
        if (pos == std::string::npos) return "";

        pos = json.find(":", pos);
        if (pos == std::string::npos) return "";

        pos = json.find("\"", pos);
        if (pos == std::string::npos) return "";
        pos++; // 跳过引号

        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";

        return json.substr(pos, end - pos);
    }

    bool extractJsonBoolValue(const std::string& json, const std::string& key) {
        std::string search_key = "\"" + key + "\"";
        size_t pos = json.find(search_key);
        if (pos == std::string::npos) return false;

        pos = json.find(":", pos);
        if (pos == std::string::npos) return false;

        // 查找 true 或 false
        size_t true_pos = json.find("true", pos);
        size_t false_pos = json.find("false", pos);

        if (true_pos != std::string::npos && (false_pos == std::string::npos || true_pos < false_pos)) {
            return true;
        }
        return false;
    }

    int extractJsonIntValue(const std::string& json, const std::string& key) {
        std::string search_key = "\"" + key + "\"";
        size_t pos = json.find(search_key);
        if (pos == std::string::npos) return 0;

        pos = json.find(":", pos);
        if (pos == std::string::npos) return 0;

        // 跳过空格和冒号
        while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
            pos++;
        }

        // 读取数字
        std::string num_str;
        while (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '-')) {
            num_str += json[pos++];
        }

        return num_str.empty() ? 0 : std::stoi(num_str);
    }
}

namespace aurora::common {

SensorConfigManager& SensorConfigManager::instance() {
    static SensorConfigManager instance;
    return instance;
}

bool SensorConfigManager::loadFromConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "[SensorConfigManager] Failed to open config file: " << config_path << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_content = buffer.str();

    return parseJsonConfig(json_content);
}

bool SensorConfigManager::parseJsonConfig(const std::string& json_content) {
    // 解析相机配置
    sensor_config_.camera_enabled = extractJsonBoolValue(json_content, "camera_enabled");
    sensor_config_.camera_count = extractJsonIntValue(json_content, "camera_count");

    // 解析激光雷达配置
    sensor_config_.lidar_enabled = extractJsonBoolValue(json_content, "lidar_enabled");
    sensor_config_.lidar_count = extractJsonIntValue(json_content, "lidar_count");

    // 解析IMU配置
    sensor_config_.imu_enabled = extractJsonBoolValue(json_content, "imu_enabled");
    sensor_config_.imu_count = extractJsonIntValue(json_content, "imu_count");

    initialized_ = true;
    return true;
}

bool SensorConfigManager::autoDetect() {
    // 在实际环境中，这里可以通过 ROS2 话题检测传感器
    // 例如：检查 /camera、/lidar、/imu 等话题是否存在

    // 这里使用默认配置作为示例
    sensor_config_ = getDefaultConfig();
    initialized_ = true;

    std::cout << "[SensorConfigManager] Using default sensor configuration (auto-detect not implemented)" << std::endl;
    return true;
}

void SensorConfigManager::setSensorConfig(const SensorConfig& config) {
    sensor_config_ = config;
    initialized_ = true;
}

void SensorConfigManager::setSensor(const std::string& sensor_type, bool enabled, int count) {
    if (sensor_type == "camera") {
        sensor_config_.camera_enabled = enabled;
        sensor_config_.camera_count = enabled ? count : 0;
    } else if (sensor_type == "lidar") {
        sensor_config_.lidar_enabled = enabled;
        sensor_config_.lidar_count = enabled ? count : 0;
    } else if (sensor_type == "imu") {
        sensor_config_.imu_enabled = enabled;
        sensor_config_.imu_count = enabled ? count : 0;
    }
    initialized_ = true;
}

void SensorConfigManager::printSummary() const {
    std::cout << "[Sensor Configuration]" << std::endl;
    std::cout << "  Camera: " << (sensor_config_.camera_enabled ? "ENABLED" : "DISABLED");
    if (sensor_config_.camera_enabled) {
        std::cout << " (" << sensor_config_.camera_count << " units)";
    }
    std::cout << std::endl;

    std::cout << "  LiDAR:  " << (sensor_config_.lidar_enabled ? "ENABLED" : "DISABLED");
    if (sensor_config_.lidar_enabled) {
        std::cout << " (" << sensor_config_.lidar_count << " units)";
    }
    std::cout << std::endl;

    std::cout << "  IMU:    " << (sensor_config_.imu_enabled ? "ENABLED" : "DISABLED");
    if (sensor_config_.imu_enabled) {
        std::cout << " (" << sensor_config_.imu_count << " units)";
    }
    std::cout << std::endl;
}

SensorConfig SensorConfigManager::getDefaultConfig() {
    SensorConfig config;
    config.camera_enabled = true;
    config.camera_count = 4;
    config.lidar_enabled = true;
    config.lidar_count = 1;
    config.imu_enabled = true;
    config.imu_count = 1;
    return config;
}

} // namespace aurora::common
