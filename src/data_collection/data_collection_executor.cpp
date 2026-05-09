// data_collection_executor.cpp

#include "data_collection_executor.h"
#include "aurora_edge_runtime/srv/trigger_recording.hpp"
#include "common/log/logger.h"
#include "trigger/strategy/strategy_parser.h"
#include "recorder/file_compress.h"
#include "recorder/file_roller.h"
#include "common/app_config.h"
#include "common/config_watcher.h"
#include "compliance/metadata_manifest.h"
#include <ctime>
#include <filesystem>
#include <sstream>
#include <mutex>
#include <condition_variable>

namespace fs = std::filesystem;
static std::string kAppConfigPath = "/config/app_config.json";

namespace aurora::collector {

DataCollectionExecutor::DataCollectionExecutor(std::shared_ptr<rclcpp::Node> node): node_(node) {
    std::string app_config_path = std::string(getInstallRootPath()) + kAppConfigPath;

    // 初始化AppConfig
    bool ok = AppConfig::getInstance().Init(app_config_path);
    if (!ok) {
        AD_ERROR(DataCollectionPlanner, "AppConfig::Init failed");
    }

    // 初始化磁盘空间检查器
    diskSpaceChecker = std::make_shared<DiskSpaceChecker>();
    if (!diskSpaceChecker) {
        AD_ERROR(DataCollectionExecutor, "Failed to create DiskSpaceChecker");
    } else {
        diskSpaceChecker->setThreshold(90.0f);
    }

    // 设置默认数据路径
    auto appconfig = AppConfig::getInstance().GetConfig();
    auto it = appconfig.dataStorage.storagePaths.find("bagPath");
    if (it != appconfig.dataStorage.storagePaths.end()) {
        data_path_ = it->second;
    }
    data_path_ = data_path_.empty() ? "./data" : data_path_;

    // 创建数据目录
    if (!fs::exists(data_path_)) {
        if (!fs::create_directories(data_path_)) {
            AD_ERROR(DataCollectionExecutor, "Failed to create data directory: %s", data_path_.c_str());
        }
    }

    last_trigger_finish_time = common::GetCurrentTimestamp();

    // Create isolated callback group for trigger service to prevent blocking
    // odom and channel callbacks
    trigger_callback_group_ = node_->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);

    // Create ROS2 service for trigger requests
    trigger_service_ = node_->create_service<aurora_edge_runtime::srv::TriggerRecording>(
        "/robot/trigger",
        [this](const std::shared_ptr<aurora_edge_runtime::srv::TriggerRecording::Request> request,
               std::shared_ptr<aurora_edge_runtime::srv::TriggerRecording::Response> response) {
            this->handleTriggerService(request, response);
        },
        rmw_qos_profile_services_default,
        trigger_callback_group_
    );

    // Start background recording + compress worker
    record_worker_running_.store(true);
    record_worker_thread_ = std::thread(&DataCollectionExecutor::recordWorkerLoop, this);

    AD_INFO(DataCollectionExecutor, "TriggerRecording service initialized: /robot/trigger (isolated callback group)");
    AD_INFO(DataCollectionExecutor, "DataCollectionExecutor created successfully");
}

DataCollectionExecutor::~DataCollectionExecutor() {
    // Stop record worker
    record_worker_running_.store(false);
    record_cv_.notify_all();
    if (record_worker_thread_.joinable()) {
        record_worker_thread_.join();
    }

    if (is_recording_.load()) {
        stopRecording();
    }
}

