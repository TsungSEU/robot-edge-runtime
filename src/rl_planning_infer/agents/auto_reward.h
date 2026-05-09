// auto_reward.h - Auto mode reward calculator
#ifndef AUTO_REWARD_H
#define AUTO_REWARD_H

#include <vector>

/**
 * @brief Auto mode state information for reward calculation
 */
struct AutoStateInfo {
    bool visited_new_sparse;        // Visited new sparse area
    bool trigger_success;           // Data collection success
    bool collision;                 // Collision
    bool reached_goal;              // Reached goal
    bool on_efficient_path;         // On efficient path
    bool visited_before;            // Repeat visit
    double distance_to_sparse;      // Distance to sparse area
    double distance_to_target;      // Distance to target
    double path_efficiency;         // Path efficiency
    int steps_taken;                // Steps taken
    int total_visited_count;        // Total visit count

    AutoStateInfo() : visited_new_sparse(false), trigger_success(false),
                   collision(false), reached_goal(false),
                   on_efficient_path(true), visited_before(false),
                   distance_to_sparse(0.0), distance_to_target(0.0),
                   path_efficiency(1.0), steps_taken(0), total_visited_count(1) {}
};

// Type alias for backward compatibility
using StateInfo = AutoStateInfo;

namespace aurora::planner {

/**
 * @brief Auto mode reward configuration
 */
struct AutoRewardConfig {
    double distance_improvement_scale = 5.0;
    double step_penalty = -0.01;
    double goal_reward = 50.0;
    double collision_penalty = -50.0;
    double new_sparse_reward = 10.0;
    double new_area_reward = 2.0;
    double inefficient_path_penalty = -5.0;
    double repeat_visit_penalty_factor = -2.0;
    double total_path_length = 100.0;
};

// Type alias for backward compatibility
using RewardConfig = AutoRewardConfig;

/**
 * @brief Auto mode reward calculator
 *
 * Computes rewards for autonomous driving navigation based on:
 * - Distance improvement
 * - Sparse area discovery
 * - Goal achievement
 * - Collision avoidance
 * - Path efficiency
 */
class AutoRewardCalculator {
public:
    /**
     * @brief Compute reward based on state transitions
     * @param prev_state_info Previous state information
     * @param new_state_info New state information
     * @param config Reward configuration
     * @return Computed reward value
     */
    static double computeReward(const AutoStateInfo& prev_state_info,
                               const AutoStateInfo& new_state_info,
                               const AutoRewardConfig& config = AutoRewardConfig());
};

// Type alias for backward compatibility
using RewardCalculator = AutoRewardCalculator;

} // namespace aurora::planner

#endif // AUTO_REWARD_H
