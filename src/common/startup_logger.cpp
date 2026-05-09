//
// Created by xucong on 26-4-9.
// Copyright (c) 2026 T3CAIC. All rights reserved.
//

#include "startup_logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <fstream>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace aurora::common {

// ANSI颜色代码
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";
    const std::string DIM = "\033[2m";
}

StartupLogger& StartupLogger::instance() {
    static StartupLogger instance;
    return instance;
}

StartupLogger::StartupLogger()
    : mode_(StartupMode::CLI),
      initialized_(false),
      enable_colors_(true) {
    // 初始化阶段状态
    stage_status_[InitStage::INIT_CONFIG] = StageStatus::PENDING;
    stage_status_[InitStage::INIT_SENSOR] = StageStatus::PENDING;
    stage_status_[InitStage::INIT_SYNC] = StageStatus::PENDING;
    stage_status_[InitStage::INIT_ENCODER] = StageStatus::PENDING;
    stage_status_[InitStage::INIT_RECORDER] = StageStatus::PENDING;
    stage_status_[InitStage::INIT_COMPLETE] = StageStatus::PENDING;

    // 检测终端是否支持颜色
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    enable_colors_ = GetConsoleMode(hOut, &dwMode) != 0;
#else
    enable_colors_ = isatty(STDOUT_FILENO) != 0;
#endif
}

StartupLogger::~StartupLogger() {
    shutdown();
}

bool StartupLogger::init(StartupMode mode, const std::string& log_file) {
    mode_ = mode;
    initialized_ = true;

    // 解析版本信息
    parseVersionInfo();

    // PROD模式不需要颜色
    if (mode_ == StartupMode::PROD) {
        enable_colors_ = false;
    }

    return true;
}

void StartupLogger::parseVersionInfo() {
    // 从CMake构建的版本宏获取（需要在编译时定义）
    #ifdef MODULE_VERSION
    system_info_.version = MODULE_VERSION;
    #else
    system_info_.version = "1.3.0"; // 默认版本
    #endif

    // 从环境变量或编译时宏获取git commit
    #ifdef GIT_COMMIT_HASH
    system_info_.git_commit = GIT_COMMIT_HASH;
    #else
    system_info_.git_commit = "unknown";
    #endif

    // 从编译时宏获取构建时间
    #ifdef BUILD_TIMESTAMP
    system_info_.build_time = BUILD_TIMESTAMP;
    #else
    system_info_.build_time = __DATE__ " " __TIME__;
    #endif

    // 环境判断
    const char* env = std::getenv("AER_ENV");
    if (env && strcmp(env, "prod") == 0) {
        system_info_.environment = "PROD";
    } else {
        system_info_.environment = "DEV";
    }

    // ROS2版本
    const char* ros_distro = std::getenv("ROS_DISTRO");
    if (ros_distro) {
        system_info_.ros2_version = std::string("ROS2 ") + ros_distro;
    } else {
        system_info_.ros2_version = "ROS2 Unknown";
    }
}

void StartupLogger::printStartupBanner(const SystemInfo& system_info) {
    system_info_ = system_info;

    if (mode_ == StartupMode::CLI) {
        printAsciiBanner();
    } else {
        printStructuredBanner();
    }
}


void StartupLogger::printAsciiBanner() {
    std::cout << std::endl;

    // AER = Aurora Edge Runtime
    std::cout << applyColor("    ___   ___   ___ ", Colors::BOLD + Colors::CYAN) << std::endl;
    std::cout << applyColor("   / _ | / __/ / _ \\ ", Colors::BOLD + Colors::CYAN) << std::endl;
    std::cout << applyColor("  / __ |/ _/  / , _/ ", Colors::BOLD + Colors::CYAN) << std::endl;
    std::cout << applyColor(" /_/ |_/___/ /_/|_| ", Colors::BOLD + Colors::CYAN) << std::endl;
    std::cout << std::endl;

    int box_width = 50;
    std::string border = applyColor(std::string(box_width, '='), Colors::BOLD + Colors::CYAN);

    std::cout << border << std::endl;

    std::cout << applyColor(" Module : ", Colors::BOLD + Colors::WHITE)
              << "Aurora Edge Runtime" << std::endl;

    std::cout << applyColor(" Version: ", Colors::BOLD + Colors::WHITE)
              << system_info_.version << " (" << system_info_.git_commit << ")" << std::endl;

    std::cout << applyColor(" Built  : ", Colors::BOLD + Colors::WHITE)
              << system_info_.build_time << std::endl;

    std::string env_color = (system_info_.environment == "PROD") ?
        Colors::RED : Colors::GREEN;
    std::cout << applyColor(" Mode   : ", Colors::BOLD + Colors::WHITE)
              << applyColor(system_info_.environment, env_color + Colors::BOLD) << std::endl;

    std::cout << applyColor(" Runtime: ", Colors::BOLD + Colors::WHITE)
              << system_info_.ros2_version << std::endl;

    std::cout << border << std::endl;

    // 传感器配置
    std::cout << applyColor(" Sensors:", Colors::BOLD + Colors::WHITE) << std::endl;

    const auto& s = system_info_.sensors;
    std::cout << "   Camera: "
              << (s.camera_enabled ? applyColor("ON ", Colors::BOLD + Colors::GREEN) : applyColor("--", Colors::DIM));
    if (s.camera_enabled) {
        std::cout << " (" << s.camera_count << " ch)";
    }
    std::cout << std::endl;

    std::cout << "   LiDAR : "
              << (s.lidar_enabled ? applyColor("ON ", Colors::BOLD + Colors::GREEN) : applyColor("--", Colors::DIM));
    if (s.lidar_enabled) {
        std::cout << " (" << s.lidar_count << " ch)";
    }
    std::cout << std::endl;

    std::cout << "   IMU   : "
              << (s.imu_enabled ? applyColor("ON ", Colors::BOLD + Colors::GREEN) : applyColor("--", Colors::DIM));
    if (s.imu_enabled) {
        std::cout << " (" << s.imu_count << " ch)";
    }
    std::cout << std::endl;

    std::cout << border << std::endl;
    std::cout << std::endl;
}