bool DataCollectionExecutor::initialize(const std::string& json_config_path) {
    AD_INFO(DataCollectionExecutor, "Initializing from JSON config: %s", json_config_path.c_str());

    // 加载 JSON 配置
    auto parser = std::make_shared<StrategyParser>();
    if (!parser->LoadConfigFromFile(json_config_path, strategy_config_)) {
        AD_ERROR(DataCollectionExecutor, "Failed to load JSON config: %s", json_config_path.c_str());
        return false;
    } else {
        std::cout << "[OK] JSON configuration loaded successfully" << std::endl;
        std::cout << "  Config ID: " << strategy_config_.configId << std::endl;
        std::cout << "  Strategy count: " << strategy_config_.strategies.size() << std::endl;

        // Display configured topics
        for (const auto& strategy : strategy_config_.strategies) {
            if (strategy.trigger.enabled) {
                std::cout << "\n  Strategy: " << strategy.businessType << std::endl;
                std::cout << "    Trigger: " << strategy.trigger.triggerId << std::endl;
                std::cout << "    Recording window: forward " << (int)strategy.mode.cacheMode.forwardCaptureDurationSec
                         << " sec + backward " << (int)strategy.mode.cacheMode.backwardCaptureDurationSec << " sec" << std::endl;
                std::cout << "    Subscribed topics (" << strategy.cyclone.channels.size() << "):" << std::endl;

                for (const auto& channel : strategy.cyclone.channels) {
                    std::cout << "      - " << channel.topic
                             << " (" << channel.type << ", " << (int)channel.capturedFrameRate << " Hz)" << std::endl;
                }
            }
        }
    }

    return initialize(strategy_config_);
}

bool DataCollectionExecutor::initialize(const StrategyConfig& strategy_config) {
    strategy_config_ = strategy_config;

    // 获取第一个启用的策略
    for (const auto& s : strategy_config_.strategies) {
        if (s.trigger.enabled) {
            strategy_ = std::make_shared<Strategy>(s);
            break;
        }
    }

    if (!strategy_) {
        AD_ERROR(DataCollectionExecutor, "No enabled strategy found in config");
        return false;
    }

    // 创建 TriggerManager
    trigger_manager_ = std::make_shared<TriggerManager>();
    if (!trigger_manager_)
    {
        AD_ERROR(DataCollectionExecutor, "Failed to create TriggerManager");
        return false;
    }

    // 初始化 TriggerManager
    if (!trigger_manager_->initialize(strategy_config_)) {
        AD_WARN(DataCollectionExecutor, "Failed to initialize TriggerManager, continuing without trigger");
    }

    // 创建 Ros2BagRecorder
    bag_recorder_ = std::make_shared<Ros2BagRecorder>(node_);
    if (!bag_recorder_->Init()) {
        AD_ERROR(DataCollectionExecutor, "Failed to initialize Ros2BagRecorder");
        return false;
    }

    // 设置策略配置
    bag_recorder_->setStrategy(strategy_);
    bag_recorder_->setCacheMode(strategy_->mode.cacheMode);

    // 初始化环形缓冲区
    if (!bag_recorder_->InitRingBuffers()) {
        AD_ERROR(DataCollectionExecutor, "Failed to initialize ring buffers");
        return false;
    }

    // AD_INFO(DataCollectionExecutor, "Ring buffers initialized for %zu topics",
    //          strategy_->cyclone.channels.size());

    // 创建 ChannelManager
    channel_manager_ = std::make_shared<ChannelManager>();
    if (!channel_manager_->Init(node_, strategy_config_)) {
        AD_ERROR(DataCollectionExecutor, "Failed to initialize ChannelManager");
        return false;
    }

    // 将 Ros2BagRecorder 注册为 observer
    // 这样消息才能从 topics 流向 环形缓冲区
    auto trigger = trigger_manager_->getTrigger();
    channel_manager_->AddObserver(trigger);

    if (strategy_->enableMasking) {
        auto cc = compliance::ComplianceConfig::fromStrategy(*strategy_);
        compliance_filter_ = std::make_shared<compliance::ComplianceFilter>(node_, cc);
        compliance_filter_->setDownstream(bag_recorder_);
        channel_manager_->AddObserver(compliance_filter_);
        AD_INFO(DataCollectionExecutor, "Compliance filter enabled (geo=%d, image=%d)",
                cc.geo_enabled, cc.image_enabled);
    } else {
        channel_manager_->AddObserver(bag_recorder_);
    }

    AD_INFO(DataCollectionExecutor, "DataCollectionExecutor initialized successfully");
    // AD_INFO(DataCollectionExecutor, "  Subscribed to %zu topics", strategy_->cyclone.channels.size());
    // AD_INFO(DataCollectionExecutor, "  Ring buffer: forward=%ds, backward=%ds",
    //          strategy_->mode.cacheMode.forwardCaptureDurationSec,
    //          strategy_->mode.cacheMode.backwardCaptureDurationSec);

    is_initialized_.store(true);
    last_trigger_finish_time = common::GetCurrentTimestamp();

    return true;
}

