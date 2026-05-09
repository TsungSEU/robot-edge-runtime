//
// Created by xucong on 26-4-9.
// Copyright (c) 2026 T3CAIC. All rights reserved.
//

#ifndef AURORA_SENSOR_CONFIG_H
#define AURORA_SENSOR_CONFIG_H

#include "startup_logger.h"
#include <string>
#include <vector>
#include <map>

namespace aurora::common {

/**
 * @brief 传感器配置管理器
 *
 * 从配置文件读取传感器信息，并提供给启动日志使用
 */
class SensorConfigManager {
public:
    /**
     * @brief 获取单例实例
     */
    static SensorConfigManager& instance();

    /**
     * @brief 从配置文件加载传感器配置
     * @param config_path 配置文件路径
     * @return 是否加载成功
     */
    bool loadFromConfig(const std::string& config_path = "config/sensor_config.json");

    /**
     * @brief 自动检测系统中的传感器（通过ROS2话题）
     * @return 是否检测成功
     */
    bool autoDetect();

    /**
     * @brief 手动设置传感器配置
     * @param config 传感器配置
     */
    void setSensorConfig(const SensorConfig& config);

    /**
     * @brief 获取传感器配置
     * @return 传感器配置
     */
    const SensorConfig& getSensorConfig() const { return sensor_config_; }

    /**
     * @brief 设置单个传感器状态
     * @param sensor_type 传感器类型 (camera/lidar/imu)
     * @param enabled 是否启用
     * @param count 数量
     */
    void setSensor(const std::string& sensor_type, bool enabled, int count = 1);

    /**
     * @brief 打印传感器配置摘要
     */
    void printSummary() const;

private:
    SensorConfigManager() = default;
    ~SensorConfigManager() = default;

    // 禁止拷贝和赋值
    SensorConfigManager(const SensorConfigManager&) = delete;
    SensorConfigManager& operator=(const SensorConfigManager&) = delete;

    /**
     * @brief 解析 JSON 配置文件
     */
    bool parseJsonConfig(const std::string& json_content);

    /**
     * @brief 获取默认传感器配置
     */
    SensorConfig getDefaultConfig();

private:
    SensorConfig sensor_config_;
    bool initialized_ = false;
};

} // namespace aurora::common

#endif // AURORA_SENSOR_CONFIG_H
