// auto_planner.cpp - Autonomous driving mode planner implementation
#include "auto_planner.h"
#include <iostream>
#include <algorithm>

// 包含优化模块
#include "common/platform/platform_adapter.h"
#include "common/memory_pool.h"
#include "common/memory_monitor.h"
#include "common/performance_utils.h"

namespace aurora::planner {

AutoPlanner::AutoPlanner(const std::string& model_file, const std::string& config_file)
    : config_file_path_(config_file)
    , model_file_(model_file)
    , current_position_(Point(0.0, 0.0))
    , goal_position_(Point(0.0, 0.0))
    , use_ppo_(true) {
    // 路径信息已在 PlannerFactory 中打印，这里只打印简短消息
    AD_INFO(AutoPlanner, "Initializing (auto mode)");

    // 初始化平台适配器
    platform_adapter_ = std::make_unique<aurora::platform::PlatformAdapter>();
    platform_adapter_->printInfo();

    // 初始化内存监控器
    memory_monitor_ = std::make_unique<aurora::monitor::MemoryMonitor>(1024); // 1GB限制

    // Initialize with default values based on PRD: configurable 2D grid (default 60x60)
    int default_map_width = 60;
    int default_map_height = 60;
    double default_resolution = 1.0;

    costmap_ = std::make_unique<CostMap>(default_map_width, default_map_height, default_resolution);
    route_planner_ = std::make_unique<AutoRoutePlanner>();
    sampling_optimizer_ = std::make_unique<SamplingOptimizer>();
    semantic_map_ = std::make_unique<SemanticMap>(default_map_width, default_map_height, default_resolution);
    constraint_checker_ = std::make_unique<SemanticConstraintChecker>(*semantic_map_);
    semantic_filter_ = std::make_unique<SemanticFilter>();
    coverage_metric_ = std::make_unique<CoverageMetric>();

}

bool AutoPlanner::initialize() {
    // Load configuration
    if (!loadConfiguration(config_file_path_)) {
        AD_ERROR(AutoPlanner, "Failed to load configuration");
        return false;
    }

    // Load PPO weights if using PPO-based planning
    loadPPOWeights(model_file_);

    // 根据平台特性优化配置
    bool use_quantized = platform_adapter_->useQuantizedModel();
    int optimal_threads = platform_adapter_->getOptimalThreadCount();
    bool has_simd = platform_adapter_->hasSIMDSupport();

    AD_DEBUG(AutoPlanner, "Platform optimization: quantized=%d, threads=%d, simd=%d",
                use_quantized, optimal_threads, has_simd);

    // 更新costmap resolution
    auto sparse_it = planner_parameters_.find("sparse_threshold");
    auto exploration_it = planner_parameters_.find("exploration_bonus");
    auto redundancy_it = planner_parameters_.find("redundancy_penalty");

    if (sparse_it != planner_parameters_.end() && exploration_it != planner_parameters_.end() && redundancy_it != planner_parameters_.end()) {
        costmap_->setParameters(sparse_it->second, exploration_it->second, redundancy_it->second);
    }

    // Update route planner parameters
    if (sparse_it != planner_parameters_.end()) {
        route_planner_->setSparseThreshold(sparse_it->second);
    }
    if (exploration_it != planner_parameters_.end()) {
        route_planner_->setExplorationBonus(exploration_it->second);
    }
    if (redundancy_it != planner_parameters_.end()) {
        route_planner_->setRedundancyPenalty(redundancy_it->second);
    }

    // Update sampling optimizer parameters
    SamplingParams sampling_params;
    sampling_params.sparse_threshold = (sparse_it != planner_parameters_.end()) ? sparse_it->second : 0.2;
    sampling_params.exploration_weight = (exploration_it != planner_parameters_.end()) ? exploration_it->second : 1.0;
    sampling_params.redundancy_penalty = (redundancy_it != planner_parameters_.end()) ? redundancy_it->second : 5.0;

    auto efficiency_it = planner_parameters_.find("auto_sampling_efficiency_weight");
    sampling_params.efficiency_weight = (efficiency_it != planner_parameters_.end()) ? efficiency_it->second : 0.5;

    sampling_optimizer_->updateParameters(sampling_params);

    // Update coverage metric
    coverage_metric_ = std::make_unique<CoverageMetric>(sampling_params.sparse_threshold);

    // 更新PPO agent配置，加入优化参数
    if (route_planner_->getPPOAgent()) {
        // 传递优化参数
        std::map<std::string, double> optimized_params = planner_parameters_;

        // 添加优化参数
        optimized_params["auto_inference_use_quantized_model"] = use_quantized ? 1.0 : 0.0;
        optimized_params["auto_inference_enable_simd"] = has_simd ? 1.0 : 0.0;
        optimized_params["auto_inference_enable_memory_pool"] = 1.0;  // 启用内存池
        optimized_params["auto_inference_num_inference_threads"] = static_cast<double>(optimal_threads);

        route_planner_->getPPOAgent()->updateConfigFromParameters(optimized_params);
    }

    AD_INFO(AutoPlanner, "Initialized successfully");
    return true;
}

bool AutoPlanner::loadConfiguration(const std::string& config_file) {
    AD_DEBUG(AutoPlanner, "Loading config: %s", config_file.c_str());

    // 如果提供了新的配置文件路径，更新当前路径
    if (!config_file.empty()) {
        config_file_path_ = config_file;
    }

    // 使用模式化加载函数
    if (!PlannerUtils::loadModeSpecificParameters(config_file_path_, "auto", planner_parameters_)) {
        AD_ERROR(AutoPlanner, "Failed to load parameters from YAML file");
        return false;
    }

    const int STATE_DIM = 25;  // 25维状态空间

    // 更新PPO agent的状态维度
    if (route_planner_->getPPOAgent()) {
        route_planner_->getPPOAgent()->setStateDim(STATE_DIM);
    }

    // ========== 加载可达性参数 ==========
    auto get_param = [&](const std::vector<const char*>& keys, double default_val) -> double {
        for (auto key : keys) {
            auto it = planner_parameters_.find(key);
            if (it != planner_parameters_.end()) {
                return it->second;
            }
        }
        return default_val;
    };

    double decay = get_param({"common_reachability_decay",
                                 "auto_reachability_decay",
                                 "reachability_decay"}, 0.95);

    int min_samples = static_cast<int>(get_param({"common_reachability_min_samples",
                                                     "auto_reachability_min_samples",
                                                     "reachability_min_samples"}, 3));

    double tolerance = get_param({"common_reachability_position_tolerance",
                                   "auto_reachability_position_tolerance",
                                   "reachability_position_tolerance"}, 0.5);

    // 更新CostMap的可达性参数
    costmap_->setReachabilityParameters(decay, min_samples, tolerance);

    AD_DEBUG(AutoPlanner, "Reachability: decay=%.2f, min_samples=%d, tolerance=%.2f",
             decay, min_samples, tolerance);

    return true;
}

// updateCostmapWithStatistics() has been moved to DataManager
// This method is no longer needed in AutoPlanner as it only handles path planning

Point AutoPlanner::optimizeNextWaypoint() {
    // Use sampling optimizer to find next optimal point
    Point next_waypoint = sampling_optimizer_->optimizeNextSample(*costmap_, current_position_);

    AD_DEBUG(AutoPlanner, "Next waypoint: %s", LogUtils::formatPoint(next_waypoint).c_str());

    return next_waypoint;
}

bool AutoPlanner::validatePath(const std::vector<Point>& path) {
    if (path.empty()) {
        return false;
    }

    AD_DEBUG(AutoPlanner, "Validating path: %d waypoints", path.size());

    // 检查内存使用
    if (!memory_monitor_->allocate("path_validation", 5)) {
        AD_WARN(AutoPlanner, "Memory allocation rejected for path validation");
        return false;
    }

    // 检查路径可行性
    if (!PlannerUtils::isPathValid(path, *costmap_)) {
        AD_WARN(AutoPlanner, "Path has collisions with obstacles");
        memory_monitor_->deallocate("path_validation", 5);
        return false;
    }

    // 检查语义约束
    auto violations = constraint_checker_->checkPathConstraints(path);
    if (!violations.empty()) {
        AD_WARN(AutoPlanner, "Path violates %d constraints", violations.size());
        for (const auto& violation : violations) {
            AD_WARN(AutoPlanner, "Constraint violation: %s", violation.description.c_str());
        }
        // 根据严重程度决定是否接受路径
        bool has_critical_or_high = false;
        for (const auto& violation : violations) {
            if (violation.severity >= 0.5) {
                has_critical_or_high = true;
                break;
            }
        }

        if (has_critical_or_high) {
            memory_monitor_->deallocate("path_validation", 5);
            return false;
        }
    }

    // 释放内存
    memory_monitor_->deallocate("path_validation", 5);

    return true;
}

// updateCoverageMetrics() has been moved to DataManager
// This method is no longer needed in AutoPlanner as it only handles path planning

double AutoPlanner::computeStateReward(const StateInfo& prev_state_info,
                                         const StateInfo& new_state_info) {
    // Create reward configuration from parameters
    RewardConfig config;
    if (planner_parameters_.find("reward_distance_improvement_scale") != planner_parameters_.end())
        config.distance_improvement_scale = planner_parameters_["reward_distance_improvement_scale"];

    if (planner_parameters_.find("reward_step_penalty") != planner_parameters_.end())
        config.step_penalty = planner_parameters_["reward_step_penalty"];

    if (planner_parameters_.find("reward_goal") != planner_parameters_.end())
        config.goal_reward = planner_parameters_["reward_goal"];

    if (planner_parameters_.find("reward_collision_penalty") != planner_parameters_.end())
        config.collision_penalty = planner_parameters_["reward_collision_penalty"];

    if (planner_parameters_.find("reward_new_sparse") != planner_parameters_.end())
        config.new_sparse_reward = planner_parameters_["reward_new_sparse"];

    if (planner_parameters_.find("reward_new_area") != planner_parameters_.end())
        config.new_area_reward = planner_parameters_["reward_new_area"];

    if (planner_parameters_.find("reward_inefficient_path_penalty") != planner_parameters_.end())
        config.new_area_reward = planner_parameters_["reward_inefficient_path_penalty"];

    if (planner_parameters_.find("reward_repeat_visit_penalty_factor") != planner_parameters_.end())
        config.repeat_visit_penalty_factor = planner_parameters_["reward_repeat_visit_penalty_factor"];

    if (planner_parameters_.find("reward_total_path_length") != planner_parameters_.end())
        config.total_path_length = planner_parameters_["reward_total_path_length"];

    // 传递配置到计算器
    double reward = RewardCalculator::computeReward(prev_state_info, new_state_info, config);

    AD_DEBUG(AutoPlanner, "Reward: %f", reward);

    return reward;
}

void AutoPlanner::reloadConfiguration() {
    // 检查内存使用
    if (!memory_monitor_->allocate("config_reload", 10)) {
        AD_WARN(AutoPlanner, "Memory allocation rejected for config reload");
        return;
    }

    if (loadConfiguration(config_file_path_)) {
        // Reinitialize with new parameters
        initialize();
        AD_INFO(AutoPlanner, "Configuration reloaded");

        // 打印内存使用情况
        if (memory_monitor_->isHighUsage()) {
            AD_WARN(PLANNER, "High memory usage detected: %.1f%%", memory_monitor_->getUsageRatio() * 100.0);
        }
    } else {
        AD_ERROR(AutoPlanner, "Failed to reload configuration");
    }

    // 释放内存
    memory_monitor_->deallocate("config_reload", 10);
}

// addDataPoint() has been moved to DataManager
// This method is no longer needed in AutoPlanner as it only handles path planning

bool AutoPlanner::loadPPOWeights(const std::string& filepath) {
    if (route_planner_->getPPOAgent()) {
        bool success = route_planner_->getPPOAgent()->loadWeights(filepath);
        if (success) {
            AD_DEBUG(AutoPlanner, "PPO weights loaded: %s", filepath.c_str());
            return true;
        } else {
            AD_ERROR(AutoPlanner, "Failed to load PPO weights: %s", filepath.c_str());
            return false;
        }
    }
    AD_WARN(AutoPlanner, "PPO agent not available");
    return false;
}

void AutoPlanner::reset() {
    // Reset planner state
    current_position_ = Point(0.0, 0.0);
    goal_position_ = Point(0.0, 0.0);
    planner_path_.clear();

    // 重置内存监控
    memory_monitor_->reset();
}

Trajectory AutoPlanner::plan(const PlannerInput& input) {
    // 检查内存使用
    if (!memory_monitor_->allocate("planning", 20)) {
        AD_WARN(AutoPlanner, "Memory allocation rejected for planning");
        // 返回空轨迹
        return Trajectory();
    }

    // Set the current and goal positions from the input
    current_position_ = input.start;
    goal_position_ = input.goal;

    // Plan path using the navigation planner
    if (use_ppo_) {
        planner_path_ = route_planner_->computePPOPath(*costmap_, input.start, input.goal);
    } else {
        // Even when not using PPO, we may still want to use the route planner for A* path
        planner_path_ = route_planner_->computeAStarPath(*costmap_, input.start, input.goal);
    }

    // Create trajectory from the path
    Trajectory trajectory;
    trajectory.states = planner_path_;

    // Validate path
    if (!validatePath(planner_path_)) {
        AD_WARN(AutoPlanner, "Planned path has constraint violations");
    }

    // 计算总成本
    if (!planner_path_.empty()) {
        for (size_t i = 1; i < planner_path_.size(); ++i) {
            double segment_cost = PlannerUtils::euclideanDistance(planner_path_[i-1], planner_path_[i]);
            trajectory.total_cost += segment_cost;
        }
    }

    // 释放内存
    memory_monitor_->deallocate("planning", 20);

    return trajectory;
}

// 打印性能统计
void AutoPlanner::printPerformanceStats() {
    const auto stats = getStats();

    AD_INFO(AutoPlanner, "Stats: plans=%lu, planning=%.2f/%.2f/%.2fms (avg/min/max), inferences=%lu, errors=%lu",
            stats.total_plans,
            stats.avg_planning_time_ms, stats.min_planning_time_ms, stats.max_planning_time_ms,
            stats.total_inferences, stats.error_count);

    // 打印推理统计
    if (ppo_agent_) {
        const auto& agent_stats = ppo_agent_->getInferenceStats();
        AD_DEBUG(AutoPlanner, "PPO: inferences=%lu, avg_latency=%.2fms",
                 agent_stats.total_inferences.load(), agent_stats.getAverageLatency());
    }
}

void AutoPlanner::updateParameters(const std::map<std::string, double>& parameters) {
    // 更新参数映射
    for (const auto& [key, value] : parameters) {
        planner_parameters_[key] = value;
    }

    // 更新PPO Agent配置
    if (ppo_agent_) {
        ppo_agent_->updateConfigFromParameters(parameters);
    }

    // 更新costmap参数
    auto sparse_it = parameters.find("sparse_threshold");
    auto exploration_it = parameters.find("exploration_bonus");
    auto redundancy_it = parameters.find("redundancy_penalty");

    if (sparse_it != parameters.end() && exploration_it != parameters.end() && redundancy_it != parameters.end()) {
        costmap_->setParameters(sparse_it->second, exploration_it->second, redundancy_it->second);
    }

    AD_INFO(AutoPlanner, "Updated %zu parameters", parameters.size());
}

// ===== NEW: Type-Safe State Building Implementation =====

AutoPlanner::PlannerState AutoPlanner::buildCurrentState(
    const Point& position,
    const CostMap& costmap,
    const std::vector<int>& action_history
) {
    PlannerState state;  // State<AutoStateTraits> - 25维，栈分配

    // ===== [0-1]: 归一化位置 =====
    double norm_x = static_cast<double>(position.x) / costmap.getWidth();
    double norm_y = static_cast<double>(position.y) / costmap.getHeight();
    state.setFeature(AutoStateTraits::NORM_X, norm_x);
    state.setFeature(AutoStateTraits::NORM_Y, norm_y);

    // ===== [2-17]: 热力图摘要 (16维) - 4×4网格 =====
    for (int i = 0; i < 16; i++) {
        int offset_x = (i % 4) - 1;  // -1, 0, 1, 2
        int offset_y = (i / 4) - 1;  // -1, 0, 1, 2
        int check_x = static_cast<int>(position.x) + offset_x;
        int check_y = static_cast<int>(position.y) + offset_y;

        double density = costmap.isValidCell(check_x, check_y) ?
                        costmap.getDataDensity(check_x, check_y) : 0.0;
        state.setFeature(AutoStateTraits::HEATMAP_START + i, density);
    }

    // ===== [18-21]: 历史动作 (4维) =====
    for (size_t i = 0; i < 4; i++) {
        double action = (i < action_history.size()) ?
                       static_cast<double>(action_history[i]) : 0.0;
        state.setFeature(AutoStateTraits::ACTION_HISTORY_START + i, action);
    }

    // ===== [22]: 剩余预算 =====
    // 注意：这里使用默认值1.0，实际应根据具体场景调整
    state.setFeature(AutoStateTraits::REMAINING_BUDGET, 1.0);

    // ===== [23]: 局部密度 =====
    double local_density = 0.0;
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int check_x = static_cast<int>(position.x) + dx;
            int check_y = static_cast<int>(position.y) + dy;
            if (costmap.isValidCell(check_x, check_y)) {
                local_density += costmap.getEffectiveCost(check_x, check_y);
                count++;
            }
        }
    }
    if (count > 0) {
        local_density /= count;
    }
    state.setFeature(AutoStateTraits::LOCAL_DENSITY, local_density);

    // ===== [24]: 可达性评分 =====
    int curr_x = static_cast<int>(position.x);
    int curr_y = static_cast<int>(position.y);
    double reachability = costmap.isValidCell(curr_x, curr_y) ?
                         costmap.getReachabilityScore(curr_x, curr_y) : 0.5;
    state.setFeature(AutoStateTraits::REACHABILITY, reachability);

    return state;  // RVO优化，避免拷贝
}

// ===== IPlanner 接口实现 =====

std::vector<Point> AutoPlanner::planMission(const collector::MissionArea& area) {
    current_position_ = area.center;
    goal_position_ = area.center;

    CostMap* cm = costmap_.get();
    PlannerInput input(current_position_, goal_position_, cm);

    auto trajectory = plan(input);
    return std::move(trajectory.states);
}

void AutoPlanner::updateWithNewData(const std::vector<collector::DataPoint>& data) {
    if (costmap_ && !data.empty()) {
        std::vector<Point> points;
        points.reserve(data.size());
        for (const auto& dp : data) {
            points.push_back(dp.position);
        }
        costmap_->updateWithDataStatistics(points);
        costmap_->adjustCostsBasedOnDensity();
    }
}

void AutoPlanner::reportCoverageMetrics() {
    if (coverage_metric_ && costmap_) {
        AD_INFO(AutoPlanner, "Coverage: %.2f%%",
                coverage_metric_->getCoverageRatio() * 100.0);
    }
}

double AutoPlanner::getAverageReward() const {
    // AutoPlanner 没有内嵌 episode reward 追踪，返回统计信息
    return planner_stats_.avg_planning_time_ms.load() > 0 ? 0.5 : 0.0;
}

} // namespace aurora::planner
