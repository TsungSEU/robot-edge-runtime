// costmap.cpp
#include "costmap.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace aurora::planner {

// Constants for reachability calculation
constexpr double DEFAULT_REACHABILITY_DECAY = 0.95;
constexpr int DEFAULT_MIN_SAMPLES = 3;
constexpr double DEFAULT_POSITION_TOLERANCE = 0.5;  // 50cm tolerance
constexpr double REACHABILITY_EPSILON = 0.01;       // Avoid division by zero

CostMap::CostMap(int width, int height, double resolution)
    : width(width), height(height), resolution(resolution),
      sparse_threshold(0.2), exploration_bonus(0.5), redundancy_penalty(0.4),
      reachability_decay_(DEFAULT_REACHABILITY_DECAY),
      min_samples_for_reliability_(DEFAULT_MIN_SAMPLES),
      max_position_tolerance_(DEFAULT_POSITION_TOLERANCE) {

    cells.resize(height, std::vector<Cell>(width, Cell(0, 0, 0.0, 0.0)));

    // Initialize execution-aware tracking arrays
    reachability_score_.resize(height, std::vector<double>(width, 0.5));  // Start with unknown (0.5)
    attempt_count_.resize(height, std::vector<int>(width, 0));
    success_count_.resize(height, std::vector<int>(width, 0));
    accumulated_error_.resize(height, std::vector<double>(width, 0.0));

    // Initialize cell coordinates
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            cells[y][x] = Cell(x, y, 0.0, 0.0);
        }
    }
}

void CostMap::setParameters(double sparse_threshold, double exploration_bonus, double redundancy_penalty) {
    this->sparse_threshold = sparse_threshold;
    this->exploration_bonus = exploration_bonus;
    this->redundancy_penalty = redundancy_penalty;
}

void CostMap::updateWithDataStatistics(const std::vector<Point>& data_points) {
    // Reset data density
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            cells[y][x].data_density = 0.0;
        }
    }
    
    // Calculate density based on data points
    for (const auto& point : data_points) {
        int cell_x = static_cast<int>(point.x / resolution);
        int cell_y = static_cast<int>(point.y / resolution);
        
        if (isValidCell(cell_x, cell_y)) {
            cells[cell_y][cell_x].data_density += 1.0;
        }
    }
    
    // Normalize densities (optional)
    double max_density = 0.0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            max_density = std::max(max_density, cells[y][x].data_density);
        }
    }
    
    if (max_density > 0.0) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                cells[y][x].data_density /= max_density;
            }
        }
    }
}

void CostMap::adjustCostsBasedOnDensity() {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Adjust cost based on data density
            // Lower cost for sparse areas (encourage exploration)
            // Higher cost for dense areas (discourage redundancy)
            if (cells[y][x].data_density < sparse_threshold) {
                cells[y][x].cost -= exploration_bonus;
            } else {
                cells[y][x].cost += redundancy_penalty;
            }
        }
    }
}

double CostMap::getDataDensity(int x, int y) const {
    if (!isValidCell(x, y)) {
        return 0.0;
    }
    return cells[y][x].data_density;
}

void CostMap::setCellCost(int x, int y, double cost) {
    if (isValidCell(x, y)) {
        cells[y][x].cost = cost;
    }
}

double CostMap::getCellCost(int x, int y) const {
    if (!isValidCell(x, y)) {
        return 0.0;
    }
    return cells[y][x].cost;
}

bool CostMap::isValidCell(int x, int y) const {
    return (x >= 0 && x < width && y >= 0 && y < height);
}

void CostMap::updateReachability(const Point& planned, const Point& actual,
                                 bool success, double stability) {
    // Convert world coordinates to cell coordinates
    int planned_x = static_cast<int>(planned.x / resolution);
    int planned_y = static_cast<int>(planned.y / resolution);

    if (!isValidCell(planned_x, planned_y)) {
        return;  // Out of bounds
    }

    // Calculate position error
    double dx = planned.x - actual.x;
    double dy = planned.y - actual.y;
    double position_error = std::sqrt(dx * dx + dy * dy);

    // Decay existing reachability score
    reachability_score_[planned_y][planned_x] *= reachability_decay_;

    // Increment attempt count
    attempt_count_[planned_y][planned_x]++;

    // Check if the actual position is within tolerance
    bool within_tolerance = (position_error <= max_position_tolerance_);

    // Update reachability score based on execution outcome
    double current_score = reachability_score_[planned_y][planned_x];

    if (success && within_tolerance) {
        // Success: increase reachability
        success_count_[planned_y][planned_x]++;
        // Score moves toward 1.0, weighted by stability
        double increase = 0.2 * stability;
        reachability_score_[planned_y][planned_x] = std::min(1.0, current_score + increase);
    } else if (success && !within_tolerance) {
        // Success but far from planned: moderate increase
        success_count_[planned_y][planned_x]++;
        // Penalty for large position error
        double error_penalty = position_error / max_position_tolerance_;
        double increase = 0.1 * stability / (1.0 + error_penalty);
        reachability_score_[planned_y][planned_x] = std::min(1.0, current_score + increase);
    } else {
        // Failure: decrease reachability
        double decrease = 0.15;
        reachability_score_[planned_y][planned_x] = std::max(0.1, current_score - decrease);
    }

    // Update accumulated error (exponential moving average)
    double& avg_error = accumulated_error_[planned_y][planned_x];
    int attempts = attempt_count_[planned_y][planned_x];
    avg_error = (avg_error * (attempts - 1) + position_error) / attempts;
}

