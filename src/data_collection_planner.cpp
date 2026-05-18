//
// Created by Tsung Xu on 2026/4/2.
// Refactored on 2026/4/24 — decomposed God Class into thin coordinator.
// Copyright (c) 2026 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
//

#include "data_collection_planner.h"

#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <cmath>
#include <limits>
#include <ctime>

#include "common/log/logger.h"
#include "common/ros2/qos_profiles.h"

namespace aurora {

static std::string kStrategyConfigPath = "/config/robot_data_collection.json";

// ============================================================================
// Construction / Destruction
// ============================================================================

DataCollectionPlanner::DataCollectionPlanner(const config::RuntimeConfig& config)
    : rclcpp::Node("aer")
    , config_(config) {

    // 创建规划器（通过工厂，消除 if-else 模式切换）
    planner::PlannerCreateConfig pcfg;
    pcfg.mode = (config_.action_type == "velocity_cmd")
              ? planner::PlannerMode::HUMANOID   // 默认
              : planner::PlannerMode::HUMANOID;   // TODO: 从 config 获取实际 mode
    pcfg.model_path = config.model_path;
    pcfg.config_path = config.config_path;

    planner_ = planner::PlannerFactory::getInstance().create(pcfg);

    // 创建位置跟踪器（替代内联的里程计逻辑）
    position_tracker_ = std::make_unique<collector::RobotPositionTracker>(this);

    // 创建反馈系统（使用配置的上传阈值）
    feedback_ = std::make_unique<collector::CollectionFeedback>(config_.collection.upload_threshold);
    feedback_->setReplanCallback([this](const collector::EnvironmentChange& change) {
        onReplanTriggered(change);
    });
    feedback_->setUploadCallback([](const std::vector<collector::ExperienceMetadata>& batch) {
        AD_INFO(DataCollectionPlanner, "Uploading %zu experiences to cloud S3", batch.size());
        // TODO: metadata serialization and upload
    });

    // 创建上传器
    uploader_ = std::make_unique<collector::AwsDataUploader>();

    // 创建策略解析器（仍由 executor 管理）
    AD_INFO(DataCollectionPlanner, "DataCollectionPlanner constructed (mode: %s)",
            planner_ ? planner::plannerModeToString(pcfg.mode).c_str() : "null");
}

DataCollectionPlanner::~DataCollectionPlanner() {
    visualizer_.reset();
    config_watcher_.reset();
}

// ============================================================================
// Initialization
// ============================================================================

bool DataCollectionPlanner::initialize() {
    strategy_config_path_ = std::string(collector::getInstallRootPath()) + kStrategyConfigPath;

    // 1. 初始化规划器
    if (!planner_ || !planner_->initialize()) {
        AD_ERROR(DataCollectionPlanner, "Failed to initialize planner");
        return false;
    }

    // 2. 初始化数据采集执行器
    executor_ = std::make_unique<collector::DataCollectionExecutor>(shared_from_this());
    executor_->setDataCallback([this](const std::vector<DataPoint>& data) {
        if (data_manager_) {
            data_manager_->addDataPoints(data);
        }
    });
    if (!executor_->initialize(strategy_config_path_)) {
        AD_ERROR(DataCollectionPlanner, "Failed to initialize data collection executor");
        return false;
    }

    // 3. 连接位置跟踪器回调到 TriggerManager
    position_tracker_->setPositionCallback(
        [this](const Point& pos) {
            if (executor_ && executor_->getTriggerManager()) {
                executor_->getTriggerManager()->updateRobotPosition(pos);
            }
        });

    // 4. 初始化上传器
    const auto& upload_config = collector::AppConfig::getInstance().GetConfig().dataUpload;
    if (!uploader_->Init(upload_config)) {
        AD_ERROR(DataCollectionPlanner, "Failed to initialize data uploader");
        return false;
    }

    // 5. 初始化可视化（使用配置的轨迹发布间隔）
    visualizer_ = std::make_unique<planner::PathVisualizer>(shared_from_this(), config_.trail_publish_interval_sec);
    visualizer_->setFrameId(config_.visualization_frame);

    // 6. 初始化机器人控制器（替代 3 个 execute 方法）
    controller_ = std::make_unique<collector::RobotController>(
        *position_tracker_, this, config_.velocity_control);

    // 7. 配置文件监控
    setupConfigWatcher();

    // 8. TriggerRecording 服务客户端
    trigger_client_ = this->create_client<aurora_edge_runtime::srv::TriggerRecording>(
        "/robot/trigger");

    AD_INFO(DataCollectionPlanner, "Data Collection Planner initialized successfully");
    return true;
}

// ============================================================================
// Mission Area
// ============================================================================

void DataCollectionPlanner::setMissionArea(const MissionArea& area) {
    mission_area_ = area;

    // 从配置读取网格分辨率
    double resolution = config_.collection.grid_resolution;
    int map_size = static_cast<int>(area.radius * 2.0 / resolution);
    data_manager_ = std::make_unique<collector::DataManager>(map_size, map_size, resolution);

    if (planner_) {
        planner_->setGoalPosition(area.center);
    }

    AD_INFO(DataCollectionPlanner, "Mission area set: center=(%.2f, %.2f), radius=%.1f, grid_resolution=%.2f, map_size=%dx%d",
            area.center.x, area.center.y, area.radius, resolution, map_size, map_size);
}

// ============================================================================
// Planning — 委托给 IPlanner，无 if-else
// ============================================================================

std::vector<Point> DataCollectionPlanner::planMission() {
    AD_INFO(DataCollectionPlanner, "Planning data collection mission");

    if (!planner_) {
        AD_ERROR(DataCollectionPlanner, "No planner available");
        return {};
    }

    // 使用实际 odom 位置作为起点（非内部状态）
    Point start = position_tracker_->getCurrentPosition();
    Point goal = mission_area_.center;

    // 当 robot 在目标附近时，偏移 goal 到任务区域边缘
    double dist_to_goal = calculateDistance(start, goal);
    if (dist_to_goal < mission_area_.radius * 0.5) {
        goal.x = mission_area_.center.x + mission_area_.radius * 0.8;
        goal.y = mission_area_.center.y + mission_area_.radius * 0.8;
        AD_INFO(DataCollectionPlanner, "Goal offset to (%.1f, %.1f) for coverage",
                goal.x, goal.y);
    }

    // 同步 humanoid 状态到实际位置
    humanoid_state_.x = start.x;
    humanoid_state_.y = start.y;

    // 通过 IPlanner 规划（使用 HumanoidPlanner 的 planWithState）
    auto* hp = dynamic_cast<planner::HumanoidPlanner*>(planner_.get());
    std::vector<Point> result;
    if (hp) {
        auto trajectory = hp->planWithState(start, goal, humanoid_state_);
        result.reserve(trajectory.path.size());
        for (const auto& pt : trajectory.path) {
            result.push_back(pt);
        }
    } else {
        result = planner_->planMission(mission_area_);
    }

    if (!result.empty()) {
        if (visualizer_ && config_.visualization_enabled) {
            visualizer_->publishPlanningPath(result);
            visualizer_->publishTrajectory(result);
            if (result.size() >= 2) {
                visualizer_->publishStartGoalMarkers(result.front(), result.back());
            }
        }
    }

    AD_INFO(DataCollectionPlanner, "Mission planned with %zu waypoints", result.size());
    return result;
}

// ============================================================================
// Execution — 统一委托给 RobotController
// ============================================================================

void DataCollectionPlanner::executeMission(const std::vector<Point>& path) {
    if (path.empty() || !executor_ || !data_manager_ || !feedback_) {
        AD_ERROR(DataCollectionPlanner, "Cannot execute: path=%zu, executor=%d, data=%d, feedback=%d",
                path.size(), !!executor_, !!data_manager_, !!feedback_);
        return;
    }

    AD_INFO(DataCollectionPlanner, "Executing mission with %zu waypoints", path.size());

    // 共享的采集状态（跨 waypoint 追踪）
    Point last_collected_pos(9999.0, 9999.0);
    auto last_collect_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);

