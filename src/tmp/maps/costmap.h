// costmap.h
#ifndef COSTMAP_H
#define COSTMAP_H

#include <vector>
#include <cmath>
#include <array>


struct Point {
    double x, y;
    Point(double x = 0, double y = 0) : x(x), y(y) {}
};

struct Cell {
    int x, y;
    double cost;
    double data_density;
    
    Cell(int x = 0, int y = 0, double cost = 0.0, double density = 0.0) 
        : x(x), y(y), cost(cost), data_density(density) {}
};

namespace aurora::planner {

/**
 * @brief Execution feedback for planning-execution alignment
 *
 * Tracks how well planned waypoints translate to actual collection points,
 * enabling the planner to learn which areas are practically reachable.
 */
struct ExecutionFeedback {
    Point planned_waypoint;   // The PPO-planned waypoint
    Point actual_position;    // Where the robot actually collected
    bool collection_success;  // Whether collection was triggered
    double position_error;    // Distance between planned and actual
    double gait_stability;    // Stability score at collection time [0-1]

    ExecutionFeedback()
        : planned_waypoint(0, 0), actual_position(0, 0),
          collection_success(false), position_error(0.0), gait_stability(0.0) {}

    ExecutionFeedback(const Point& planned, const Point& actual, bool success,
                     double error, double stability)
        : planned_waypoint(planned), actual_position(actual),
          collection_success(success), position_error(error),
          gait_stability(stability) {}
};

class CostMap {
private:
    std::vector<std::vector<Cell>> cells;
    int width, height;
    double resolution; // meters per cell
    double sparse_threshold;
    double exploration_bonus;
    double redundancy_penalty;

    // Reachability score for each cell [0-1], initialized to 0.5 (unknown)
    std::vector<std::vector<double>> reachability_score_;
    std::vector<std::vector<int>> attempt_count_;
    std::vector<std::vector<int>> success_count_;
    std::vector<std::vector<double>> accumulated_error_;

    // Reachability decay factor: old data has less influence
    double reachability_decay_;
    int min_samples_for_reliability_;
    double max_position_tolerance_;

public:
    CostMap(int width, int height, double resolution);

    void setParameters(double sparse_threshold, double exploration_bonus, double redundancy_penalty);

    void updateWithDataStatistics(const std::vector<Point>& data_points);

    void adjustCostsBasedOnDensity();

    double getDataDensity(int x, int y) const;

    void setCellCost(int x, int y, double cost);

    double getCellCost(int x, int y) const;

    bool isValidCell(int x, int y) const;

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    double getResolution() const { return resolution; }

    // Getters for parameters
    double getSparseThreshold() const { return sparse_threshold; }
    double getExplorationBonus() const { return exploration_bonus; }
    double getRedundancyPenalty() const { return redundancy_penalty; }


    /**
     * @brief Update reachability based on execution feedback
     *
     * When PPO plans a waypoint but collection happens elsewhere,
     * this updates the reachability model to reflect reality.
     *
     * @param planned The PPO-planned waypoint
     * @param actual Where collection actually occurred
     * @param success Whether collection was successful
     * @param stability Gait stability score [0-1]
     */
    void updateReachability(const Point& planned, const Point& actual,
                           bool success, double stability = 0.5);

    /**
     * @brief Batch update reachability from multiple feedback samples
     */
    void updateReachabilityBatch(const std::vector<ExecutionFeedback>& feedback);

    /**
     * @brief Get the effective cost considering both data value AND reachability
     *
     * Effective cost = base_cost / (reachability + epsilon)
     * This penalizes areas that are frequently planned but rarely reached.
     *
     * @param x Cell x coordinate
     * @param y Cell y coordinate
     * @return Effective cost for planning decisions
     */
    double getEffectiveCost(int x, int y) const;

    /**
     * @brief Get reachability score for a cell
     * @return Reachability [0-1], where 1.0 = always reachable
     */
    double getReachabilityScore(int x, int y) const;

    /**
     * @brief Get execution statistics for a cell
     */
    void getExecutionStats(int x, int y, int& attempts, int& successes) const;

    /**
     * @brief Reset reachability tracking (e.g., when environment changes)
     */
    void resetReachability();

    /**
     * @brief Set reachability parameters
     */
    void setReachabilityParameters(double decay, int min_samples, double tolerance);

    /**
     * @brief Check if a cell has reliable reachability data
     */
    bool hasReliableReachability(int x, int y) const;

    /**
     * @brief Add an obstacle at the specified world coordinates
     * @param location World coordinates (x, y) where the obstacle was detected
     * @param radius Obstacle radius in meters (default: 0.5m)
     *
     * Marks cells within the radius as blocked by setting their cost to infinity.
     * This is used when environment changes (NEW_OBSTACLE event) are detected.
     */
    void addObstacle(const Point& location, double radius = 0.5);

    /**
     * @brief Remove an obstacle at the specified world coordinates
     * @param location World coordinates (x, y) where the obstacle was cleared
     * @param radius Obstacle radius in meters (default: 0.5m)
     *
     * Resets cells within the radius to their base cost (removes obstacle marking).
     */
    void removeObstacle(const Point& location, double radius = 0.5);

    /**
     * @brief Check if a cell is blocked by an obstacle
     * @param x Cell x coordinate
     * @param y Cell y coordinate
     * @return true if the cell has infinite cost (blocked), false otherwise
     */
    bool isCellBlocked(int x, int y) const;
};

} // namespace aurora::planner

#endif // COSTMAP_H