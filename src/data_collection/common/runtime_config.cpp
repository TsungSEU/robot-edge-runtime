//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#include "runtime_config.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <yaml-cpp/yaml.h>

namespace aurora::config {

RuntimeConfig RuntimeConfig::fromArgs(int argc, char** argv) {
    RuntimeConfig config;
    bool show_help = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            show_help = true;
        } else if (arg == "--mode") {
            if (i + 1 < argc) {
                std::string mode_str = argv[++i];
            }
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                config.config_path = argv[++i];
            }
        } else if (arg == "--model") {
            if (i + 1 < argc) {
                config.model_path = argv[++i];
            }
        }
    }

    if (show_help) {
        std::cout << "Usage: aer [OPTIONS]\n\n"
                  << "Options:\n"
                  << "  --mode <MODE>      运行模式: auto (自动驾驶), humanoid (人形机器人)\n"
                  << "                     默认: humanoid\n"
                  << "  --config <PATH>    配置文件路径\n"
                  << "                     默认: config/planner_weights.yaml\n"
                  << "  --model <PATH>     ONNX模型文件路径\n"
                  << "  --help             显示此帮助信息\n"
                  << std::endl;
    }

    // 如果未指定 model_path，从环境变量获取
    if (config.model_path.empty()) {
        const char* env_model_path = std::getenv("AER_MODEL_PATH");
        if (env_model_path && std::string(env_model_path).length() > 0) {
            config.model_path = env_model_path;
        }
    }

    // 从 YAML 配置文件读取 action_type、grid_resolution 和 upload_threshold
    try {
        std::string abs_config = config.config_path;
        if (abs_config.empty() || abs_config[0] != '/') {
            const char* install_root = std::getenv("INSTALL_ROOT_PATH");
            if (install_root) {
                abs_config = std::string(install_root) + "/" + abs_config;
            }
        }
        YAML::Node yaml_config = YAML::LoadFile(abs_config);

        // 读取 action_type
        if (yaml_config["humanoid"] && yaml_config["humanoid"]["action"] &&
            yaml_config["humanoid"]["action"]["action_type"]) {
            config.action_type = yaml_config["humanoid"]["action"]["action_type"].as<std::string>();
        }

        // 读取 grid_resolution 和 upload_threshold
        if (yaml_config["common"]) {
            if (yaml_config["common"]["grid_resolution"]) {
                config.collection.grid_resolution = yaml_config["common"]["grid_resolution"].as<double>();
            }
            if (yaml_config["common"]["upload_threshold"]) {
                config.collection.upload_threshold = yaml_config["common"]["upload_threshold"].as<size_t>();
            }
            if (yaml_config["common"]["trail_publish_interval_sec"]) {
                config.trail_publish_interval_sec = yaml_config["common"]["trail_publish_interval_sec"].as<double>();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[WARN] Failed to read config from YAML: " << e.what()
                  << ", using defaults" << std::endl;
    }

    return config;
}

} // namespace aurora::config