    // 采集判断回调（被 RobotController 在每个 waypoint 调用）
    auto checker = [this, &last_collected_pos, &last_collect_time](
        const Point& wp, size_t idx) -> std::optional<DataPoint> {
        auto result = tryCollect(wp, idx, last_collected_pos, last_collect_time);
        if (result.has_value()) {
            last_collected_pos = wp;
            last_collect_time = std::chrono::steady_clock::now();
        }
        return result;
    };

    // waypoint 到达回调（可视化更新）
    std::vector<Point> collected_positions;
    auto on_reached = [this, &collected_positions](const Point& wp, size_t idx) {
        collected_positions.push_back(wp);
        if (visualizer_ && config_.visualization_enabled) {
            visualizer_->updateRobotTrail(wp);
            visualizer_->publishCollectionPoints(collected_positions);
        }
    };

    // 清除上一轮可视化
    if (visualizer_) {
        visualizer_->clearTrailAndCollectionPoints();
    }

    // 根据配置选择执行模式
    collector::RobotController::ExecutionResult exec_result;
    if (config_.action_type == "velocity_cmd") {
        exec_result = controller_->executeVelocityCommands(path, checker, on_reached);
    } else {
        exec_result = controller_->executePathTracking(
            path, checker, on_reached, config_.collection.waypoint_wait_timeout);
    }