std::vector<DataPoint> DataCollectionExecutor::executeAlongPath(
    const std::vector<Point>& path,
    const std::string& output_bag_path) {

    if (!is_initialized_.load()) {
        AD_ERROR(DataCollectionExecutor, "Executor not initialized, call initialize() first");
        return {};
    }

    // 检查必要的组件
    if (!bag_recorder_) {
        AD_ERROR(DataCollectionExecutor, "bag_recorder_ is null!");
        return {};
    }
    if (!strategy_) {
        AD_WARN(DataCollectionExecutor, "strategy_ is null, using default values");
    }

    AD_INFO(DataCollectionExecutor, "Executing data collection along path with %zu waypoints",
             path.size());

    if (path.empty()) {
        AD_WARN(DataCollectionExecutor, "Empty path provided");
        return {};
    }

    std::vector<DataPoint> collected_data;

    // ========== 遍历路径，在需要采集的点触发录制 ==========
    for (size_t i = 0; i < path.size(); ++i) {
        if (!running()) break;  // 检查是否还在运行

        const Point& waypoint = path[i];

        // 判断是否应该采集
        if (!shouldCollectAt(waypoint)) {
            AD_DEBUG(DataCollectionExecutor, "Skipping waypoint %zu at (%.2f, %.2f)",
                     i, waypoint.x, waypoint.y);
            continue;
        }

        // 执行采集 - 生成元数据
        total_attempts_++;
        auto data_point = executeAtPoint(waypoint);

        // 使用 TriggerRecord 触发环形缓冲区录制
        // TriggerRecord 会：1)提取前向缓存 2)等待后向录制 3)自动落盘
        uint64_t trigger_timestamp = common::GetCurrentTimestamp();
        std::string trigger_id = strategy_ ? strategy_->trigger.triggerId : "10000";
        trigger_id = "waypoint_" + std::to_string(i) + "_" + trigger_id;
        std::string business_type = strategy_ ? strategy_->businessType : "default";
        std::string bag_path = data_path_ + "/" + generateBagFilename(trigger_id, business_type);

        AD_INFO(DataCollectionExecutor, "Triggering recording at waypoint %zu: %s",
                 i, bag_path.c_str());

        bag_recorder_->TriggerRecord(trigger_timestamp, bag_path);
        total_recordings_++;

        auto ret = compress(bag_path);
        if (!ret) AD_ERROR(DataCollectionExecutor, "Failed to compress bag: %s", bag_path.c_str());

        // 如果成功采集，添加到结果集合
        if (data_point.has_value()) {
            collected_data.push_back(data_point.value());
        }

        // 短暂等待，模拟机器人移动到下一个点
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 触发回调通知新数据
    if (!collected_data.empty()) {
        triggerCallback(collected_data);
    }

    AD_INFO(DataCollectionExecutor, "Path execution completed: %zu points collected out of %zu waypoints",
             collected_data.size(), path.size());

    total_collected_ += collected_data.size();
    return collected_data;
}

bool DataCollectionExecutor::startRecording(const std::string& output_bag_path) {
    if (is_recording_.load()) {
        AD_WARN(DataCollectionExecutor, "Already recording");
        return false;
    }

    AD_INFO(DataCollectionExecutor, "Starting recording to: %s", output_bag_path.c_str());

    if (!bag_recorder_->Open(OptMode::WRITE, output_bag_path)) {
        AD_ERROR(DataCollectionExecutor, "Failed to open bag file");
        return false;
    }

    is_recording_.store(true);
    current_bag_path_ = output_bag_path;
    // total_recordings_++;

    return true;
}

bool DataCollectionExecutor::stopRecording() {
    if (!is_recording_.load()) {
        AD_WARN(DataCollectionExecutor, "Not recording");
        return false;
    }

    AD_INFO(DataCollectionExecutor, "Stopping recording...");
    bag_recorder_->Close();
    is_recording_.store(false);

    auto bag_info = bag_recorder_->GetStatistics();
    AD_INFO(DataCollectionExecutor, "Recording stopped: %zu messages",
             bag_info.total_messages);

    // 压缩并上传
    if (!current_bag_path_.empty()) {
        compress(current_bag_path_);
        current_bag_path_.clear();
    }

    return true;
}

bool DataCollectionExecutor::shouldCollectAt(const Point& point) {
    if (!trigger_manager_) {
        return true;  // 如果没有 trigger，默认采集
    }

    Point trigger_point(point.x, point.y);
    return trigger_manager_->shouldTrigger(trigger_point);
}

std::optional<DataPoint> DataCollectionExecutor::executeAtPoint(const Point& point) {
    AD_DEBUG(DataCollectionExecutor, "Executing collection at (%.2f, %.2f)",
             point.x, point.y);

    // Notify trigger manager about position visit (lightweight method)
    if (trigger_manager_) {
        trigger_manager_->notifyPositionVisited(point, common::GetCurrentTimestamp());
    }

    // 实际数据采集由 ChannelManager 持续进行到环形缓冲区
    // 这里我们生成 DataPoint 元数据用于追踪
    std::string sensor_data = "data_at_" + std::to_string(point.x) + "_" + std::to_string(point.y)
                               + "_t_" + std::to_string(common::GetCurrentTimestamp());

    if (sensor_data.empty()) {
        AD_WARN(DataCollectionExecutor, "No sensor data collected at (%.2f, %.2f)",
                 point.x, point.y);
        return std::nullopt;
    }

    // 创建数据点
    DataPoint data_point(point, sensor_data, static_cast<double>(std::time(nullptr)));

    return data_point;
}

TBagInfo DataCollectionExecutor::getRecordingStatistics() const {
    if (bag_recorder_) {
        return bag_recorder_->GetStatistics();
    }
    return TBagInfo();
}

std::string DataCollectionExecutor::generateBagFilename(const std::string& trigger_id,
                                                      const std::string& business_type) {
    std::string data_source = "EmbodiedAI";
    std::string timestamp_str = common::UnixSecondsToString(
        common::GetCurrentTimestamp() / 1000000,
        "%Y%m%d%H%M%S"
    );

    // 安全检查：确保 trigger_id 和 business_type 不为空
    std::string safe_trigger_id = trigger_id.empty() ? "unknown" : trigger_id;
    std::string safe_business_type = business_type.empty() ? "default" : business_type;

    std::stringstream ss;
    ss << data_source << "_" << timestamp_str << "_"
       << safe_business_type << "_" << safe_trigger_id;
    return ss.str();
}

void DataCollectionExecutor::triggerCallback(const std::vector<DataPoint>& new_data) {
    if (data_callback_) {
        AD_DEBUG(DataCollectionExecutor, "Triggering data callback with %zu data points",
                 new_data.size());
        data_callback_(new_data);
    }
}

bool DataCollectionExecutor::compress(const std::string& bag_path) {
    // 检查文件是否存在
    if (!fs::exists(bag_path)) {
        AD_ERROR(DataCollectionExecutor, "Bag file not found: %s", bag_path.c_str());
        return false;
    }

    // 读取压缩格式配置
    auto appconfig = AppConfig::getInstance().GetConfig();
    std::string fmt = appconfig.dataStorage.compressionFormat;
    FileCompress::CompressionFormat compressionFormat = FileCompress::CompressionFormat::Lz4;
    std::string extension = ".tar.lz4";
    if (fmt == "gzip") {
        compressionFormat = FileCompress::CompressionFormat::Gzip;
        extension = ".tar.gz";
    }

    // 生成压缩文件路径
    std::string compressed_path = bag_path + extension;

    AD_DEBUG(DataCollectionExecutor, "Compressing %s -> %s...",
             bag_path.c_str(), compressed_path.c_str());

    // 压缩
    std::vector<std::string> input_files = {bag_path};
    auto ret = FileCompress::CompressFiles(input_files, compressed_path, compressionFormat);

    if (ret == FileCompress::ErrorCode::Success) {
        // 删除原始文件
        // common::DeleteFiles(input_files);

        return true;
    } else {
        AD_ERROR(DataCollectionExecutor, "Compress failed");
        return false;
    }
}

bool DataCollectionExecutor::running() const {
    // 检查节点是否还在运行
    return node_ && rclcpp::ok();
}


bool DataCollectionExecutor::checkDiskSpace() const {
    if (!diskSpaceChecker) {
        AD_ERROR(DataCollectionExecutor, "DiskSpaceChecker is null");
        return false;
    }

    auto appconfig = AppConfig::getInstance().GetConfig();
    unsigned long long total, freeSpaceBytes;
    if (!diskSpaceChecker->getDiskSpace(data_path_, total, freeSpaceBytes)) {
        AD_ERROR(DataCollectionExecutor, "Failed to get disk space");
        return false;
    }
    const uint64_t freeSpaceMb = freeSpaceBytes / (1024 * 1024);
    return freeSpaceMb >= static_cast<uint64_t>(appconfig.dataStorage.requriedSpaceMb);
}

// ========== 冷却状态检查 ==========

bool DataCollectionExecutor::isInCooldown() const {
    if (!strategy_) {
        return false;
    }

    uint64_t cooldownDurationSec = strategy_->mode.cacheMode.cooldownDurationSec;
    if (cooldownDurationSec == 0) {
        return false;
    }

    auto time_since_last_finish = common::GetCurrentTimestamp() - last_trigger_finish_time;
    uint64_t required_cooldown_us = cooldownDurationSec * 1000000ULL;

    if (time_since_last_finish < required_cooldown_us) {
        return true;
    }

    return false;
}

// ========== 配置热加载实现 ==========

bool DataCollectionExecutor::reloadConfig(const std::string& json_config_path) {
    AD_INFO(DataCollectionExecutor, "Reloading config from: %s", json_config_path.c_str());

    // 加载新的配置
    auto parser = std::make_shared<StrategyParser>();
    StrategyConfig new_config;
    if (!parser->LoadConfigFromFile(json_config_path, new_config)) {
        AD_ERROR(DataCollectionExecutor, "Failed to load new config from: %s", json_config_path.c_str());
        return false;
    }

    return reloadConfig(new_config);
}

bool DataCollectionExecutor::reloadConfig(const StrategyConfig& new_config) {
    // 检查是否正在重载
    if (is_reloading_.exchange(true)) {
        AD_WARN(DataCollectionExecutor, "Config reload already in progress, skipping");
        return false;
    }

    std::lock_guard<std::mutex> lock(config_mutex_);

    AD_DEBUG(DataCollectionExecutor, "Starting config reload...");
    AD_DEBUG(DataCollectionExecutor, "  New configId: %s", new_config.configId.c_str());
    AD_DEBUG(DataCollectionExecutor, "  Strategies: %zu", new_config.strategies.size());

    try {
        // 1. 如果正在录制，等待录制完成
        if (is_recording_.load()) {
            AD_INFO(DataCollectionExecutor, "Waiting for current recording to complete...");
            int wait_count = 0;
            while (is_recording_.load() && wait_count < 100) {  // 最多等待10秒
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                wait_count++;
            }
            if (is_recording_.load()) {
                AD_ERROR(DataCollectionExecutor, "Timeout waiting for recording to complete");
                is_reloading_.store(false);
                return false;
            }
        }

        // 2. 获取新配置中第一个启用的策略
        std::shared_ptr<Strategy> new_strategy;
        for (const auto& s : new_config.strategies) {
            if (s.trigger.enabled) {
                new_strategy = std::make_shared<Strategy>(s);
                break;
            }
        }

        if (!new_strategy) {
            AD_ERROR(DataCollectionExecutor, "No enabled strategy found in new config");
            is_reloading_.store(false);
            return false;
        }

        // 3. 停止并清理旧的 ChannelManager
        // 注意：我们需要先移除 observer，然后销毁 channel_manager_
        if (channel_manager_) {
            if (compliance_filter_) {
                channel_manager_->RemoveObserver(compliance_filter_);
            } else if (bag_recorder_) {
                channel_manager_->RemoveObserver(bag_recorder_);
            }
            auto trigger = trigger_manager_ ? trigger_manager_->getTrigger() : nullptr;
            if (trigger) {
                channel_manager_->RemoveObserver(trigger);
            }
        }
        channel_manager_.reset();
        compliance_filter_.reset();

        // 4. 更新配置
        strategy_config_ = new_config;
        strategy_ = new_strategy;

        // 5. 重新初始化 TriggerManager
        if (trigger_manager_) {
            if (!trigger_manager_->initialize(strategy_config_)) {
                AD_WARN(DataCollectionExecutor, "Failed to re-initialize TriggerManager, continuing without trigger");
            }
        }

        // 6. 更新 Ros2BagRecorder 的配置
        if (bag_recorder_) {
            bag_recorder_->setStrategy(strategy_);
            bag_recorder_->setCacheMode(strategy_->mode.cacheMode);

            // 重新初始化环形缓冲区
            AD_INFO(DataCollectionExecutor, "Re-initializing ring buffers...");
            if (!bag_recorder_->InitRingBuffers()) {
                AD_ERROR(DataCollectionExecutor, "Failed to re-initialize ring buffers");
                is_reloading_.store(false);
                return false;
            }

            AD_INFO(DataCollectionExecutor, "Ring buffers re-initialized for %zu topics",
                    strategy_->cyclone.channels.size());
        }

        // 7. 重新创建并初始化 ChannelManager
        channel_manager_ = std::make_shared<ChannelManager>();
        if (!channel_manager_->Init(node_, strategy_config_)) {
            AD_ERROR(DataCollectionExecutor, "Failed to re-initialize ChannelManager");
            channel_manager_.reset();
            is_reloading_.store(false);
            return false;
        }

        // 重新注册 observer (with compliance filter if masking enabled)
        if (bag_recorder_) {
            if (strategy_->enableMasking) {
                auto cc = compliance::ComplianceConfig::fromStrategy(*strategy_);
                compliance_filter_ = std::make_shared<compliance::ComplianceFilter>(node_, cc);
                compliance_filter_->setDownstream(bag_recorder_);
                channel_manager_->AddObserver(compliance_filter_);
            } else {
                channel_manager_->AddObserver(bag_recorder_);
            }
        }
        auto trigger = trigger_manager_ ? trigger_manager_->getTrigger() : nullptr;
        if (trigger) {
            channel_manager_->AddObserver(trigger);
            AD_INFO(DataCollectionExecutor, "Re-added Trigger as observer");
        }

        AD_INFO(DataCollectionExecutor, "Config reload completed successfully");
        AD_INFO(DataCollectionExecutor, "  New business type: %s", strategy_->businessType.c_str());
        AD_INFO(DataCollectionExecutor, "  New ring buffer: forward=%ds, backward=%ds",
                static_cast<int>(strategy_->mode.cacheMode.forwardCaptureDurationSec),
                static_cast<int>(strategy_->mode.cacheMode.backwardCaptureDurationSec));

        is_reloading_.store(false);
        return true;

    } catch (const std::exception& e) {
        AD_ERROR(DataCollectionExecutor, "Exception during config reload: %s", e.what());
        is_reloading_.store(false);
        return false;
    }
}

void DataCollectionExecutor::handleTriggerService(
    const std::shared_ptr<aurora_edge_runtime::srv::TriggerRecording::Request> request,
    std::shared_ptr<aurora_edge_runtime::srv::TriggerRecording::Response> response) {

    AD_INFO(DataCollectionExecutor, "Service trigger received: %s at (%.2f, %.2f)",
            request->trigger_id.c_str(), request->pos.x, request->pos.y);

    // Check cooldown first
    if (isInCooldown()) {
        response->success = false;
        response->message = "Recording in cooldown period";
        response->bag_path = "";
        response->cooldown_remaining = 5000;  // TODO: Calculate actual remaining time
        AD_WARN(DataCollectionExecutor, "Trigger rejected: cooldown period");
        return;
    }

    // Check disk space
    if (!checkDiskSpace()) {
        response->success = false;
        response->message = "Insufficient disk space";
        response->bag_path = "";
        response->cooldown_remaining = 0;
        AD_ERROR(DataCollectionExecutor, "Trigger rejected: insufficient disk space");
        return;
    }

    // Generate bag file path
    std::string bag_path = data_path_ + "/" + generateBagFilename(
        request->trigger_id,
        request->business_type
    );

    // Update cooldown timestamp immediately to prevent rapid re-triggering
    last_trigger_finish_time = common::GetCurrentTimestamp();
    total_recordings_++;

    // Enqueue recording + compress to background worker (non-blocking)
    // This moves the 5-second TriggerRecord wait off the executor thread
    {
        // Snapshot forward ring buffers NOW (on executor thread) to capture
        // data at the exact trigger time. If we defer this to the background
        // worker, the ring buffer will have rotated past trigger_timestamp
        // while waiting for the previous recording's 5s backward capture.
        RecordTask task;
        task.trigger_timestamp = request->trigger_timestamp;
        task.bag_path = bag_path;
        task.trigger_id = request->trigger_id;
        task.business_type = request->business_type;
        task.trigger_x = request->pos.x;
        task.trigger_y = request->pos.y;
        bag_recorder_->snapshotForwardBuffers(request->trigger_timestamp, task.saved_forward_buffers);

        std::lock_guard<std::mutex> lock(record_queue_mutex_);
        record_queue_.push(std::move(task));
    }
    record_cv_.notify_one();

    // Return success immediately (recording happens asynchronously)
    response->success = true;
    response->message = "Recording triggered successfully";
    response->bag_path = bag_path;
    response->cooldown_remaining = 0;

    AD_INFO(DataCollectionExecutor, "Recording enqueued: %s", bag_path.c_str());
}

void DataCollectionExecutor::recordWorkerLoop() {
    while (record_worker_running_.load()) {
        RecordTask task;
        {
            std::unique_lock<std::mutex> lock(record_queue_mutex_);
            record_cv_.wait(lock, [this] {
                return !record_worker_running_.load() || !record_queue_.empty();
            });
            if (!record_worker_running_.load() && record_queue_.empty()) break;
            if (record_queue_.empty()) continue;
            task = std::move(record_queue_.front());
            record_queue_.pop();
        }

        if (!task.bag_path.empty()) {
            // Step 1: TriggerRecord (includes 5s backward capture wait + ring buffer write)
            AD_INFO(DataCollectionExecutor, "Background recording: %s", task.bag_path.c_str());
            bag_recorder_->TriggerRecord(task.trigger_timestamp, task.bag_path,
                                         std::move(task.saved_forward_buffers));

            // Step 2: Generate metadata manifest
            {
                auto bag_info = bag_recorder_->GetBagInfo();
                bool masking_on = strategy_ ? strategy_->enableMasking : false;
                auto masking_cfg = strategy_ ? strategy_->maskingConfig : MaskingConfig{};
                auto channels = strategy_ ? strategy_->cyclone.channels : std::vector<Channel>{};
                auto manifest = compliance::MetadataManifestGenerator::buildManifest(
                    task.bag_path, bag_info, task.trigger_timestamp,
                    task.trigger_id, task.business_type,
                    masking_cfg, masking_on,
                    task.trigger_x, task.trigger_y, channels);
                compliance::MetadataManifestGenerator::generate(task.bag_path, manifest);
            }

            // Step 3: Compress the bag file
            AD_INFO(DataCollectionExecutor, "Background compressing: %s", task.bag_path.c_str());
            if (!compress(task.bag_path)) {
                AD_ERROR(DataCollectionExecutor, "Background compress failed: %s", task.bag_path.c_str());
            }
        }
    }

    // Drain remaining items on shutdown
    std::lock_guard<std::mutex> lock(record_queue_mutex_);
    while (!record_queue_.empty()) {
        auto task = std::move(record_queue_.front());
        record_queue_.pop();
        if (!task.bag_path.empty()) {
            bag_recorder_->TriggerRecord(task.trigger_timestamp, task.bag_path,
                                         std::move(task.saved_forward_buffers));
            compress(task.bag_path);
        }
    }
}

} // namespace aurora
