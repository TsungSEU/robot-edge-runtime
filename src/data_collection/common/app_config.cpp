//
// Created by xucong on 25-5-7.
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#include "app_config.h"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <regex>
#include <limits>

namespace {

/**
 * @brief Replace ${ENV_VAR} and ${ENV_VAR:-default} patterns with environment values.
 */
std::string expandEnvVars(const std::string& input) {
    static const std::regex env_pattern(R"(\$\{([^}]+)\})");
    std::string result = input;
    std::smatch match;
    std::string::const_iterator search_start(result.cbegin());

    while (std::regex_search(search_start, result.cend(), match, env_pattern)) {
        std::string full_match = match[0].str();
        std::string expr = match[1].str();

        // Check for default value syntax: VAR:-default
        std::string var_name;
        std::string default_val;
        auto sep_pos = expr.find(":-");
        if (sep_pos != std::string::npos) {
            var_name = expr.substr(0, sep_pos);
            default_val = expr.substr(sep_pos + 2);
        } else {
            var_name = expr;
        }

        const char* env_val = std::getenv(var_name.c_str());
        std::string replacement = (env_val && std::string(env_val).length() > 0)
                                  ? std::string(env_val)
                                  : default_val;

        size_t pos = result.find(full_match);
        result.replace(pos, full_match.length(), replacement);
        search_start = result.cbegin() + pos + replacement.length();
    }

    return result;
}

/**
 * @brief Recursively expand environment variables in JSON string values.
 */
void expandEnvVarsInJson(nlohmann::json& j) {
    if (j.is_string()) {
        j = expandEnvVars(j.get<std::string>());
    } else if (j.is_object()) {
        for (auto& [key, val] : j.items()) {
            expandEnvVarsInJson(val);
        }
    } else if (j.is_array()) {
        for (auto& elem : j) {
            expandEnvVarsInJson(elem);
        }
    }
}

} // anonymous namespace