    // 更新规划器位置
    if (planner_ && !path.empty()) {
        planner_->setCurrentPosition(path.back());
    }

    // 更新 CostMap 和反馈
    if (!exec_result.collected_data.empty()) {
        planner::CostMap* costmap = planner_ ? planner_->getCostMap() : nullptr;
        if (costmap && data_manager_->hasDataPoints()) {
            data_manager_->updateCostmapWithStatistics(*costmap);
            data_manager_->updateCoverageMetrics(*costmap);
            updateReachabilityForPath(path, exec_result.collected_data);
        }

        auto collection_result = createCollectionResult(
            path, exec_result.collected_data, exec_result.execution_time);
        feedback_->submitCollectionResult(collection_result);
    }

    // 发布最终可视化
    if (visualizer_ && config_.visualization_enabled) {
        visualizer_->publishCollectedPath(path);
        if (!collected_positions.empty()) {
            visualizer_->publishCollectionPoints(collected_positions);
        }
    }

    AD_INFO(DataCollectionPlanner, "Mission executed: %zu/%zu waypoints, %zu points collected",
            exec_result.waypoints_reached, path.size(), exec_result.collected_data.size());
}

// ============================================================================
// Upload & Metrics
// ============================================================================

void DataCollectionPlanner::uploadCollectedData() {
    if (uploader_->Start()) {
        AD_INFO(DataCollectionPlanner, "Data uploaded successfully");
        if (data_manager_) {
            data_manager_->clear();
        }
    } else {
        AD_ERROR(DataCollectionPlanner, "Failed to upload data");
    }
}

void DataCollectionPlanner::reportCoverageMetrics() {
    planner::CostMap* costmap = planner_ ? planner_->getCostMap() : nullptr;
    if (costmap && data_manager_) {
        data_manager_->updateCoverageMetrics(*costmap);
        const auto& coverage = data_manager_->getCoverageMetrics();
        AD_INFO(DataCollectionPlanner, "Coverage: %.2f%% (%d/%d cells)",
                coverage.getCoverageRatio() * 100.0,
                coverage.getVisitedCells(), coverage.getTotalCells());
    }
}

DataCollectionPlanner::MissionStats DataCollectionPlanner::getStats() const {
    MissionStats stats;
    if (feedback_) {
        auto reward_stats = feedback_->getRewardStats();
        stats.average_reward = reward_stats.average_reward;
        stats.min_reward = reward_stats.min_reward;
        stats.max_reward = reward_stats.max_reward;
        stats.count = reward_stats.count;
    }
    return stats;
}

// ============================================================================
// Visualization
// ============================================================================

void DataCollectionPlanner::setVisualizationEnabled(bool enabled) {
    config_.visualization_enabled = enabled;
    if (visualizer_ && !enabled) {
        visualizer_->clearMarkers();
    }
}

void DataCollectionPlanner::setVisualizationFrame(const std::string& frame_id) {
    config_.visualization_frame = frame_id;
    if (visualizer_) {
        visualizer_->setFrameId(frame_id);
    }
}

// ============================================================================
// Internal helpers
// ============================================================================

std::optional<DataPoint> DataCollectionPlanner::tryCollect(
    const Point& waypoint, size_t index,
    const Point& last_collected_pos,
    const std::chrono::steady_clock::time_point& last_time) {

    double distance = calculateDistance(waypoint, last_collected_pos);
    auto now = std::chrono::steady_clock::now();
    double time_since = std::chrono::duration<double>(now - last_time).count();

    if (distance < config_.collection.min_step_distance ||
        time_since < config_.collection.min_collection_interval) {
        return std::nullopt;
    }

    if (executor_->isInCooldown() || !executor_->shouldCollectAt(waypoint)) {
        return std::nullopt;
    }

    // 使用实际机器人位置
    Point actual_pos = position_tracker_->getCurrentPosition();

    // 通过 ROS2 服务触发录制
    if (!trigger_client_ || !trigger_client_->service_is_ready()) {
        AD_DEBUG(DataCollectionPlanner, "Trigger service not available at waypoint %zu", index);
        return std::nullopt;
    }

    auto request = std::make_shared<aurora_edge_runtime::srv::TriggerRecording::Request>();
    request->business_type = "humanoid_gait";
    request->trigger_id = "waypoint_" + std::to_string(index);
    request->trigger_timestamp = common::GetCurrentTimestamp();
    request->trigger_desc = "";
    request->pos.x = actual_pos.x;
    request->pos.y = actual_pos.y;
    request->pos.z = 0.0;

    auto future = trigger_client_->async_send_request(request);
    if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
        AD_ERROR(DataCollectionPlanner, "Service call timeout at waypoint %zu", index);
        return std::nullopt;
    }

    auto response = future.get();
    if (!response->success) {
        return std::nullopt;
    }

    std::string sensor_data = "gait_data_" + std::to_string(actual_pos.x) +
                              "_" + std::to_string(actual_pos.y);
    return DataPoint(actual_pos, sensor_data, static_cast<double>(std::time(nullptr)));
}

