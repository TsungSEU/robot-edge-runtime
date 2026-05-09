// coverage_metric.cpp
#include "coverage_metric.h"
#include <algorithm>

namespace aurora::planner {
CoverageMetric::CoverageMetric(double sparse_threshold)
    : total_cells(0), visited_cells(0), total_sparse_cells(0), 
      visited_sparse_cells(0), sparse_threshold(sparse_threshold) {
}

void CoverageMetric::updateCoverage(const CostMap& costmap, 
                                   const std::vector<std::pair<int, int>>& visited_cells_list) {
    // Reset counters
    visited_cells = 0;
    visited_sparse_cells = 0;
    
    // Get map dimensions
    int width = costmap.getWidth();
    int height = costmap.getHeight();
    total_cells = width * height;
    total_sparse_cells = 0;
    
    // Count total sparse cells
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (costmap.getDataDensity(x, y) < sparse_threshold) {
                total_sparse_cells++;
            }
        }
    }
    
    // Track visited cells using a 2D boolean array
    std::vector<std::vector<bool>> is_visited(height, std::vector<bool>(width, false));
    
    // Mark visited cells
    for (const auto& cell : visited_cells_list) {
        int x = cell.first;
        int y = cell.second;
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            is_visited[y][x] = true;
        }
    }
    
    // Count visited cells and visited sparse cells
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (is_visited[y][x]) {
                visited_cells++;
                
                // Check if this is a sparse cell
                if (costmap.getDataDensity(x, y) < sparse_threshold) {
                    visited_sparse_cells++;
                }
            }
        }
    }
}

double CoverageMetric::getCoverageRatio() const {
    if (total_cells == 0) {
        return 0.0;
    }
    return static_cast<double>(visited_cells) / total_cells;
}

double CoverageMetric::getSparseCoverageRatio() const {
    if (total_sparse_cells == 0) {
        return 0.0;
    }
    return static_cast<double>(visited_sparse_cells) / total_sparse_cells;
}

void CoverageMetric::reset() {
    total_cells = 0;
    visited_cells = 0;
    total_sparse_cells = 0;
    visited_sparse_cells = 0;
}
}