void CostMap::updateReachabilityBatch(const std::vector<ExecutionFeedback>& feedback) {
    for (const auto& fb : feedback) {
        updateReachability(fb.planned_waypoint, fb.actual_position,
                          fb.collection_success, fb.gait_stability);
    }
}

double CostMap::getEffectiveCost(int x, int y) const {
    if (!isValidCell(x, y)) {
        return std::numeric_limits<double>::infinity();  // Invalid cells have infinite cost
    }

    double base_cost = cells[y][x].cost;
    double reachability = reachability_score_[y][x];

    // Effective cost = base_cost / reachability
    // Low reachability → high effective cost (penalty for hard-to-reach areas)
    // High reachability → low effective cost (preference for reachable areas)
    return base_cost / (reachability + REACHABILITY_EPSILON);
}

double CostMap::getReachabilityScore(int x, int y) const {
    if (!isValidCell(x, y)) {
        return 0.0;
    }
    return reachability_score_[y][x];
}

void CostMap::getExecutionStats(int x, int y, int& attempts, int& successes) const {
    if (!isValidCell(x, y)) {
        attempts = 0;
        successes = 0;
        return;
    }
    attempts = attempt_count_[y][x];
    successes = success_count_[y][x];
}

void CostMap::resetReachability() {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            reachability_score_[y][x] = 0.5;  // Reset to unknown
            attempt_count_[y][x] = 0;
            success_count_[y][x] = 0;
            accumulated_error_[y][x] = 0.0;
        }
    }
}

void CostMap::setReachabilityParameters(double decay, int min_samples, double tolerance) {
    reachability_decay_ = std::clamp(decay, 0.8, 0.99);
    min_samples_for_reliability_ = std::max(1, min_samples);
    max_position_tolerance_ = std::max(0.1, tolerance);
}

bool CostMap::hasReliableReachability(int x, int y) const {
    if (!isValidCell(x, y)) {
        return false;
    }
    return attempt_count_[y][x] >= min_samples_for_reliability_;
}

void CostMap::addObstacle(const Point& location, double radius) {
    // Convert world coordinates to cell coordinates
    int center_x = static_cast<int>(location.x / resolution);
    int center_y = static_cast<int>(location.y / resolution);

    // Calculate the radius in cells (add 1 for safety margin)
    int radius_cells = static_cast<int>(std::ceil(radius / resolution)) + 1;

    // Mark all cells within the radius as blocked
    int blocked_count = 0;
    for (int dy = -radius_cells; dy <= radius_cells; dy++) {
        for (int dx = -radius_cells; dx <= radius_cells; dx++) {
            int cell_x = center_x + dx;
            int cell_y = center_y + dy;

            if (isValidCell(cell_x, cell_y)) {
                // Check if this cell is within the radius (circular obstacle)
                double dist = std::sqrt(dx * dx + dy * dy) * resolution;
                if (dist <= radius) {
                    // Mark cell as blocked by setting cost to infinity
                    cells[cell_y][cell_x].cost = std::numeric_limits<double>::infinity();
                    blocked_count++;
                }
            }
        }
    }
}

void CostMap::removeObstacle(const Point& location, double radius) {
    // Convert world coordinates to cell coordinates
    int center_x = static_cast<int>(location.x / resolution);
    int center_y = static_cast<int>(location.y / resolution);

    // Calculate the radius in cells (add 1 for safety margin)
    int radius_cells = static_cast<int>(std::ceil(radius / resolution)) + 1;

    // Reset all cells within the radius to base cost
    int cleared_count = 0;
    for (int dy = -radius_cells; dy <= radius_cells; dy++) {
        for (int dx = -radius_cells; dx <= radius_cells; dx++) {
            int cell_x = center_x + dx;
            int cell_y = center_y + dy;

            if (isValidCell(cell_x, cell_y)) {
                // Check if this cell is within the radius (circular obstacle)
                double dist = std::sqrt(dx * dx + dy * dy) * resolution;
                if (dist <= radius) {
                    // Reset cell to base cost (0.0)
                    // The cost will be adjusted by adjustCostsBasedOnDensity() if needed
                    cells[cell_y][cell_x].cost = 0.0;
                    cleared_count++;
                }
            }
        }
    }
}

bool CostMap::isCellBlocked(int x, int y) const {
    if (!isValidCell(x, y)) {
        return true;  // Out of bounds cells are considered blocked
    }
    // Check if cell has infinite cost (blocked by obstacle)
    return std::isinf(cells[y][x].cost);
}

} // namespace aurora::planner