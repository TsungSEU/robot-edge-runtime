// humanoid_planner.h
#ifndef HUMANOID_PLANNER_H
#define HUMANOID_PLANNER_H

#include "i_planner.h"
#include "../agents/humanoid_state.h"
#include "../agents/humanoid_action.h"
#include "../agents/humanoid_reward.h"
#include "../agents/humanoid_ppo_agent.h"
#include "../maps/humanoid_data_value.h"
#include "../maps/costmap.h"
#include "../maps/coverage_metric.h"
#include "../config/planner_config_manager.h"
#include "../utils/sector_computer.h"
#include "data_collection/common/types.h"

#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <array>

namespace aurora::planner {

/**
 * @brief Humanoid 规划配置 (43-dim state, 3-dim action)
 */
struct HumanoidPlannerConfig {
    HumanoidPPOConfig ppo_config;
    HumanoidRewardConfig reward_config;
    DataValueConfig value_config;

    // 规划步长参数
    int max_planning_steps = 500;           // 最大规划步数
    double planning_dt = 0.1;                // 规划时间步长 (秒)，默认 0.1 (10Hz)

    HumanoidPlannerConfig() {
        ppo_config.state_dim = HumanoidState::STATE_DIM;
        ppo_config.action_dim = HumanoidAction::ACTION_DIM;
    }
};

/**
 * @brief 统一的规划输入
 */
struct UnifiedPlannerInput {
    Point start;
    Point goal;

    // Humanoid 状态信息 (43-dim)
    HumanoidStateInfo humanoid_state;

    UnifiedPlannerInput() : start(0, 0), goal(0, 0) {}
};

/**
 * @brief 统一的轨迹输出
 */
struct UnifiedTrajectory {
    std::vector<Point> path;           // 路径点
    std::vector<HumanoidAction> actions;  // 动作序列
    std::vector<double> values;        // 价值估计
    double total_cost = 0.0;
    double total_reward = 0.0;

    // 统计信息
    int steps_taken = 0;
    double distance_traveled = 0.0;
    bool reached_goal = false;
    bool fell_down = false;

    UnifiedTrajectory() = default;
};

/**
 * @brief 人形机器人数据采集规划器
 *
 * 43维状态空间, 3维连续动作空间 (速度命令)
 * - HumanoidPPOAgent: PPO策略推理
 * - HumanoidDataValueModel: 多维度数据价值评估
 * - HumanoidRewardCalculator: 多目标奖励计算 (10分量)
 * - CostMap: 空间成本地图
 */
class HumanoidPlanner : public IPlanner {
private:
    // 核心组件
    std::unique_ptr<HumanoidPPOAgent> ppo_agent_;
    std::unique_ptr<HumanoidDataValueModel> data_value_model_;
    std::unique_ptr<CostMap> costmap_;

    // 状态跟踪
    HumanoidStateInfo current_state_;
    HumanoidAction last_action_;
    HumanoidPPOConfig ppo_config_;
    HumanoidRewardConfig reward_config_;
    DataValueConfig value_config_;
    HumanoidPlannerConfig config_;  // 规划器配置
    std::array<double, 8> action_history_;  // 最近8步 forward_vel
    size_t action_history_idx_ = 0;

    // 配置
    std::string model_file_;
    std::string config_file_;

    // 状态变量
    bool model_loaded_ = false;
    int current_step_;
    double episode_reward_;

    // 统计信息
    PlannerStats planner_stats_;
    uint64_t total_plans_;
    double total_planning_time_ms_;

    // 线程安全
    mutable std::mutex planning_mutex_;

public:
    /**
     * @brief 构造函数
     */
    HumanoidPlanner(const std::string& model_file,
                   const std::string& config_file,
                   const HumanoidPlannerConfig& config = HumanoidPlannerConfig());

    /**
     * @brief 析构函数
     */
    ~HumanoidPlanner() override = default;

    // ===== PlannerBase接口实现 =====

    void reset() override;
    UnifiedTrajectory planUnified(const UnifiedPlannerInput& input);
    Trajectory plan(const PlannerInput& input) override;

