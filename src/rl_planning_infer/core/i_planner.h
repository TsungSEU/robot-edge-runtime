//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 TsungX. All rights reserved.
//

#ifndef I_PLANNER_H
#define I_PLANNER_H

#include <vector>
#include <memory>
#include <string>

#include "planner_base.hpp"
#include "../maps/costmap.h"
#include "data_collection/common/types.h"

namespace aurora::planner {

/**
 * @brief 规划器统一接口
 *
 * 面向 DataCollectionPlanner 使用的高层接口。
 * 继承 PlannerBase，添加 DCP 所需的任务级方法。
 * 消除 DCP 中的 if-else 模式切换，所有规划器通过此接口多态调用。
 *
 * 现有 PlannerBase 提供: reset(), plan(), initialize(), loadConfiguration(),
 *                         getMode(), getStats() 等
 * IPlanner 额外提供:     planMission(), updateWithNewData(), getCostMap() 等
 */
class IPlanner : public PlannerBase {
public:
    ~IPlanner() override = default;

    // ===== 任务级规划接口（DCP 使用）=====

    /**
     * @brief 规划数据采集任务路径
     * @param area 任务区域
     * @return 优化后的航路点集合
     */
    virtual std::vector<Point> planMission(const collector::MissionArea& area) = 0;

    /**
     * @brief 用新采集的数据更新规划器状态
     * @param data 新采集的数据点
     */
    virtual void updateWithNewData(const std::vector<collector::DataPoint>& data) = 0;

    // ===== CostMap 访问 =====

    /**
     * @brief 获取 CostMap（供 DataManager 更新覆盖率等）
     */
    virtual CostMap* getCostMap() = 0;

    // ===== 统计与报告 =====

    /**
     * @brief 报告覆盖率指标
     */
    virtual void reportCoverageMetrics() = 0;

    /**
     * @brief 获取平均奖励
     */
    virtual double getAverageReward() const = 0;

    // ===== 位置管理 =====

    /**
     * @brief 设置目标位置
     */
    virtual void setGoalPosition(const Point& goal) = 0;

    /**
     * @brief 设置当前位置
     */
    virtual void setCurrentPosition(const Point& pos) = 0;

    /**
     * @brief 获取当前位置
     */
    virtual Point getCurrentPosition() const = 0;
};

} // namespace aurora::planner

#endif // I_PLANNER_H
