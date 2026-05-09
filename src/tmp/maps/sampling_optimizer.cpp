// sampling_optimizer.cpp
#include "sampling_optimizer.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace aurora::planner {
SamplingOptimizer::SamplingOptimizer(const SamplingParams& parameters)
    : params(parameters), coverage_metric(parameters.sparse_threshold) {
}

Point SamplingOptimizer::optimizeNextSample(const CostMap& costmap, 
                                          const Point& current_position) {
    int width = costmap.getWidth();
    int height = costmap.getHeight();
    
    int current_x = static_cast<int>(current_position.x);
    int current_y = static_cast<int>(current_position.y);
    
    double best_score = -std::numeric_limits<double>::infinity();
    Point best_point(current_position.x, current_position.y);
    
    // Evaluate each cell in the costmap
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Skip invalid cells
            if (!costmap.isValidCell(x, y)) {
                continue;
            }
            
            double score = calculateSamplingScore(costmap, x, y, current_x, current_y);
            
            if (score > best_score) {
                best_score = score;
                best_point = Point(static_cast<double>(x), static_cast<double>(y));
            }
        }
    }
    
    return best_point;
}

double SamplingOptimizer::calculateSamplingScore(const CostMap& costmap, int x, int y, 
                                               int current_x, int current_y) const {
    // Get basic properties of the cell
    double density = costmap.getDataDensity(x, y);
    double cost = costmap.getCellCost(x, y);
    
    // Calculate distance to current position
    double dx = static_cast<double>(x - current_x);
    double dy = static_cast<double>(y - current_y);
    double distance = std::sqrt(dx*dx + dy*dy);
    
    // Base score calculation
    double score = 0.0;
    
    // Reward for sparse areas (encourage exploration)
    if (density < params.sparse_threshold) {
        score += params.exploration_weight * (params.sparse_threshold - density);
    }
    
    // Penalty for high-density areas (discourage redundancy)
    if (density >= params.sparse_threshold) {
        score -= params.redundancy_penalty * (density - params.sparse_threshold);
    }
    
    // Efficiency component - prefer closer cells
    if (distance > 0) {
        score += params.efficiency_weight / distance;  // Closer is better
    }
    
    // Cost component - lower cost is better
    score -= cost;
    
    return score;
}

Point SamplingOptimizer::findNearestSparseCell(const CostMap& costmap, 
                                              const Point& current_position) const {
    int width = costmap.getWidth();
    int height = costmap.getHeight();
    
    int current_x = static_cast<int>(current_position.x);
    int current_y = static_cast<int>(current_position.y);
    
    double min_distance = std::numeric_limits<double>::infinity();
    Point nearest_point(current_position.x, current_position.y);
    
    // Search for nearest sparse cell
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Check if this is a sparse cell
            if (costmap.getDataDensity(x, y) < params.sparse_threshold) {
                double dx = static_cast<double>(x - current_x);
                double dy = static_cast<double>(y - current_y);
                double distance = std::sqrt(dx*dx + dy*dy);
                
                if (distance < min_distance) {
                    min_distance = distance;
                    nearest_point = Point(static_cast<double>(x), static_cast<double>(y));
                }
            }
        }
    }
    
    return nearest_point;
}

void SamplingOptimizer::updateParameters(const SamplingParams& new_params) {
    params = new_params;
    // Update coverage metric with new sparse threshold
    coverage_metric = CoverageMetric(params.sparse_threshold);
}

} // namespace aurora::planner