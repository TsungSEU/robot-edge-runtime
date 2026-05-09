// humanoid_planner.cpp
#include "humanoid_planner.h"
#include "../utils/planner_utils.h"
#include "common/log/logger.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace aurora::planner {

// ===== HumanoidPlanner 实现 =====

HumanoidPlanner::HumanoidPlanner(const std::string& model_file,
                                const std::string& config_file,
                                const HumanoidPlannerConfig& config)
    : ppo_config_(config.ppo_config)
    , reward_config_(config.reward_config)
    , value_config_(config.value_config)
    , config_(config)
    , action_history_{}
    , action_history_idx_(0)
    , model_file_(model_file)
    , config_file_(config_file)
    , current_step_(0)
    , episode_reward_(0)
    , total_plans_(0)
    , total_planning_time_ms_(0) {
    // 路径信息已在 PlannerFactory 中打印，这里只打印简短消息
    AD_INFO(HumanoidPlanner, "Initializing (43-dim state, 3-dim action)");
}

bool HumanoidPlanner::initialize() {
    // 初始化数据价值模型
    data_value_model_ = std::make_unique<HumanoidDataValueModel>(value_config_, 0.5);

    // 初始化成本地图
    costmap_ = std::make_unique<CostMap>(80, 80, 0.5);  // 40m x 40m, 0.5m 分辨率

    // 初始化 PPO Agent (43-dim state, 3-dim action)
    ppo_agent_ = std::make_unique<HumanoidPPOAgent>(ppo_config_);
    model_loaded_ = ppo_agent_->loadOnnxModel(model_file_);
    if (!model_loaded_) {
        AD_WARN(HumanoidPlanner, "Failed to load ONNX model, will use fallback");
    } else {
        AD_INFO(HumanoidPlanner, "ONNX model loaded");
    }

    // 加载配置
    loadConfiguration(config_file_);

    AD_INFO(HumanoidPlanner, "Initialized successfully");
    return true;
}

void HumanoidPlanner::reset() {
    current_step_ = 0;
    episode_reward_ = 0;
    current_state_ = HumanoidStateInfo();
    last_action_ = HumanoidAction();
    action_history_.fill(0.0);
    action_history_idx_ = 0;
    if (ppo_agent_) ppo_agent_->resetStatistics();
}

Trajectory HumanoidPlanner::plan(const PlannerInput& input) {
    UnifiedPlannerInput unified_input;
    unified_input.start = input.start;
    unified_input.goal = input.goal;

    auto result = planUnified(unified_input);

    Trajectory trajectory;
    trajectory.states = result.path;
    trajectory.total_cost = result.total_cost;

    return trajectory;
}

