// data_manager.h
#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <vector>
#include <memory>
#include <sstream>
#include "common/utils/utils.h"
#include "rl_planning_infer/maps/costmap.h"
#include "rl_planning_infer/maps/coverage_metric.h"
#include "common/types.h"  // For DataPoint, MissionArea

namespace aurora::collector {

/**
 * @brief DataManager - 负责管理数据采集点的存储、统计和CostMap更新
 *
 * 职责：
 * 1. 管理数据点集合
 * 2. 更新CostMap的数据统计
 * 3. 计算和维护覆盖率指标
 *
 * 设计原则：
 * - 单一职责：只负责数据管理，不涉及采集执行
 * - 独立性：可以被其他模块复用
 * - 可测试性：可以独立进行单元测试
 */
class DataManager {
private:
    // 数据点集合
    std::vector<DataPoint> data_points_;

    // 位置点集合（用于CostMap更新）
    std::vector<Point> data_point_positions_;

    // 覆盖率指标
    std::unique_ptr<planner::CoverageMetric> coverage_metric_;

    // 地图尺寸信息
    int map_width_;
    int map_height_;
    double resolution_;

public:
    /**
     * @brief 构造函数
     * @param map_width 地图宽度（网格数量）
     * @param map_height 地图高度（网格数量）
     * @param resolution 分辨率（米/格）
     */
    DataManager(int map_width, int map_height, double resolution);

    ~DataManager() = default;

    // ========== 数据管理接口 ==========

    /**
     * @brief 添加单个数据点
     * @param data_point 数据点对象
     */
    void addDataPoint(const DataPoint& data_point);

    /**
     * @brief 批量添加数据点
     * @param data_points 数据点集合
     */
    void addDataPoints(const std::vector<DataPoint>& data_points);

    /**
     * @brief 获取所有数据点
     * @return 数据点集合的常量引用
     */
    const std::vector<DataPoint>& getDataPoints() const { return data_points_; }

    /**
     * @brief 获取数据点数量
     * @return 数据点总数
     */
    size_t getDataPointCount() const { return data_points_.size(); }

    bool hasDataPoints() const { return !data_point_positions_.empty(); }

    /**
     * @brief 清空所有数据点
     */
    void clear();

    // ========== CostMap更新接口 ==========

    /**
     * @brief 使用数据统计更新CostMap
     * @param costmap 要更新的CostMap引用
     *
     * 此方法会：
     * 1. 使用data_point_positions_更新costmap的数据密度
     * 2. 调用costmap.adjustCostsBasedOnDensity()调整成本
     */
    void updateCostmapWithStatistics(planner::CostMap& costmap);

    // ========== 覆盖率统计接口 ==========

    /**
     * @brief 更新覆盖率指标
     * @param costmap 实际规划器的CostMap（包含数据密度信息）
     *
     * 根据当前数据点位置和CostMap的数据密度计算覆盖率
     */
    void updateCoverageMetrics(const planner::CostMap& costmap);

    /**
     * @brief 获取覆盖率指标
     * @return CoverageMetric对象的常量引用
     */
    const planner::CoverageMetric& getCoverageMetrics() const { return *coverage_metric_; }

    /**
     * @brief 获取覆盖率报告
     * @return 格式化的覆盖率报告字符串
     */
    std::string getCoverageReport() const;
};

} // namespace aurora

#endif // DATA_MANAGER_H
