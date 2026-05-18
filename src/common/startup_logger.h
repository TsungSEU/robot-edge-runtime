//
// Created by xucong on 26-4-9.
// Copyright (c) 2026 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
//

#ifndef AURORA_STARTUP_LOGGER_H
#define AURORA_STARTUP_LOGGER_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <chrono>

namespace aurora::common {

/**
 * @brief 启动日志模式枚举
 */
enum class StartupMode {
    PROD,  // 生产模式：结构化日志、无ASCII、适合机器解析
    CLI    // 开发模式：带ASCII banner、用户友好输出
};

/**
 * @brief 传感器配置结构
 */
struct SensorConfig {
    bool camera_enabled = false;
    bool lidar_enabled = false;
    bool imu_enabled = false;
    int camera_count = 0;
    int lidar_count = 0;
    int imu_count = 0;
};

/**
 * @brief 系统信息结构
 */
struct SystemInfo {
    std::string module_name = "Aurora-Edge-Runtime";
    std::string version;           // 从CMakeLists.txt获取
    std::string git_commit;        // 从git获取
    std::string build_time;        // 编译时间
    std::string environment;       // PROD / DEV
    std::string ros2_version;      // ROS2版本
    SensorConfig sensors;
};

/**
 * @brief 启动阶段枚举
 */
enum class InitStage {
    INIT_CONFIG,
    INIT_SENSOR,
    INIT_SYNC,
    INIT_ENCODER,
    INIT_RECORDER,
    INIT_COMPLETE
};

/**
 * @brief 启动阶段状态
 */
enum class StageStatus {
    PENDING,
    IN_PROGRESS,
    SUCCESS,
    FAILED
};

/**
 * @brief 启动日志类
 *
 * 提供车端数据采集系统的启动日志功能，支持PROD和CLI两种模式
 */
class StartupLogger {
public:
    /**
     * @brief 获取单例实例
     */
    static StartupLogger& instance();

    /**
     * @brief 初始化启动日志器
     * @param mode 运行模式 (PROD/CLI)
     * @param log_file 日志文件路径 (可选，PROD模式推荐)
     * @return 初始化是否成功
     */
    bool init(StartupMode mode, const std::string& log_file = "");

    /**
     * @brief 打印启动Banner
     * @param system_info 系统信息
     */
    void printStartupBanner(const SystemInfo& system_info);

    /**
     * @brief 记录初始化阶段开始
     * @param stage 初始化阶段
     */
    void logInitStageBegin(InitStage stage);

    /**
     * @brief 记录初始化阶段完成
     * @param stage 初始化阶段
     * @param success 是否成功
     * @param message 附加消息
     */
    void logInitStageEnd(InitStage stage, bool success, const std::string& message = "");

    /**
     * @brief 记录初始化阶段进度
     * @param stage 初始化阶段
     * @param progress 进度百分比 (0-100)
     * @param message 附加消息
     */
    void logInitProgress(InitStage stage, int progress, const std::string& message = "");

    /**
     * @brief 打印系统就绪消息
     */
    void printSystemReady();

    /**
     * @brief 打印错误消息
     * @param message 错误消息
     */
    void logError(const std::string& message);

    /**
     * @brief 打印警告消息
     * @param message 警告消息
     */
    void logWarning(const std::string& message);

    /**
     * @brief 打印信息消息
     * @param message 信息消息
     */
    void logInfo(const std::string& message);

    /**
     * @brief 设置传感器配置
     * @param config 传感器配置
     */
    void setSensorConfig(const SensorConfig& config);

    /**
     * @brief 获取当前模式
     */
    StartupMode getMode() const { return mode_; }

    /**
     * @brief 获取系统信息
     */
    const SystemInfo& getSystemInfo() const { return system_info_; }

    /**
     * @brief 关闭日志器
     */
    void shutdown();

private:
    StartupLogger();
    ~StartupLogger();

    // 禁止拷贝和赋值
    StartupLogger(const StartupLogger&) = delete;
    StartupLogger& operator=(const StartupLogger&) = delete;

    /**
     * @brief 获取阶段名称
     */
    static std::string getStageName(InitStage stage);

    /**
     * @brief 获取阶段描述
     */
    static std::string getStageDescription(InitStage stage);

    /**
     * @brief 打印ASCII Banner (CLI模式)
     */
    void printAsciiBanner();

    /**
     * @brief 打印结构化Banner (PROD模式)
     */
    void printStructuredBanner();

    /**
     * @brief 应用颜色 (CLI模式)
     */
    std::string applyColor(const std::string& text, const std::string& color_code);

    /**
     * @brief 获取当前时间戳
     */
    static std::string getCurrentTimestamp();

    /**
     * @brief 解析版本信息
     */
    void parseVersionInfo();

private:
    StartupMode mode_;
    SystemInfo system_info_;
    bool initialized_;
    bool enable_colors_;
    std::map<InitStage, StageStatus> stage_status_;
    std::map<InitStage, std::chrono::steady_clock::time_point> stage_start_time_;
};

/**
 * @brief 便捷宏：打印启动Banner
 */
#define PRINT_STARTUP_BANNER(info) \
    aurora::common::StartupLogger::instance().printStartupBanner(info)

/**
 * @brief 便捷宏：记录初始化阶段
 */
#define LOG_INIT_STAGE_BEGIN(stage) \
    aurora::common::StartupLogger::instance().logInitStageBegin(aurora::common::InitStage::stage)

#define LOG_INIT_STAGE_END(stage, success, msg) \
    aurora::common::StartupLogger::instance().logInitStageEnd(aurora::common::InitStage::stage, success, msg)

/**
 * @brief 便捷宏：系统就绪
 */
#define PRINT_SYSTEM_READY() \
    aurora::common::StartupLogger::instance().printSystemReady()

} // namespace aurora::common

#endif // AURORA_STARTUP_LOGGER_H