UnifiedTrajectory HumanoidPlanner::planUnified(const UnifiedPlannerInput& input) {
    std::lock_guard<std::mutex> lock(planning_mutex_);

    auto start_time = std::chrono::high_resolution_clock::now();
    total_plans_++;

    UnifiedTrajectory trajectory;
    trajectory.path.push_back(input.start);

    Point current_pos = input.start;
    Point goal = input.goal;

    double total_dist = std::sqrt(
        std::pow(goal.x - current_pos.x, 2) +
        std::pow(goal.y - current_pos.y, 2));

    // ===== 直线插值路径（模型未加载时使用） =====
    if (!model_loaded_) {
        AD_INFO(HumanoidPlanner, "Fallback straight-line path (%.2fm)", total_dist);

        double waypoint_spacing = 0.5;
        int num_points = static_cast<int>(std::ceil(total_dist / waypoint_spacing));

        for (int i = 1; i <= num_points; ++i) {
            double t = static_cast<double>(i) / num_points;
            Point wp;
            wp.x = current_pos.x + t * (goal.x - current_pos.x);
            wp.y = current_pos.y + t * (goal.y - current_pos.y);
            trajectory.path.push_back(wp);
            trajectory.steps_taken++;

            HumanoidAction action;
            action.forward_vel = 0.5;
            trajectory.actions.push_back(action);
        }

        trajectory.reached_goal = true;
        trajectory.total_cost = total_dist;
        trajectory.distance_traveled = total_dist;
    } else {
        // ===== PPO 推理 + 目标导航修正 =====
        const int max_steps = config_.max_planning_steps;
        const double dt = config_.planning_dt;
        double theta = input.humanoid_state.theta;

        HumanoidStateInfo sim_state = input.humanoid_state;
        sim_state.x = input.start.x;
        sim_state.y = input.start.y;
        sim_state.map_width = 40.0;
        sim_state.map_height = 40.0;
        sim_state.max_range = 10.0;

        for (int step = 0; step < max_steps; ++step) {
            double dist_to_goal = std::sqrt(
                std::pow(current_pos.x - goal.x, 2) +
                std::pow(current_pos.y - goal.y, 2));

            if (dist_to_goal < 0.5) {
                trajectory.reached_goal = true;
                break;
            }

            // 更新导航状态
            sim_state.goal_dx = goal.x - current_pos.x;
            sim_state.goal_dy = goal.y - current_pos.y;
            sim_state.goal_distance = dist_to_goal;
            double abs_goal_heading = std::atan2(sim_state.goal_dy, sim_state.goal_dx);
            double bearing = abs_goal_heading - theta;
            while (bearing > M_PI) bearing -= 2.0 * M_PI;
            while (bearing < -M_PI) bearing += 2.0 * M_PI;
            sim_state.goal_bearing = bearing;

            // 计算 data_value_sectors (8方向) — 委托给 SectorComputer
            if (costmap_ && data_value_model_) {
                sim_state.data_value_sectors = SectorComputer::computeDataValueSectors(
                    *costmap_, *data_value_model_, current_pos.x, current_pos.y, theta, 10.0);
                sim_state.obstacle_sectors = SectorComputer::computeObstacleSectors(
                    *costmap_, current_pos.x, current_pos.y, theta, 5.0);
            }

            // 可达性
            int cell_x = static_cast<int>(current_pos.x / 0.5);
            int cell_y = static_cast<int>(current_pos.y / 0.5);
            if (costmap_ && costmap_->isValidCell(cell_x, cell_y)) {
                sim_state.current_value = costmap_->getEffectiveCost(cell_x, cell_y) / 5.0;
            }

            // 填入动作历史
            for (int i = 0; i < 8; ++i) {
                size_t idx = (action_history_idx_ + i) % 8;
                sim_state.action_history[i] = action_history_[idx];
            }

            // PPO 推理
            HumanoidState nn_state = HumanoidState::fromStateInfo(sim_state);
            HumanoidAction action;
            if (ppo_agent_) {
                std::vector<double> action_raw;
                double log_prob;
                action = ppo_agent_->selectAction(nn_state, action_raw, log_prob);
            }

            // 目标导航修正
            {
                double heading_error = sim_state.goal_bearing;
                double correction_weight = std::max(0.3, std::min(1.0, std::abs(heading_error) / (M_PI / 3)));
                double corrected_turn = std::clamp(2.0 * heading_error, -0.3, 0.3);
                action.angular_vel = (1.0 - correction_weight) * action.angular_vel
                                   + correction_weight * corrected_turn;

                double alignment = std::cos(heading_error);
                if (alignment > 0.0) {
                    action.forward_vel = std::max(action.forward_vel, 0.2 * alignment);
                } else {
                    action.forward_vel = std::max(action.forward_vel, -0.1);
                }
                action.clip();
            }

            // 更新动作历史
            pushAction(action.forward_vel);

            // 执行动作 (10Hz 步进)
            double dx = dt * (action.forward_vel * std::cos(theta) - action.lateral_vel * std::sin(theta));
            double dy = dt * (action.forward_vel * std::sin(theta) + action.lateral_vel * std::cos(theta));
            double dtheta = dt * action.angular_vel;

            Point next_pos;
            next_pos.x = current_pos.x + dx;
            next_pos.y = current_pos.y + dy;
            theta += dtheta;

            // 碰撞检查
            if (costmap_ && costmap_->isValidCell(
                    static_cast<int>(next_pos.x / 0.5),
                    static_cast<int>(next_pos.y / 0.5))) {
                double effective_cost = costmap_->getEffectiveCost(
                    static_cast<int>(next_pos.x / 0.5),
                    static_cast<int>(next_pos.y / 0.5));
                if (effective_cost > 2.0) {
                    trajectory.fell_down = true;
                    break;
                }
            }

            trajectory.path.push_back(next_pos);
            trajectory.actions.push_back(action);

            double step_dist = std::sqrt(dx * dx + dy * dy);
            trajectory.total_cost += step_dist;
            trajectory.distance_traveled += step_dist;

            double value = ppo_agent_ ? ppo_agent_->evaluateValue(nn_state) : 0.0;
            trajectory.values.push_back(value);

            current_pos = next_pos;
            trajectory.steps_taken++;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double planning_time = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();
    total_planning_time_ms_ += planning_time;

    AD_DEBUG(HumanoidPlanner, "Planning: steps=%d, cost=%.2f, time=%.2fms, reached=%d",
             trajectory.steps_taken, trajectory.total_cost, planning_time, trajectory.reached_goal);

    return trajectory;
}

UnifiedTrajectory HumanoidPlanner::planWithState(
    const Point& start,
    const Point& goal,
    const HumanoidStateInfo& state_info) {

    UnifiedPlannerInput input;
    input.start = start;
    input.goal = goal;
    input.humanoid_state = state_info;

    return planUnified(input);
}

HumanoidAction HumanoidPlanner::selectAction(const HumanoidStateInfo& state_info) {
    HumanoidState state = HumanoidState::fromStateInfo(state_info);

    std::vector<double> action_raw;
    double log_prob;

    if (ppo_agent_) {
        return ppo_agent_->selectAction(state, action_raw, log_prob);
    }

    return HumanoidAction();
}

double HumanoidPlanner::evaluateStateValue(const HumanoidStateInfo& state_info) {
    HumanoidState state = HumanoidState::fromStateInfo(state_info);

    if (ppo_agent_) {
        return ppo_agent_->evaluateValue(state);
    }

    return 0.0;
}

DataValueResult HumanoidPlanner::evaluateLocationValue(double x, double y,
                                                      SceneType scene_type) {
    if (data_value_model_) {
        return data_value_model_->evaluateLocationValue(x, y, scene_type);
    }
    return DataValueResult();
}

void HumanoidPlanner::addDataPoint(const DataPointMetadata& metadata) {
    if (data_value_model_) {
        data_value_model_->addDataPoint(metadata);
    }

    if (costmap_) {
        std::vector<Point> points = {Point(metadata.x, metadata.y)};
        costmap_->updateWithDataStatistics(points);
        costmap_->adjustCostsBasedOnDensity();
    }
}

void HumanoidPlanner::reportExecutionFeedback(const Point& planned_waypoint,
                                              const Point& actual_position,
                                              bool collection_success,
                                              double gait_stability) {
    if (costmap_) {
        costmap_->updateReachability(planned_waypoint, actual_position,
                                    collection_success, gait_stability);

        AD_DEBUG(HumanoidPlanner,
                "Execution feedback: planned=(%.2f,%.2f), actual=(%.2f,%.2f), "
                "success=%d, stability=%.2f",
                planned_waypoint.x, planned_waypoint.y,
                actual_position.x, actual_position.y,
                collection_success, gait_stability);

        int cell_x = static_cast<int>(planned_waypoint.x / costmap_->getResolution());
        int cell_y = static_cast<int>(planned_waypoint.y / costmap_->getResolution());
        if (costmap_->isValidCell(cell_x, cell_y)) {
            int attempts, successes;
            costmap_->getExecutionStats(cell_x, cell_y, attempts, successes);
            double reachability = costmap_->getReachabilityScore(cell_x, cell_y);

            AD_DEBUG(HumanoidPlanner,
                    "Cell (%d,%d): reachability=%.2f, attempts=%d, successes=%d",
                    cell_x, cell_y, reachability, attempts, successes);
        }
    }
}

void HumanoidPlanner::reportExecutionFeedbackBatch(
    const std::vector<ExecutionFeedback>& feedback) {

    if (costmap_) {
        costmap_->updateReachabilityBatch(feedback);
        AD_INFO(HumanoidPlanner,
               "Batch execution feedback: %zu entries processed",
               feedback.size());

        int total_attempts = 0;
        int total_successes = 0;

        for (const auto& fb : feedback) {
            total_attempts++;
            if (fb.collection_success) {
                total_successes++;
            }
        }

        int reliable_cells = 0;
        for (int y = 0; y < costmap_->getHeight(); y += 2) {
            for (int x = 0; x < costmap_->getWidth(); x += 2) {
                if (costmap_->hasReliableReachability(x, y)) {
                    reliable_cells++;
                }
            }
        }

        AD_INFO(HumanoidPlanner,
               "Reachability summary: %d/%d success, %d reliable cells",
               total_successes, total_attempts, reliable_cells);
    }
}

double HumanoidPlanner::computeReward(const HumanoidRewardState& reward_state) {
    return HumanoidRewardCalculator::computeReward(reward_state, reward_config_);
}

bool HumanoidPlanner::loadConfiguration(const std::string& config_file) {
    AD_INFO(HumanoidPlanner, "Loading configuration from: %s", config_file.c_str());

    std::map<std::string, double> params;
    if (!PlannerUtils::loadModeSpecificParameters(config_file, "humanoid", params)) {
        AD_ERROR(HumanoidPlanner, "Failed to load configuration from YAML file");
        return false;
    }

    // 推理配置
    auto hasParam = [&](const std::vector<std::string>& keys) -> bool {
        for (const auto& key : keys) {
            if (params.find(key) != params.end()) return true;
        }
        return false;
    };

    auto getParam = [&](const std::vector<std::string>& keys, double def = 0.0) -> double {
        for (const auto& key : keys) {
            auto it = params.find(key);
            if (it != params.end()) return it->second;
        }
        return def;
    };

    if (hasParam({"humanoid_inference_init_log_std"}))
        ppo_config_.init_log_std = getParam({"humanoid_inference_init_log_std"});
    if (hasParam({"humanoid_inference_min_log_std"}))
        ppo_config_.min_log_std = getParam({"humanoid_inference_min_log_std"});
    if (hasParam({"humanoid_inference_max_log_std"}))
        ppo_config_.max_log_std = getParam({"humanoid_inference_max_log_std"});
    if (hasParam({"humanoid_inference_num_inference_threads"}))
        ppo_config_.num_inference_threads = static_cast<int>(
            getParam({"humanoid_inference_num_inference_threads"}));

    // 奖励配置
    reward_config_ = HumanoidRewardCalculator::fromParams(params);

    // 数据价值配置
    if (hasParam({"data_value_w_spatial_rarity", "w_spatial_rarity"}))
        value_config_.w_spatial_rarity = getParam({"data_value_w_spatial_rarity", "w_spatial_rarity"});
    if (hasParam({"data_value_w_temporal_freshness", "w_temporal_freshness"}))
        value_config_.w_temporal_freshness = getParam({"data_value_w_temporal_freshness", "w_temporal_freshness"});
    if (hasParam({"data_value_w_scene_diversity", "w_scene_diversity"}))
        value_config_.w_scene_diversity = getParam({"data_value_w_scene_diversity", "w_scene_diversity"});
    if (hasParam({"data_value_w_quality", "w_quality"}))
        value_config_.w_quality = getParam({"data_value_w_quality", "w_quality"});
    if (hasParam({"data_value_w_coverage", "w_coverage"}))
        value_config_.w_coverage = getParam({"data_value_w_coverage", "w_coverage"});
    if (hasParam({"data_value_temporal_decay_rate"}))
        value_config_.temporal_decay_rate = getParam({"data_value_temporal_decay_rate"});
    if (hasParam({"data_value_recency_bonus_duration"}))
        value_config_.recency_bonus_duration = static_cast<int>(getParam({"data_value_recency_bonus_duration"}));
    if (hasParam({"data_value_min_quality_threshold"}))
        value_config_.min_quality_threshold = getParam({"data_value_min_quality_threshold"});
    if (hasParam({"data_value_rare_scene_bonus"}))
        value_config_.rare_scene_bonus = getParam({"data_value_rare_scene_bonus"});

    // 成本地图参数
    if (costmap_) {
        double sparse_threshold = getParam({"sparse_threshold"}, 0.15);
        double exploration_bonus = getParam({"exploration_bonus"}, 10.0);
        double redundancy_penalty = getParam({"redundancy_penalty"}, 5.0);

        costmap_->setParameters(sparse_threshold, exploration_bonus, redundancy_penalty);

        double reachability_decay = getParam({"reachability_decay", "reachability_decay_rate"}, 0.95);
        double min_samples = getParam({"reachability_min_samples", "min_samples_for_reliability"}, 3.0);
        double position_tolerance = getParam({"reachability_position_tolerance", "position_tolerance"}, 0.5);

        costmap_->setReachabilityParameters(reachability_decay,
                                            static_cast<int>(min_samples),
                                            position_tolerance);

        AD_INFO(HumanoidPlanner, "Reachability parameters: decay=%.2f, min_samples=%.0f, tolerance=%.2f",
                reachability_decay, min_samples, position_tolerance);
    }

    // 规划步长参数
    if (hasParam({"max_planning_steps", "max_steps"})) {
        config_.max_planning_steps = static_cast<int>(getParam({"max_planning_steps", "max_steps"}, 500));
    }
    if (hasParam({"policy_hz", "control_policy_hz"})) {
        double policy_hz = getParam({"policy_hz", "control_policy_hz"}, 10.0);
        config_.planning_dt = 1.0 / policy_hz;  // 转换为时间步长
    }

    AD_INFO(HumanoidPlanner, "Planning parameters: max_steps=%d, dt=%.3f (policy_hz=%.1f)",
            config_.max_planning_steps, config_.planning_dt, 1.0 / config_.planning_dt);

    AD_INFO(HumanoidPlanner, "Configuration loaded successfully");
    return true;
}

void HumanoidPlanner::reloadConfiguration() {
    loadConfiguration(config_file_);
}

bool HumanoidPlanner::loadPPOModel(const std::string& model_path) {
    if (ppo_agent_) {
        return ppo_agent_->loadOnnxModel(model_path);
    }
    return false;
}

void HumanoidPlanner::updatePPOConfig(const HumanoidPPOConfig& config) {
    ppo_config_ = config;
    ppo_agent_ = std::make_unique<HumanoidPPOAgent>(ppo_config_);
    ppo_agent_->loadOnnxModel(model_file_);
}

void HumanoidPlanner::updateRewardConfig(const HumanoidRewardConfig& config) {
    reward_config_ = config;
}

void HumanoidPlanner::updateValueConfig(const DataValueConfig& config) {
    value_config_ = config;
    data_value_model_ = std::make_unique<HumanoidDataValueModel>(value_config_, 0.5);
}

HumanoidPPOAgent::InferenceStats HumanoidPlanner::getInferenceStats() const {
    if (ppo_agent_) {
        return ppo_agent_->getInferenceStats();
    }
    return HumanoidPPOAgent::InferenceStats{};
}

void HumanoidPlanner::printPerformanceStats() {
    const auto stats = getInferenceStats();

    AD_INFO(HumanoidPlanner, "=== HumanoidPlanner Performance Stats ===");
    AD_INFO(HumanoidPlanner, "Total plans: %lu", total_plans_);
    AD_INFO(HumanoidPlanner, "Avg planning time: %.2f ms",
           total_plans_ > 0 ? total_planning_time_ms_ / total_plans_ : 0);

    if (stats.total_count > 0) {
        AD_INFO(HumanoidPlanner, "PPO Inferences: %u", stats.total_count);
        AD_INFO(HumanoidPlanner, "Avg inference: %.2f ms", stats.avg_latency_ms);
    }

    AD_INFO(HumanoidPlanner, "==========================================");
}

void HumanoidPlanner::updateCostMap() {
    if (data_value_model_ && costmap_) {
        data_value_model_->updateStatistics();
    }
}

bool HumanoidPlanner::validateTrajectory(const UnifiedTrajectory& trajectory) {
    if (trajectory.fell_down) return false;
    if (trajectory.steps_taken > 500) return false;
    return true;
}

void HumanoidPlanner::updateParameters(const std::map<std::string, double>& parameters) {
    auto lr_it = parameters.find("learning_rate");
    if (lr_it != parameters.end()) {
        // PPO learning rate not applicable for inference
    }

    auto w_data_it = parameters.find("reward_w_approach");
    if (w_data_it != parameters.end()) {
        reward_config_.w_approach = w_data_it->second;
    }

    auto sparse_it = parameters.find("sparse_threshold");
    auto exploration_it = parameters.find("exploration_bonus");
    auto redundancy_it = parameters.find("redundancy_penalty");

    if (sparse_it != parameters.end() && exploration_it != parameters.end() && redundancy_it != parameters.end()) {
        costmap_->setParameters(sparse_it->second, exploration_it->second, redundancy_it->second);
    }

    AD_INFO(HumanoidPlanner, "Parameters updated");
}

// ===== IPlanner 接口实现 =====

std::vector<Point> HumanoidPlanner::planMission(const collector::MissionArea& area) {
    Point start = current_state_.x != 0 || current_state_.y != 0
                ? Point(current_state_.x, current_state_.y)
                : area.center;

    auto trajectory = planWithState(start, area.center, current_state_);

    std::vector<Point> result;
    result.reserve(trajectory.path.size());
    for (const auto& pt : trajectory.path) {
        result.push_back(pt);
    }

    return result;
}

void HumanoidPlanner::updateWithNewData(const std::vector<collector::DataPoint>& data) {
    if (!data.empty() && costmap_) {
        std::vector<Point> points;
        points.reserve(data.size());
        for (const auto& dp : data) {
            points.push_back(dp.position);
        }
        costmap_->updateWithDataStatistics(points);
        costmap_->adjustCostsBasedOnDensity();
    }
}

void HumanoidPlanner::reportCoverageMetrics() {
    if (costmap_) {
        int total = costmap_->getWidth() * costmap_->getHeight();
        int visited = 0;
        for (int y = 0; y < costmap_->getHeight(); ++y) {
            for (int x = 0; x < costmap_->getWidth(); ++x) {
                if (costmap_->getEffectiveCost(x, y) > 0) visited++;
            }
        }
        AD_INFO(HumanoidPlanner, "Coverage: %d/%d cells (%.1f%%)",
                visited, total, 100.0 * visited / std::max(1, total));
    }
}

double HumanoidPlanner::getAverageReward() const {
    return episode_reward_;
}

void HumanoidPlanner::setGoalPosition(const Point& goal) {
    // HumanoidPlanner 不直接存储 goal，由 planMission 参数传入
    // 此接口保留以兼容 IPlanner
}

void HumanoidPlanner::setCurrentPosition(const Point& pos) {
    current_state_.x = pos.x;
    current_state_.y = pos.y;
}

Point HumanoidPlanner::getCurrentPosition() const {
    return Point(current_state_.x, current_state_.y);
}

} // namespace aurora::planner