void StartupLogger::printStructuredBanner() {
    // JSON格式的结构化日志
    std::cout << "{\"event\":\"startup\",\"module\":\"" << system_info_.module_name
              << "\",\"version\":\"" << system_info_.version
              << "\",\"commit\":\"" << system_info_.git_commit
              << "\",\"build_time\":\"" << system_info_.build_time
              << "\",\"environment\":\"" << system_info_.environment
              << "\",\"ros2_version\":\"" << system_info_.ros2_version
              << "\",\"sensors\":{"
              << "\"camera\":" << (system_info_.sensors.camera_enabled ? "true" : "false")
              << ",\"camera_count\":" << system_info_.sensors.camera_count
              << ",\"lidar\":" << (system_info_.sensors.lidar_enabled ? "true" : "false")
              << ",\"lidar_count\":" << system_info_.sensors.lidar_count
              << ",\"imu\":" << (system_info_.sensors.imu_enabled ? "true" : "false")
              << ",\"imu_count\":" << system_info_.sensors.imu_count
              << "}}" << std::endl;
}

std::string StartupLogger::applyColor(const std::string& text, const std::string& color_code) {
    if (enable_colors_) {
        return color_code + text + Colors::RESET;
    }
    return text;
}

void StartupLogger::logInitStageBegin(InitStage stage) {
    stage_status_[stage] = StageStatus::IN_PROGRESS;
    stage_start_time_[stage] = std::chrono::steady_clock::now();

    if (mode_ == StartupMode::CLI) {
        std::cout << applyColor("[" + getCurrentTimestamp() + "]", Colors::DIM) << " "
                  << applyColor("INIT", Colors::BLUE) << " "
                  << applyColor(getStageName(stage), Colors::BOLD + Colors::WHITE)
                  << " ... " << std::endl;
    } else {
        std::cout << "{\"event\":\"init_stage_begin\",\"stage\":\"" << getStageName(stage)
                  << "\",\"timestamp\":\"" << getCurrentTimestamp() << "\"}" << std::endl;
    }
}

void StartupLogger::logInitStageEnd(InitStage stage, bool success, const std::string& message) {
    stage_status_[stage] = success ? StageStatus::SUCCESS : StageStatus::FAILED;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - stage_start_time_[stage]).count();

    if (mode_ == StartupMode::CLI) {
        if (success) {
            std::cout << applyColor("OK", Colors::BOLD + Colors::GREEN);
        } else {
            std::cout << applyColor("FAILED", Colors::BOLD + Colors::RED);
        }
        std::cout << " (" << duration << "ms)";
        if (!message.empty()) {
            std::cout << " - " << message;
        }
        std::cout << std::endl;
    } else {
        std::cout << "{\"event\":\"init_stage_end\",\"stage\":\"" << getStageName(stage)
                  << "\",\"status\":\"" << (success ? "success" : "failed")
                  << "\",\"duration_ms\":" << duration
                  << ",\"timestamp\":\"" << getCurrentTimestamp() << "\"";
        if (!message.empty()) {
            std::cout << ",\"message\":\"" << message << "\"";
        }
        std::cout << "}" << std::endl;
    }
}

