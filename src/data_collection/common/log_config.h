/*
 * Copyright (c) 2025 T3CAIC Group Limited. All rights reserved.
 * Tsung Xu<xucong@t3caic.com>
 */

#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H


#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "common/data.h"

namespace aurora::collector {

struct LogConfigData {
    struct AppInfo {
        std::string appId;
        std::vector<std::string> logPaths;
    };
    struct LogInfo {
        std::string desc;
        std::vector<AppInfo> apps;
    };
    std::unordered_map<int, LogInfo> logs;
    struct PeriodicInfo {
        bool enable;
        uint64_t period;
        std::vector<int> logTypeList;
    } periodic_info;
    std::unordered_map<std::string, std::string> savePaths;
};

class LogConfig {
public:
    static LogConfig& GetInstance();
    bool Init(const std::string& filePath);
    LogConfigData GetConfig() const;

private:
    LogConfig() = default;
    LogConfig(const LogConfig&) = delete;
    LogConfig& operator=(const LogConfig&) = delete;

    bool CheckValid(const std::string& jsonString);
    bool CheckFileExists(const std::string& filePath) const;
    bool CheckJsonFormat(const nlohmann::json& jsonData) const;

    nlohmann::json configData;
    LogConfigData parsedConfig;
};

}

#endif //LOG_CONFIG_H