namespace aurora::collector {

AppConfig& AppConfig::getInstance() {
    static AppConfig instance;
    return instance;
}

bool AppConfig::Init(const std::string& filePath) {
    if (!checkFileExists(filePath)) {
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

    std::cout << "AppConfig CheckValid is true"  << std::endl;

    configData = nlohmann::json::parse(jsonString);

    // Expand environment variables in all string values (${ENV_VAR} and ${ENV_VAR:-default})
    expandEnvVarsInJson(configData);

    // ===== 数值校验辅助 =====
    auto safeInt = [](const nlohmann::json& j, const std::string& key, int default_val,
                      int min_val = std::numeric_limits<int>::min(),
                      int max_val = std::numeric_limits<int>::max()) -> int {
        if (!j.contains(key) || !j[key].is_number()) return default_val;
        int val = j[key].get<int>();
        if (val < min_val || val > max_val) {
            std::cerr << "Config warning: " << key << "=" << val
                      << " out of range [" << min_val << "," << max_val
                      << "], using default " << default_val << std::endl;
            return default_val;
        }
        return val;
    };

    // DataStorage
    parsedConfig.dataStorage.rollingDeleteThreshold = safeInt(configData["dataStorage"], "rollingDeleteThreshold", 10, 0, 100);
    parsedConfig.dataStorage.rollInterval = safeInt(configData["dataStorage"], "rollInterval", 10, 1, 3600);
    parsedConfig.dataStorage.bagInterval = safeInt(configData["dataStorage"], "bagInterval", 10, 1, 3600);
    parsedConfig.dataStorage.capacityMb = configData["dataStorage"].value("capacityMb", (uint64_t)102400);
    parsedConfig.dataStorage.requriedSpaceMb = configData["dataStorage"].value("requiredSpaceMb", (uint64_t)256);
    parsedConfig.dataStorage.storagePaths["bagPath"] = configData["dataStorage"]["storagePaths"].value("bagPath", "/tmp/aer/bag");
    parsedConfig.dataStorage.storagePaths["encPath"] = configData["dataStorage"]["storagePaths"].value("encPath", "/tmp/aer/enc");
    parsedConfig.dataStorage.compressionFormat = configData["dataStorage"].value("compressionFormat", std::string("lz4"));

    // DataProto
    parsedConfig.dataProto.vin = configData["dataProto"]["vin"];
    parsedConfig.dataProto.software_version = configData["dataProto"]["software_version"];
    parsedConfig.dataProto.hardware_version = configData["dataProto"]["hardware_version"];
    parsedConfig.dataProto.device = configData["dataProto"]["device"];
    parsedConfig.dataProto.device_id = configData["dataProto"]["device_id"];
    parsedConfig.dataProto.mqtt.broker = configData["dataProto"]["mqtt"]["broker"];
    parsedConfig.dataProto.mqtt.broker_ssl = configData["dataProto"]["mqtt"]["broker_ssl"];
    parsedConfig.dataProto.mqtt.username = configData["dataProto"]["mqtt"]["username"];
    parsedConfig.dataProto.mqtt.password = configData["dataProto"]["mqtt"]["password"];
    parsedConfig.dataProto.mqtt.upTopic = configData["dataProto"]["mqtt"]["upTopic"];
    parsedConfig.dataProto.mqtt.downTopic = configData["dataProto"]["mqtt"]["downTopic"];

    // DataUpload
    parsedConfig.dataUpload.retryCount = (int8_t)configData["dataUpload"]["retryCount"];
    parsedConfig.dataUpload.retryIntervalSec = (int64_t)configData["dataUpload"]["retryIntervalSec"];
    parsedConfig.dataUpload.uploadFileIntervalMs = (int64_t)configData["dataUpload"]["uploadFileIntervalMs"];
    parsedConfig.dataUpload.uploadFileSliceIntervalMs = (int64_t)configData["dataUpload"]["uploadFileSliceIntervalMs"];
    parsedConfig.dataUpload.uploadFileSliceSizeMb = (size_t)configData["dataUpload"]["uploadFileSliceSizeMb"];
    parsedConfig.dataUpload.clientCertPath = configData["dataUpload"]["clientCertPath"];
    parsedConfig.dataUpload.clientKeyPath = configData["dataUpload"]["clientKeyPath"];
    parsedConfig.dataUpload.caCertPath = configData["dataUpload"]["caCertPath"];
    parsedConfig.dataUpload.gateway = std::string(configData["dataUpload"]["gateway"]);
    parsedConfig.dataUpload.fileRecordPath = configData["dataUpload"]["fileRecordPath"];
    parsedConfig.dataUpload.filenameRegex = std::string(configData["dataUpload"]["filenameRegex"]);
    parsedConfig.dataUpload.uploadPaths.emplace("encPath", configData["dataUpload"]["uploadPaths"]["encPath"]);
    parsedConfig.dataUpload.rsa_pub_key_path = configData["dataUpload"]["publicKeyPath"];
    parsedConfig.dataUpload.watch_dir = configData["dataUpload"]["uploadPaths"]["bagPath"];
    parsedConfig.dataUpload.enc_dir = configData["dataUpload"]["uploadPaths"]["encPath"];

    // AWS S3配置
    parsedConfig.dataUpload.aws.enabled = configData["dataUpload"]["aws"]["enabled"];
    parsedConfig.dataUpload.aws.accessKeyId = configData["dataUpload"]["aws"]["accessKeyId"];
    parsedConfig.dataUpload.aws.secretAccessKey = configData["dataUpload"]["aws"]["secretAccessKey"];
    parsedConfig.dataUpload.aws.region = configData["dataUpload"]["aws"]["region"];
    parsedConfig.dataUpload.aws.bucketName = configData["dataUpload"]["aws"]["bucketName"];
    parsedConfig.dataUpload.aws.endpointUrl = configData["dataUpload"]["aws"]["endpointUrl"];

    // Log
    parsedConfig.log.logLevel = configData["log"]["LOG_level"];
    parsedConfig.log.logPattern = configData["log"]["LOG_pattern"];
    parsedConfig.log.logPath = configData["log"]["LOG_path"];
    parsedConfig.log.logBasename = configData["log"]["LOG_basename"];

    // Debug
    parsedConfig.debug.closeMqttSsl = configData["debug"]["closeMqttSsl"];
    parsedConfig.debug.closeDataReporter = configData["debug"]["closeDataReporter"];
    parsedConfig.debug.closeDataStorage = configData["debug"]["closeDataStorage"];
    parsedConfig.debug.closeDataEnc = configData["debug"]["closeDataEnc"];
    parsedConfig.debug.closeDataUpload = configData["debug"]["closeDataUpload"];
    parsedConfig.debug.deleteFileAfterDataUpload = configData["debug"]["deleteFileAfterDataUpload"];
    parsedConfig.debug.closeLogUpload = configData["debug"]["closeLogUpload"];
    parsedConfig.debug.cloudtimeOutMs = configData["debug"]["cloudtimeOutMs"];

    return true;
}

AppConfigData AppConfig::GetConfig() const {
    return parsedConfig;
}

bool AppConfig::CheckValid(const std::string& jsonString) {
    try {
        nlohmann::json jsonData = nlohmann::json::parse(jsonString);
        return checkJsonFormat(jsonData);
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

bool AppConfig::checkFileExists(const std::string& filePath) const {
    return std::filesystem::exists(filePath);
}

bool AppConfig::checkJsonFormat(const nlohmann::json& jsonData) const {
    if (!jsonData.contains("dataStorage") || !jsonData.contains("dataUpload")||
        !jsonData.contains("dataProto")|| !jsonData.contains("log") ||
        !jsonData.contains("debug")) {
        return false;
    }

    // DataStorage
    if (!jsonData["dataStorage"].contains("rollingDeleteThreshold") || !jsonData["dataStorage"].contains("rollInterval") ||
        !jsonData["dataStorage"].contains("bagInterval") || !jsonData["dataStorage"].contains("storagePaths") ||
        !jsonData["dataStorage"].contains("capacityMb") || !jsonData["dataStorage"].contains("requiredSpaceMb")) {
        return false;
    }

    // DataProto
    if (!jsonData["dataProto"].contains("vin") || !jsonData["dataProto"].contains("software_version") ||
        !jsonData["dataProto"].contains("hardware_version") || !jsonData["dataProto"].contains("device") ||
        !jsonData["dataProto"].contains("device_id") || !jsonData["dataProto"].contains("mqtt")) {
        return false;
    }

    // DataUpload
    if (!jsonData["dataUpload"].contains("retryCount") || !jsonData["dataUpload"].contains("retryIntervalSec") ||
        !jsonData["dataUpload"].contains("uploadFileIntervalMs") || !jsonData["dataUpload"].contains("uploadFileSliceIntervalMs") ||
        !jsonData["dataUpload"].contains("uploadFileSliceSizeMb") || !jsonData["dataUpload"].contains("clientCertPath") ||
        !jsonData["dataUpload"].contains("clientKeyPath") || !jsonData["dataUpload"].contains("caCertPath") ||
        !jsonData["dataUpload"].contains("gateway") || !jsonData["dataUpload"].contains("fileRecordPath") ||
        !jsonData["dataUpload"].contains("uploadPaths")) {
        return false;
    }

    if (jsonData["dataUpload"].contains("aws")) {
        if (!jsonData["dataUpload"]["aws"].contains("enabled") || !jsonData["dataUpload"]["aws"].contains("region") ||
            !jsonData["dataUpload"]["aws"].contains("bucketName") || !jsonData["dataUpload"]["aws"].contains("endpointUrl")) {
            return false;
        }
    }

    // Log
    if (!jsonData["log"].contains("LOG_level") || !jsonData["log"].contains("LOG_pattern") ||
        !jsonData["log"].contains("LOG_path") || !jsonData["log"].contains("LOG_basename")) {
        return false;
    }

    // Debug
    if (!jsonData["debug"].contains("closeMqttSsl") || !jsonData["debug"].contains("closeDataReporter") ||
        !jsonData["debug"].contains("closeDataStorage") || !jsonData["debug"].contains("closeDataEnc") ||
        !jsonData["debug"].contains("closeDataUpload") || !jsonData["debug"].contains("deleteFileAfterDataUpload") ||
        !jsonData["debug"].contains("closeLogUpload") || !jsonData["debug"].contains("cloudtimeOutMs")) {
        return false;
    }

    return true;
}


}