void StartupLogger::logInitProgress(InitStage stage, int progress, const std::string& message) {
    if (mode_ == StartupMode::CLI) {
        // 简单的进度条
        int bar_width = 30;
        int filled = (progress * bar_width) / 100;

        std::cout << "\r" << applyColor("[" + getCurrentTimestamp() + "]", Colors::DIM) << " "
                  << applyColor(getStageName(stage), Colors::BOLD + Colors::WHITE) << " [";

        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) {
                std::cout << applyColor("=", Colors::GREEN);
            } else if (i == filled) {
                std::cout << applyColor(">", Colors::GREEN);
            } else {
                std::cout << applyColor("-", Colors::DIM);
            }
        }

        std::cout << "] " << std::setw(3) << progress << "%";
        if (!message.empty()) {
            std::cout << " - " << message;
        }
        std::cout << std::flush;
    } else {
        std::cout << "{\"event\":\"init_progress\",\"stage\":\"" << getStageName(stage)
                  << "\",\"progress\":" << progress
                  << ",\"timestamp\":\"" << getCurrentTimestamp() << "\"";
        if (!message.empty()) {
            std::cout << ",\"message\":\"" << message << "\"";
        }
        std::cout << "}" << std::endl;
    }
}

void StartupLogger::printSystemReady() {
    if (mode_ == StartupMode::CLI) {
        std::cout << std::endl;
        std::cout << applyColor("[" + getCurrentTimestamp() + "]", Colors::DIM) << " "
                  << applyColor("SYSTEM READY", Colors::BOLD + Colors::GREEN + Colors::BOLD)
                  << std::endl;
        std::cout << std::endl;
    } else {
        std::cout << "{\"event\":\"system_ready\",\"timestamp\":\""
                  << getCurrentTimestamp() << "\"}" << std::endl;
    }
}

void StartupLogger::logError(const std::string& message) {
    if (mode_ == StartupMode::CLI) {
        std::cout << applyColor("[" + getCurrentTimestamp() + "]", Colors::DIM) << " "
                  << applyColor("ERROR", Colors::BOLD + Colors::RED) << " "
                  << message << std::endl;
    } else {
        std::cout << "{\"event\":\"error\",\"timestamp\":\""
                  << getCurrentTimestamp() << "\",\"message\":\"" << message << "\"}" << std::endl;
    }
}

void StartupLogger::logWarning(const std::string& message) {
    if (mode_ == StartupMode::CLI) {
        std::cout << applyColor("[" + getCurrentTimestamp() + "]", Colors::DIM) << " "
                  << applyColor("WARN", Colors::YELLOW) << " "
                  << message << std::endl;
    } else {
        std::cout << "{\"event\":\"warning\",\"timestamp\":\""
                  << getCurrentTimestamp() << "\",\"message\":\"" << message << "\"}" << std::endl;
    }
}

void StartupLogger::logInfo(const std::string& message) {
    if (mode_ == StartupMode::CLI) {
        std::cout << applyColor("[" + getCurrentTimestamp() + "]", Colors::DIM) << " "
                  << applyColor("INFO", Colors::GREEN) << " "
                  << message << std::endl;
    } else {
        std::cout << "{\"event\":\"info\",\"timestamp\":\""
                  << getCurrentTimestamp() << "\",\"message\":\"" << message << "\"}" << std::endl;
    }
}

void StartupLogger::setSensorConfig(const SensorConfig& config) {
    system_info_.sensors = config;
}

void StartupLogger::shutdown() {
    if (initialized_) {
        initialized_ = false;
    }
}

std::string StartupLogger::getStageName(InitStage stage) {
    switch (stage) {
        case InitStage::INIT_CONFIG:    return "INIT_CONFIG";
        case InitStage::INIT_SENSOR:    return "INIT_SENSOR";
        case InitStage::INIT_SYNC:      return "INIT_SYNC";
        case InitStage::INIT_ENCODER:   return "INIT_ENCODER";
        case InitStage::INIT_RECORDER:  return "INIT_RECORDER";
        case InitStage::INIT_COMPLETE:  return "INIT_COMPLETE";
        default:                        return "UNKNOWN";
    }
}

std::string StartupLogger::getStageDescription(InitStage stage) {
    switch (stage) {
        case InitStage::INIT_CONFIG:    return "Configuration Loading";
        case InitStage::INIT_SENSOR:    return "Sensor Initialization";
        case InitStage::INIT_SYNC:      return "Time Synchronization";
        case InitStage::INIT_ENCODER:   return "Video Encoder Setup";
        case InitStage::INIT_RECORDER:  return "Data Recorder Setup";
        case InitStage::INIT_COMPLETE:  return "Initialization Complete";
        default:                        return "Unknown Stage";
    }
}

std::string StartupLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace aurora::common
