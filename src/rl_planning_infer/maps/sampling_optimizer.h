// sampling_optimizer.h
#ifndef SAMPLING_OPTIMIZER_H
#define SAMPLING_OPTIMIZER_H

#include <vector>
#include "costmap.h"
#include "coverage_metric.h"

namespace aurora::planner {

struct SamplingParams {
    double exploration_weight;
    double efficiency_weight;
    double redundancy_penalty;
    double sparse_threshold;
    
    SamplingParams() : exploration_weight(1.0), efficiency_weight(0.5), 
                      redundancy_penalty(0.4), sparse_threshold(0.2) {}
};

class SamplingOptimizer {
private:
    SamplingParams params;
    CoverageMetric coverage_metric;
    
public:
    SamplingOptimizer(const SamplingParams& parameters = SamplingParams());
    
    /**
     * @brief Optimize sampling strategy based on current coverage and costmap
     * @param costmap Current costmap with data density information
     * @param current_position Current position of the vehicle
     * @return Next optimal sampling point
     */
    Point optimizeNextSample(const CostMap& costmap, const Point& current_position);
    
    /**
     * @brief Calculate sampling score for a cell
     * @param costmap Current costmap
     * @param x X-coordinate of the cell
     * @param y Y-coordinate of the cell
     * @param current_x Current x-position
     * @param current_y Current y-position
     * @return Sampling score (higher is better)
     */
    double calculateSamplingScore(const CostMap& costmap, int x, int y, 
                                int current_x, int current_y) const;
    
    /**
     * @brief Find nearest sparse cell from current position
     * @param costmap Current costmap
     * @param current_position Current position
     * @return Position of nearest sparse cell
     */
    Point findNearestSparseCell(const CostMap& costmap, 
                               const Point& current_position) const;
    
    /**
     * @brief Update optimizer parameters
     */
    void updateParameters(const SamplingParams& new_params);
    
    /**
     * @brief Get current coverage metric
     */
    const CoverageMetric& getCoverageMetric() const { return coverage_metric; }
};

} // namespace aurora::planner

#endif // SAMPLING_OPTIMIZER_H