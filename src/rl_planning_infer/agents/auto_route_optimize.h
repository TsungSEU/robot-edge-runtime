// auto_route_optimize.h - Auto mode route optimizer
#ifndef AUTO_ROUTE_OPTIMIZER_H
#define AUTO_ROUTE_OPTIMIZER_H

#include <vector>
#include <memory>
#include "../maps/costmap.h"
#include "auto_ppo_agent.h"

namespace aurora {
namespace planner {

// Forward declarations
class AutoPPOAgent;
struct AutoMapData {
    int width, height;
    double resolution;

    AutoMapData(int w = 0, int h = 0, double res = 1.0)
        : width(w), height(h), resolution(res) {}

    CostMap toCostMap() const {
        return CostMap(width, height, resolution);
    }
};

struct AutoDataStats {
    double sparse_threshold;
    double exploration_bonus;
    double redundancy_penalty;

    AutoDataStats(double threshold = 0.2, double bonus = 0.5, double penalty = 0.4)
        : sparse_threshold(threshold), exploration_bonus(bonus), redundancy_penalty(penalty) {}

    double dataDensity(const Point& position) const {
        (void)position;
        return 0.0;
    }
};

// Type aliases for backward compatibility
using MapData = AutoMapData;
using DataStats = AutoDataStats;

/**
 * @brief Auto mode route planner
 *
 * Handles path planning for autonomous driving with:
 * - A* algorithm for shortest path
 * - PPO-based RL for data collection optimization
 * - 25-dimensional state space
 * - 4-dimensional discrete action space
 */
class AutoRoutePlanner {
private:
    double threshold_sparse;
    double exploration_bonus;
    double redundancy_penalty;

    std::unique_ptr<AutoPPOAgent> ppo_agent_;

public:
    // Auto mode state dimension (including reachability feature)
    static constexpr int AUTO_STATE_DIM = 25;  // 25-dim state (24 original + 1 reachability)

    AutoRoutePlanner(double sparse_threshold = 0.2,
                    double exploration_bonus = 0.5,
                    double redundancy_penalty = 0.4);

    /**
     * @brief Optimize route based on map data and statistics
     */
    void optRoute(const AutoMapData& map, const AutoDataStats& stats);

    /**
     * @brief Compute path using A* algorithm
     */
    std::vector<Point> computeAStarPath(const CostMap& costmap,
                                       const Point& start,
                                       const Point& goal);

    /**
     * @brief Compute path using PPO-based RL
     */
    std::vector<Point> computePPOPath(const CostMap& costmap,
                                     const Point& start,
                                     const Point& goal);

    // Setters for parameters
    void setSparseThreshold(double threshold) { threshold_sparse = threshold; }
    void setExplorationBonus(double bonus) { exploration_bonus = bonus; }
    void setRedundancyPenalty(double penalty) { redundancy_penalty = penalty; }

    /**
     * @brief Set PPO agent
     */
    void setPPOAgent(std::unique_ptr<AutoPPOAgent> agent) {
        ppo_agent_ = std::move(agent);
    }

    /**
     * @brief Get PPO agent
     */
    AutoPPOAgent* getPPOAgent() {
        return ppo_agent_.get();
    }

    PPOAgent* getPPOAgentLegacy() {
        return ppo_agent_.get();
    }
};

// Type aliases for backward compatibility
using RoutePlanner = AutoRoutePlanner;

} // namespace planner
} // namespace aurora

#endif // AUTO_ROUTE_OPTIMIZER_H
