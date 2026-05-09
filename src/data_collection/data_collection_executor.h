// data_collection_executor.h
#ifndef DATA_COLLECTION_EXECUTOR_H
#define DATA_COLLECTION_EXECUTOR_H

#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <atomic>
#include <string>
#include <thread>
#include <queue>
#include <unordered_map>
#include <condition_variable>
#include "aurora_edge_runtime/srv/trigger_recording.hpp"
#include "common/utils/utils.h"
#include "common/types.h"  // For DataPoint
#include "trigger/trigger_manager.h"
#include "trigger/strategy/strategy_config.h"
#include "recorder/ros2bag_recorder.h"
#include "recorder/diskspace_checker.hpp"
#include "channel/channel_manager.h"
#include "compliance/compliance_filter.h"

namespace aurora::collector {

/**
 * @brief DataCollectionExecutor - 负责实际的数据采集执行
 *
 * 核心功能：触发 -> 环形存储 -> 数据落盘
 *
 * 职责：
 * 1. 从 JSON 配置加载 topics 并初始化环形缓冲区
 * 2. 订阅 ROS2 topics 并持续记录到环形缓冲区
 * 3. 判断是否触发录制（基于 Trigger）
 * 4. 触发后将环形缓冲区数据落盘为 .db3 文件
 * 5. 压缩并上传到 AWS S3
 */
class DataCollectionExecutor {
public:
    using DataCallback = std::function<void(const std::vector<DataPoint>&)>;

    /**
     * @brief 构造函数
     * @param node ROS2 节点
     * @param trigger_manager 触发器管理模块
     */
    DataCollectionExecutor(std::shared_ptr<rclcpp::Node> node);

    ~DataCollectionExecutor();

    // ========== 初始化接口 ==========

    /**
     * @brief 从 JSON 配置初始化录制系统
     * @param json_config_path JSON 配置文件路径
     * @return true 如果初始化成功
     *
     * 此方法会：
     * 1. 加载 JSON 配置文件
     * 2. 初始化环形缓冲区（从 JSON 读取 topics 和缓存时长）
     * 3. 创建 ChannelManager 并订阅所有 topics
     * 4. 将 Ros2BagRecorder 注册为 observer
     */
    bool initialize(const std::string& json_config_path);

    /**
     * @brief 初始化录制系统（使用已加载的配置）
     * @param strategy_config 策略配置
     * @return true 如果初始化成功
     */
    bool initialize(const StrategyConfig& strategy_config);

    // ========== 数据采集执行接口 ==========

    /**
     * @brief 沿路径执行数据采集（带录制）
     * @param path 路径点集合
     * @param output_bag_path 输出 bag 文件路径（可选）
     * @return 采集到的数据点集合
     *
     * 完整流程：
     * 1. 打开 bag 文件准备录制
     * 2. 遍历路径，执行采集（环形缓冲区持续记录）
     * 3. 关闭 bag 文件（数据落盘）
     * 4. 压缩为 tar.lz4
     */
    std::vector<DataPoint> executeAlongPath(const std::vector<Point>& path,
                                            const std::string& output_bag_path = "");

    /**
     * @brief 开始录制（手动触发模式）
     * @param output_bag_path 输出 bag 文件路径
     * @return true 如果成功开始录制
     */
    bool startRecording(const std::string& output_bag_path);

    /**
     * @brief 停止录制（手动触发模式）
     * @return true 如果成功停止录制
     */
    bool stopRecording();

    // ========== 判断接口 ==========

    /**
     * @brief 判断是否应该在给定点采集数据
     * @param point 待判断的点
     * @return true 如果应该采集，false 否则
     */
    bool shouldCollectAt(const Point& point);

    // ========== 状态查询接口 ==========

    /**
     * @brief 检查是否正在录制
     */
    bool isRecording() const { return is_recording_.load(); }

    /**
     * @brief 获取录制器统计信息
     */
    TBagInfo getRecordingStatistics() const;

    // ========== 回调接口 ==========

    /**
     * @brief 设置数据采集回调
     * @param callback 回调函数，接收新采集的数据点集合
     */
    void setDataCallback(DataCallback callback) { data_callback_ = callback; }

    // ========== 统计接口 ==========

    /**
     * @brief 获取采集统计信息
     */
    size_t getTotalCollected() const { return total_collected_; }
    size_t getTotalAttempts() const { return total_attempts_; }
    size_t getTotalRecordings() const { return total_recordings_; }

    /**
     * @brief 检查系统是否还在运行
     */
    bool running() const;

    bool checkDiskSpace() const;

    // ========== 配置热加载接口 ==========

