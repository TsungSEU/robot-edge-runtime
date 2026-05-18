/*
 * Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
 * Tsung Xu<congx0829@163.com>
 */

#include "log_config.h"
#include <fstream>
#include <iostream>
#include "common/log/logger.h"
#include "common/utils/utils.h"
#include "common/base.h"

namespace aurora::collector {

LogConfig& LogConfig::GetInstance() {
    static LogConfig instance;
    return instance;
}

bool LogConfig::Init(const std::string& filePath) {
    if (!CheckFileExists(filePath)) {
        std::cerr << "Error: Configuration file does not exist." << std::endl;
        return false;
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open configuration file." << std::endl;
        return false;
    }

    std::string jsonString((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (!CheckValid(jsonString)) {
        std::cerr << "Error: Configuration file is invalid." << std::endl;
        return false;
    }

    AD_INFO(LogConfig, "CheckValid is true");

    configData = nlohmann::json::parse(jsonString);
    parsedConfig.periodic_info.enable = configData.at("periodic").at("enable");
    parsedConfig.periodic_info.period = configData.at("periodic").at("period");
    for (int it : configData.at("periodic").at("logTypeList")) {
        parsedConfig.periodic_info.logTypeList.emplace_back(it);
    }
    for (const auto& j : configData.at("logs")) {
        LogConfigData::LogInfo log_info;
        auto logType = j.at("logType").get<int>();
        log_info.desc = j.at("desc");
        for (const auto& jj : j.at("apps")) {
            LogConfigData::AppInfo app_info;
            app_info.appId = jj.at("appId");
            std::string root_path = jj.at("logRootPath");
            auto basenames = common::split(jj.at("baseName"), '|');
            if (!root_path.empty() && !basenames.empty()) {
                for (const auto& basename : basenames) {
                    app_info.logPaths.emplace_back(root_path + "/" + basename);
                }
            }
            log_info.apps.emplace_back(app_info);
        }
        parsedConfig.logs[logType] = log_info;
    }
    for (const auto& it : configData.at("savePaths").items()) {
        parsedConfig.savePaths[it.key()] = it.value();
    }

    return true;
}

LogConfigData LogConfig::GetConfig() const {
    return parsedConfig;
}

bool LogConfig::CheckValid(const std::string& jsonString) {
    try {
        nlohmann::json jsonData = nlohmann::json::parse(jsonString);
        return CheckJsonFormat(jsonData);
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

bool LogConfig::CheckFileExists(const std::string& filePath) const {
    return fs::exists(filePath);
}

bool LogConfig::CheckJsonFormat(const nlohmann::json& jsonData) const {
    if (!jsonData.contains("logs") || !jsonData.contains("periodic") || !jsonData.contains("savePaths")) {
        return false;
    }
    if (!jsonData["periodic"].contains("enable") || !jsonData["periodic"].contains("logTypeList") ||
        !jsonData["periodic"].contains("period")) {
        return false;
    }
    for (const auto& j : jsonData["logs"]) {
        if (!j.contains("logType") || !j.contains("desc") || !j.contains("apps")) {
            return false;
        }
        for (const auto& jj : j["apps"]) {
            if (!jj.contains("appId") || !jj.contains("logRootPath") || !jj.contains("baseName")) {
                return false;
            }
        }
    }

    return true;
}

}
