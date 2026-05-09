// auto_planner.h - Autonomous driving mode planner
#ifndef AUTO_PLANNER_H
#define AUTO_PLANNER_H

#include <vector>
#include <string>
#include <memory>
#include <map>

// Include all component headers
#include "../maps/costmap.h"
#include "../observation/state_base.h"
#include "../observation/traits/auto_state_traits.h"
#include "../observation/utils/state_converter.h"
#include "../agents/auto_route_optimize.h"
#include "../agents/auto_reward.h"
#include "../agents/auto_ppo_agent.h"
#include "../maps/coverage_metric.h"
#include "../maps/sampling_optimizer.h"
#include "../safety/semantic_map.h"
#include "../safety/semantic_constraint.h"
#include "../safety/semantic_filter.h"
#include "../utils/planner_utils.h"
#include "i_planner.h"
#include "common/memory_monitor.h"
#include "../config/planner_config_manager.h"
#include "data_collection/common/types.h"

namespace aurora::planner {

/**
 * @brief Auto mode planner for autonomous driving
 *
 * This planner is designed for autonomous driving vehicles with:
 * - 25-dimensional observation space
 * - 4-dimensional discrete action space
 * - PPO-based reinforcement learning
 */
class AutoPlanner : public IPlanner {
private:
    // ===== Type Aliases for Type-Safe State =====
    using PlannerState = State<AutoStateTraits>;

    // Core components
    std::unique_ptr<CostMap> costmap_;
    std::unique_ptr<AutoRoutePlanner> route_planner_;
    std::unique_ptr<SamplingOptimizer> sampling_optimizer_;
    std::unique_ptr<SemanticMap> semantic_map_;
    std::unique_ptr<SemanticConstraintChecker> constraint_checker_;
    std::unique_ptr<SemanticFilter> semantic_filter_;
    std::unique_ptr<CoverageMetric> coverage_metric_;

    // Configuration
    std::string config_file_path_;
    std::string model_file_;
    std::map<std::string, double> planner_parameters_;

    // State variables
    Point current_position_;
    Point goal_position_;
    std::vector<Point> planner_path_;

    // RL parameters
    bool use_ppo_;  // Flag to enable PPO-based planning

    std::unique_ptr<platform::PlatformAdapter> platform_adapter_;
    std::unique_ptr<monitor::MemoryMonitor> memory_monitor_;

    // PPO Agent
    std::unique_ptr<AutoPPOAgent> ppo_agent_;

    // 统计信息
    PlannerStats planner_stats_;

    /**
     * @brief Build type-safe state from current position and environment
     *
     * @param position Current robot position
     * @param costmap Cost map for heatmap data
     * @param action_history Recent action history
     * @return Type-safe 25-dimensional state
     */
    PlannerState buildCurrentState(
        const Point& position,
        const CostMap& costmap,
        const std::vector<int>& action_history
    );

public:
    AutoPlanner(const std::string& model_file, const std::string& config_file);

    ~AutoPlanner() override = default;

    // ===== Implementation of PlannerBase interface =====

    void reset() override;
    Trajectory plan(const PlannerInput& input) override;

    // 生命周期管理
    bool initialize() override;
    bool loadConfiguration(const std::string& config_file) override;
    void reloadConfiguration() override;

    // 状态查询
    bool isReady() const override { return initialized_; }
    PlannerMode getMode() const override { return PlannerMode::AUTO; }

    // 性能统计
    PlannerStatsSnapshot getStats() const override { return planner_stats_.getSnapshot(); }
    void printPerformanceStats() override;

    // 配置更新
    void updateParameters(const std::map<std::string, double>& parameters) override;

    // ===== IPlanner 接口实现 =====

    std::vector<Point> planMission(const collector::MissionArea& area) override;
    void updateWithNewData(const std::vector<collector::DataPoint>& data) override;
    void reportCoverageMetrics() override;
    double getAverageReward() const override;

    /**
     * @brief Optimize next waypoint for data collection
     * @return Next optimal waypoint
     */
    Point optimizeNextWaypoint();

    /**
     * @brief Check if planned path satisfies all constraints
     * @param path Path to validate
     * @return true if path is valid, false otherwise
     */
    bool validatePath(const std::vector<Point>& path);

    /**
     * @brief Compute reward for current state transition
     * @param prev_state_info Information about previous state
     * @param new_state_info Information about new state
     * @return Computed reward
     */
    double computeStateReward(const StateInfo& prev_state_info,
                              const StateInfo& new_state_info);

    /**
     * @brief Load PPO weights from file
     */
    bool loadPPOWeights(const std::string& filepath);

    /**
     * @brief Get current coverage metrics
     */
    const CoverageMetric& getCoverageMetric() const { return *coverage_metric_; }

    /**
     * @brief Get current position (IPlanner override)
     */
    Point getCurrentPosition() const override { return current_position_; }

    /**
     * @brief Set current position
     */
    void setCurrentPosition(const Point& position) { current_position_ = position; }

    /**
     * @brief Get goal position
     */
    const Point& getGoalPosition() const { return goal_position_; }

    /**
     * @brief Set goal position
     */
    void setGoalPosition(const Point& position) { goal_position_ = position; }

    /**
     * @brief Get the route planner instance for direct access
     */
    AutoRoutePlanner* getAutoRoutePlanner() { return route_planner_.get(); }

    /**
     * @brief Get the costmap instance for data management
     * @return Pointer to the costmap
     */
    CostMap* getCostMap() { return costmap_.get(); }

    /**
     * @brief Get the costmap instance for data management (const version)
     * @return Const pointer to the costmap
     */
    const CostMap* getCostMap() const { return costmap_.get(); }

};

} // namespace aurora::planner
#endif // AUTO_PLANNER_H
