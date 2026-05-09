// auto_reward.cpp - Auto mode reward calculator
#include "auto_reward.h"
#include <cmath>

namespace aurora::planner {

double AutoRewardCalculator::computeReward(const AutoStateInfo& prev_state_info,
                                      const AutoStateInfo& new_state_info,
                                      const AutoRewardConfig& config) {
    double reward = 0.0;

    // Distance-based reward
    if (prev_state_info.distance_to_target > new_state_info.distance_to_target) {
        double distance_improvement = prev_state_info.distance_to_target - new_state_info.distance_to_target;
        double normalization_factor = (config.total_path_length > 1.0) ? (100.0 / config.total_path_length) : 1.0;
        reward += config.distance_improvement_scale * distance_improvement * normalization_factor;
    }

    // Time penalty
    reward += config.step_penalty;

    // Goal reward
    if (new_state_info.reached_goal) {
        reward += config.goal_reward;
    }

    // Collision penalty
    if (new_state_info.collision) {
        reward += config.collision_penalty;
    }

    // Data scarcity reward
    if (new_state_info.visited_new_sparse) {
        reward += config.new_sparse_reward;
    }

    // Coverage reward
    if (!new_state_info.visited_before) {
        reward += config.new_area_reward;
    }

    // Path efficiency penalty
    if (!new_state_info.on_efficient_path) {
        reward += config.inefficient_path_penalty;
    }

    // Repetitive path penalty
    if (new_state_info.visited_before && new_state_info.total_visited_count > 3) {
        reward += config.repeat_visit_penalty_factor * (new_state_info.total_visited_count - 3);
    }

    return reward;
}

} // namespace aurora::planner