void DataCollectionPlanner::updateReachabilityForPath(
    const std::vector<Point>& path,
    const std::vector<DataPoint>& collected_data) {

    planner::CostMap* costmap = planner_ ? planner_->getCostMap() : nullptr;
    if (!costmap || path.empty()) return;

    size_t collected_idx = 0;
    for (size_t i = 0; i < path.size() && collected_idx < collected_data.size(); ++i) {
        const Point& planned = path[i];
        const Point& collected = collected_data[collected_idx].position;
        double dist = calculateDistance(planned, collected);

        if (dist <= config_.collection.position_tolerance) {
            double stability = std::max(0.1, 1.0 - (dist / config_.collection.position_tolerance));
            costmap->updateReachability(planned, collected, true, stability);
            collected_idx++;
        } else {
            costmap->updateReachability(planned, planned, false, 0.1);
        }
    }
}

collector::CollectionResult DataCollectionPlanner::createCollectionResult(
    const std::vector<Point>& path,
    const std::vector<DataPoint>& collected_data,
    double execution_time) {

    collector::CollectionResult result;
    result.planned_path = path;
    result.planned_goal = mission_area_.center;
    result.collected_data = collected_data;
    result.attempted_points = path.size();
    result.successful_points = collected_data.size();
    result.execution_time = execution_time;
    result.timestamp = common::GetCurrentTimestamp();

    for (const auto& dp : collected_data) {
        result.executed_path.push_back(dp.position);
    }

    if (!collected_data.empty()) {
        size_t non_empty = 0;
        for (const auto& dp : collected_data) {
            if (!dp.sensor_data.empty()) non_empty++;
        }
        result.data_quality_score = static_cast<double>(non_empty) / collected_data.size();

        if (data_manager_) {
            const auto& coverage = data_manager_->getCoverageMetrics();
            result.novelty_score = coverage.getCoverageRatio();
            result.data_value = result.novelty_score * 2.0;
        }
    }

    result.actual_cost = 0.0;
    for (size_t i = 1; i < result.executed_path.size(); ++i) {
        result.actual_cost += calculateDistance(result.executed_path[i], result.executed_path[i-1]);
    }

    return result;
}

void DataCollectionPlanner::onReplanTriggered(const collector::EnvironmentChange& change) {
    AD_WARN(DataCollectionPlanner, "Replan triggered: type=%d, severity=%.2f",
            static_cast<int>(change.type), change.severity);

    if (change.type == collector::EnvironmentChange::NEW_OBSTACLE) {
        planner::CostMap* costmap = planner_ ? planner_->getCostMap() : nullptr;
        if (costmap) {
            costmap->addObstacle(change.location, 0.5);
        }
    }
}

void DataCollectionPlanner::onConfigChanged(const std::string& file_path) {
    AD_INFO(DataCollectionPlanner, "Config changed: %s", file_path.c_str());
    if (executor_ && executor_->reloadConfig(file_path)) {
        AD_INFO(DataCollectionPlanner, "Config reloaded successfully");
    }
}

void DataCollectionPlanner::setupConfigWatcher() {
    if (strategy_config_path_.empty()) return;

    config_watcher_ = std::make_unique<collector::ConfigWatcher>(strategy_config_path_, 500);
    config_watcher_->setChangeCallback([this](const std::string& fp) { onConfigChanged(fp); });
    config_watcher_->start();
}

double DataCollectionPlanner::calculateDistance(const Point& p1, const Point& p2) {
    double dx = p1.x - p2.x;
    double dy = p1.y - p2.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace aurora
