// coverage_metric.h
#ifndef COVERAGE_METRIC_H
#define COVERAGE_METRIC_H

#include <vector>
#include "costmap.h"

namespace aurora::planner {

class CoverageMetric {
private:
    int total_cells;
    int visited_cells;
    int total_sparse_cells;
    int visited_sparse_cells;
    double sparse_threshold;

public:
    CoverageMetric(double sparse_threshold = 0.2);
    
    /**
     * @brief Update coverage metrics based on visited cells
     * @param costmap The costmap with data density information
     * @param visited_cells_list List of visited cell coordinates
     */
    void updateCoverage(const CostMap& costmap, 
                       const std::vector<std::pair<int, int>>& visited_cells_list);
    
    /**
     * @brief Calculate grid coverage ratio
     * @return Coverage ratio (visited cells / total cells)
     */
    double getCoverageRatio() const;
    
    /**
     * @brief Calculate sparse sample coverage ratio
     * @return Sparse coverage ratio (visited sparse cells / total sparse cells)
     */
    double getSparseCoverageRatio() const;
    
    /**
     * @brief Get total number of cells
     */
    int getTotalCells() const { return total_cells; }
    
    /**
     * @brief Get number of visited cells
     */
    int getVisitedCells() const { return visited_cells; }
    
    /**
     * @brief Get total number of sparse cells
     */
    int getTotalSparseCells() const { return total_sparse_cells; }
    
    /**
     * @brief Get number of visited sparse cells
     */
    int getVisitedSparseCells() const { return visited_sparse_cells; }
    
    /**
     * @brief Reset metrics
     */
    void reset();
};

} // namespace aurora::planner
#endif // COVERAGE_METRIC_H