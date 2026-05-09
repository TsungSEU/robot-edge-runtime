//
// Created by Tsung Xu on 2026/4/2.
// Refactored on 2026/4/24 — decomposed God Class into thin coordinator.
// Copyright (c) 2026 TsungX. All rights reserved.
//

#ifndef DATA_COLLECTION_PLANNER_H
#define DATA_COLLECTION_PLANNER_H

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <chrono>

#include <rclcpp/rclcpp.hpp>

// Planning
#include "rl_planning_infer/core/i_planner.h"
#include "rl_planning_infer/core/planner_factory.h"
#include "rl_planning_infer/core/humanoid_planner.h"
#include "rl_planning_infer/core/path_visualizer.h"

// Data collection
#include "data_collection/data_manager.h"
#include "data_collection/data_collection_executor.h"
#include "data_collection/collection_feedback.h"
#include "data_collection/robot_position_tracker.h"
#include "data_collection/robot_controller.h"
#include "data_collection/uploader/aws_data_uploader.h"
#include "data_collection/common/config_watcher.h"
#include "data_collection/common/runtime_config.h"
#include "data_collection/common/types.h"

// Services
#include "aurora_edge_runtime/srv/trigger_recording.hpp"

namespace aurora {

using collector::DataPoint;
using collector::MissionArea;

/**
 * @brief 数据采集规划器 — 薄协调器
 *
 * 重构后的 DataCollectionPlanner 仅负责协调子系统：
 * - IPlanner:          路径规划（通过工厂创建，多态调用）
 * - RobotPositionTracker: 里程计跟踪和 waypoint 等待
 * - RobotController:   机器人控制（统一 path_tracking / velocity_cmd）
 * - DataCollectionExecutor: 数据采集执行
 * - CollectionFeedback:   采集反馈
 * - PathVisualizer:     可视化
 * - AwsDataUploader:    云端上传
 *
 * 不再包含业务逻辑，所有职责委托给子系统。
 */
class DataCollectionPlanner : public rclcpp::Node {
public:
    DataCollectionPlanner(const config::RuntimeConfig& config);

    ~DataCollectionPlanner() override;

    /**
     * @brief 初始化所有子系统
     */
    bool initialize();

    /**
     * @brief 设置任务区域
     */
    void setMissionArea(const MissionArea& area);

    /**
     * @brief 规划数据采集任务
     */
    std::vector<Point> planMission();

    /**
     * @brief 执行数据采集任务
     */
    void executeMission(const std::vector<Point>& path);

    /**
     * @brief 上传采集数据到云端
     */
    void uploadCollectedData();

    /**
     * @brief 报告覆盖率指标
     */
    void reportCoverageMetrics();

    /**
     * @brief 获取学习统计
     */
    struct MissionStats {
        double average_reward = 0.0;
        double min_reward = 0.0;
        double max_reward = 0.0;
        int count = 0;
    };
    MissionStats getStats() const;

    // ===== 便捷接口 =====

    void setVisualizationEnabled(bool enabled);
    void setVisualizationFrame(const std::string& frame_id);

    /**
     * @brief 获取规划器（供需要 Humanoid 专用接口的场景使用）
     */
    planner::IPlanner* getPlanner() { return planner_.get(); }

    /**
     * @brief 获取位置跟踪器（供 main.cpp 服务调用使用）
     */
    collector::RobotPositionTracker& getPositionTracker() { return *position_tracker_; }

    /**
     * @brief 获取运行时配置
     */
    const config::RuntimeConfig& getConfig() const { return config_; }

private:
    // ===== 子系统（组合，非 God Object）=====
    std::unique_ptr<planner::IPlanner> planner_;
    std::unique_ptr<collector::RobotPositionTracker> position_tracker_;
    std::unique_ptr<collector::RobotController> controller_;
    std::unique_ptr<collector::DataCollectionExecutor> executor_;
    std::unique_ptr<collector::DataManager> data_manager_;
    std::unique_ptr<collector::CollectionFeedback> feedback_;
    std::unique_ptr<planner::PathVisualizer> visualizer_;
    std::unique_ptr<collector::AwsDataUploader> uploader_;
    std::unique_ptr<collector::ConfigWatcher> config_watcher_;

    // ===== 状态 =====
    config::RuntimeConfig config_;
    MissionArea mission_area_;
    std::string strategy_config_path_;

    // ROS2 service client
    rclcpp::Client<aurora_edge_runtime::srv::TriggerRecording>::SharedPtr trigger_client_;

    // Humanoid 状态跟踪（用于 selectAction 等）
    planner::HumanoidStateInfo humanoid_state_;
    planner::HumanoidAction last_action_;

    // ===== 内部方法 =====
    std::optional<DataPoint> tryCollect(const Point& waypoint, size_t index,
                                         const Point& last_collected_pos,
                                         const std::chrono::steady_clock::time_point& last_time);
    void updateReachabilityForPath(const std::vector<Point>& path,
                                    const std::vector<DataPoint>& collected_data);
    collector::CollectionResult createCollectionResult(
        const std::vector<Point>& path,
        const std::vector<DataPoint>& collected_data,
        double execution_time);
    void onConfigChanged(const std::string& file_path);
    void onReplanTriggered(const collector::EnvironmentChange& change);
    void setupConfigWatcher();

    static double calculateDistance(const Point& p1, const Point& p2);
};

} // namespace aurora

#endif // DATA_COLLECTION_PLANNER_H