    // 生命周期管理
    bool initialize() override;
    bool loadConfiguration(const std::string& config_file) override;
    void reloadConfiguration() override;

    // 状态查询
    bool isReady() const override { return initialized_; }
    PlannerMode getMode() const override { return PlannerMode::HUMANOID; }

    // 性能统计
    PlannerStatsSnapshot getStats() const override { return planner_stats_.getSnapshot(); }
    void printPerformanceStats() override;

    // 配置更新
    void updateParameters(const std::map<std::string, double>& parameters) override;

    // ===== IPlanner 接口实现 =====

    std::vector<Point> planMission(const collector::MissionArea& area) override;
    void updateWithNewData(const std::vector<collector::DataPoint>& data) override;
    CostMap* getCostMap() override { return costmap_.get(); }
    void reportCoverageMetrics() override;
    double getAverageReward() const override;
    void setGoalPosition(const Point& goal) override;
    void setCurrentPosition(const Point& pos) override;
    Point getCurrentPosition() const override;

    /**
     * @brief 获取推理统计
     */
    HumanoidPPOAgent::InferenceStats getInferenceStats() const;

    // ===== Humanoid 专用接口 =====

    /**
     * @brief 使用完整状态信息规划
     */
    UnifiedTrajectory planWithState(
        const Point& start,
        const Point& goal,
        const HumanoidStateInfo& state_info);

    /**
     * @brief 选择单个动作
     */
    HumanoidAction selectAction(const HumanoidStateInfo& state_info);

    /**
     * @brief 评估状态价值
     */
    double evaluateStateValue(const HumanoidStateInfo& state_info);

    /**
     * @brief 评估位置的数据价值
     */
    DataValueResult evaluateLocationValue(double x, double y,
                                         SceneType scene_type = SceneType::UNKNOWN);

    /**
     * @brief 添加采集的数据点
     */
    void addDataPoint(const DataPointMetadata& metadata);

    /**
     * @brief 报告执行反馈（规划vs实际）
     */
    void reportExecutionFeedback(const Point& planned_waypoint,
                                const Point& actual_position,
                                bool collection_success,
                                double gait_stability = 0.5);

    /**
     * @brief 批量报告执行反馈
     */
    void reportExecutionFeedbackBatch(const std::vector<ExecutionFeedback>& feedback);

    /**
     * @brief 计算状态奖励
     */
    double computeReward(const HumanoidRewardState& reward_state);

    // ===== 配置管理 =====

    bool loadPPOModel(const std::string& model_path);
    void updatePPOConfig(const HumanoidPPOConfig& config);
    void updateRewardConfig(const HumanoidRewardConfig& config);
    void updateValueConfig(const DataValueConfig& config);

    // ===== 获取器 =====

    const HumanoidPPOAgent* getAgent() const { return ppo_agent_.get(); }
    HumanoidPPOAgent* getAgent() { return ppo_agent_.get(); }

    const HumanoidDataValueModel* getDataValueModel() const { return data_value_model_.get(); }
    HumanoidDataValueModel* getDataValueModel() { return data_value_model_.get(); }

    const CostMap* getCostMap() const { return costmap_.get(); }

    const HumanoidPPOConfig& getPPOConfig() const { return ppo_config_; }
    const HumanoidRewardConfig& getRewardConfig() const { return reward_config_; }
    const DataValueConfig& getValueConfig() const { return value_config_; }

    // ===== 状态管理 =====

    void setCurrentState(const HumanoidStateInfo& state) { current_state_ = state; }
    const HumanoidStateInfo& getCurrentState() const { return current_state_; }

    void pushAction(double forward_vel) {
        action_history_[action_history_idx_] = forward_vel;
        action_history_idx_ = (action_history_idx_ + 1) % 8;
    }

    // ===== 统计信息 =====

    double getEpisodeReward() const { return episode_reward_; }
    int getCurrentStep() const { return current_step_; }
    uint64_t getTotalPlans() const { return total_plans_; }

private:
    void updateCostMap();
    bool validateTrajectory(const UnifiedTrajectory& trajectory);
};

} // namespace aurora::planner

#endif // HUMANOID_PLANNER_H