    /**
     * @brief 从文件路径重新加载配置
     * @param json_config_path JSON 配置文件路径
     * @return true 如果重载成功
     *
     * 此方法会：
     * 1. 读取新的配置文件
     * 2. 如果当前正在录制，等待录制完成
     * 3. 重新初始化 ChannelManager（topic订阅）
     * 4. 更新 Ring Buffer 配置
     */
    bool reloadConfig(const std::string& json_config_path);

    /**
     * @brief 重新加载配置（使用已加载的配置对象）
     * @param strategy_config 新的策略配置
     * @return true 如果重载成功
     */
    bool reloadConfig(const StrategyConfig& strategy_config);

    /**
     * @brief 获取当前配置
     */
    const StrategyConfig& getCurrentConfig() const { return strategy_config_; }

    /**
     * @brief 检查配置是否有效
     */
    bool isConfigValid() const { return is_initialized_.load(); }

    /**
     * @brief 检查是否处于冷却期间
     * @return true 如果在冷却期间，false 如果可以触发采集
     */
    bool isInCooldown() const;

    /**
     * @brief 获取TriggerManager（用于更新机器人位置）
     */
    std::shared_ptr<TriggerManager> getTriggerManager() const { return trigger_manager_; }

private:
    /**
     * @brief 在指定点执行数据采集（内部实现）
     * @param point 采集点位置
     * @return 采集到的数据点（如果有）
     *
     * 注意：实际数据采集由 ChannelManager 持续进行
     * 此方法主要用于生成 DataPoint 元数据
     */
    std::optional<DataPoint> executeAtPoint(const Point& point);

    /**
     * @brief 生成 bag 文件名（遵循命名规范）
     * @param trigger_id 触发器ID
     * @param business_type 业务类型
     */
    std::string generateBagFilename(const std::string& trigger_id,
                                    const std::string& business_type);

    /**
     * @brief 触发数据回调
     * @param new_data 新采集的数据点集合
     */
    void triggerCallback(const std::vector<DataPoint>& new_data);

    /**
     * @brief 压缩并上传 bag 文件
     * @param bag_path bag 文件路径
     */
    bool compress(const std::string& bag_path);

private:
    // ROS2 节点
    std::shared_ptr<rclcpp::Node> node_;

    // 配置重载保护
    mutable std::mutex config_mutex_;
    std::atomic<bool> is_reloading_{false};

    // 录制组件
    std::shared_ptr<Ros2BagRecorder> bag_recorder_;
    std::shared_ptr<ChannelManager> channel_manager_;
    std::shared_ptr<TriggerManager> trigger_manager_;
    std::shared_ptr<Strategy> strategy_;
    std::shared_ptr<DiskSpaceChecker> diskSpaceChecker;
    std::shared_ptr<compliance::ComplianceFilter> compliance_filter_;

    // 配置
    StrategyConfig strategy_config_;
    std::string data_path_;
    uint64_t last_trigger_finish_time;

    // 状态
    std::atomic<bool> is_recording_{false};
    std::atomic<bool> is_initialized_{false};

    // 当前录制文件路径
    std::string current_bag_path_;

    // 回调函数
    DataCallback data_callback_;

    // 统计信息
    size_t total_collected_{0};
    size_t total_attempts_{0};
    size_t total_recordings_{0};

    // ROS2 service server for trigger requests (isolated callback group)
    rclcpp::CallbackGroup::SharedPtr trigger_callback_group_;
    rclcpp::Service<aurora_edge_runtime::srv::TriggerRecording>::SharedPtr trigger_service_;

    // Background recording + compress worker (moves TriggerRecord + compress off executor thread)
    struct RecordTask {
        uint64_t trigger_timestamp;
        std::string bag_path;
        std::string trigger_id;
        std::string business_type;
        double trigger_x = 0.0;
        double trigger_y = 0.0;
        // Forward data snapshot: extracted immediately in handleTriggerService
        // to avoid queue-delay-induced ring buffer drift
        std::unordered_map<std::string, std::vector<aurora::collector::TimestampedData>> saved_forward_buffers;
    };
    std::thread record_worker_thread_;
    std::queue<RecordTask> record_queue_;
    std::mutex record_queue_mutex_;
    std::condition_variable record_cv_;
    std::atomic<bool> record_worker_running_{true};
    void recordWorkerLoop();

    // Service callback handler
    void handleTriggerService(
        const std::shared_ptr<aurora_edge_runtime::srv::TriggerRecording::Request> request,
        std::shared_ptr<aurora_edge_runtime::srv::TriggerRecording::Response> response);
};

} // namespace aurora

#endif // DATA_COLLECTION_EXECUTOR_H
