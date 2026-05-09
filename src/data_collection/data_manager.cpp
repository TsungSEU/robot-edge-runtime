// data_manager.cpp
#include "data_manager.h"
#include "common/log/logger.h"
#include "rl_planning_infer/utils/planner_utils.h"
#include <sstream>
#include <algorithm>

namespace aurora::collector {

DataManager::DataManager(int map_width, int map_height, double resolution)
    : map_width_(map_width)
    , map_height_(map_height)
    , resolution_(resolution) {
    AD_INFO(DataManager, "Creating DataManager with map_size=%dx%d, resolution=%.2f",
            map_width, map_height, resolution);

    // 创建覆盖率指标对象
    coverage_metric_ = std::make_unique<planner::CoverageMetric>();

    AD_INFO(DataManager, "DataManager created successfully");
}

void DataManager::addDataPoint(const DataPoint& data_point) {
    data_points_.push_back(data_point);
    data_point_positions_.push_back(data_point.position);

    AD_DEBUG(DataManager, "Added data point at (%.2f, %.2f), total: %zu",
             data_point.position.x, data_point.position.y, data_points_.size());
}

void DataManager::addDataPoints(const std::vector<DataPoint>& data_points) {
    if (data_points.empty()) {
        AD_WARN(DataManager, "Attempted to add empty data points collection");
        return;
    }

    // 批量添加数据点
    for (const auto& data_point : data_points) {
        data_points_.push_back(data_point);
        data_point_positions_.push_back(data_point.position);
    }

    AD_DEBUG(DataManager, "Added %zu data points, total: %zu",
              data_points.size(), data_points_.size());
}

void DataManager::clear() {
    size_t old_size = data_points_.size();
    data_points_.clear();
    data_point_positions_.clear();
    coverage_metric_->reset();

    AD_INFO(DataManager, "Cleared %zu data points", old_size);
}

void DataManager::updateCostmapWithStatistics(planner::CostMap& costmap) {
    AD_INFO(DataManager, "Updating costmap with %zu data points", data_point_positions_.size());

    if (data_point_positions_.empty()) {
        AD_DEBUG(DataManager, "No data points to update costmap");
        return;
    }

    // 使用数据点位置更新costmap
    costmap.updateWithDataStatistics(data_point_positions_);

    // 根据密度调整成本
    costmap.adjustCostsBasedOnDensity();

    AD_INFO(DataManager, "Costmap updated successfully");
}

void DataManager::updateCoverageMetrics(const planner::CostMap& costmap) {
    AD_INFO(DataManager, "Updating coverage metrics with CostMap");

    if (data_point_positions_.empty()) {
        AD_DEBUG(DataManager, "No data points for coverage calculation");
        return;
    }

    // 将世界坐标转换为网格坐标
    std::vector<std::pair<int, int>> visited_cells;
    for (const auto& point : data_point_positions_) {
        auto grid_coords = planner::PlannerUtils::worldToGrid(point, resolution_);
        visited_cells.push_back(grid_coords);
    }

    // 使用实际的规划器 CostMap 更新覆盖率指标（包含真实的数据密度）
    coverage_metric_->updateCoverage(costmap, visited_cells);

    AD_INFO(DataManager, "Coverage updated - Total: %d, Visited: %d, Sparse: %d/%d, Ratio: %.2f%%",
            coverage_metric_->getTotalCells(),
            coverage_metric_->getVisitedCells(),
            coverage_metric_->getVisitedSparseCells(),
            coverage_metric_->getTotalSparseCells(),
            coverage_metric_->getCoverageRatio() * 100.0);
}

std::string DataManager::getCoverageReport() const {
    std::ostringstream oss;

    oss << "=== Data Coverage Report ===" << std::endl;
    oss << "Total data points: " << data_points_.size() << std::endl;
    oss << "Total cells: " << coverage_metric_->getTotalCells() << std::endl;
    oss << "Visited cells: " << coverage_metric_->getVisitedCells() << std::endl;
    oss << "Coverage ratio: " << (coverage_metric_->getCoverageRatio() * 100.0) << "%" << std::endl;
    oss << "Sparse coverage ratio: " << (coverage_metric_->getSparseCoverageRatio() * 100.0) << "%" << std::endl;
    oss << "============================" << std::endl;

    return oss.str();
}

} // namespace aurora
