//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#ifndef SECTOR_COMPUTER_H
#define SECTOR_COMPUTER_H

#include <array>
#include <cmath>
#include <algorithm>

#include "../maps/costmap.h"
#include "../maps/humanoid_data_value.h"

namespace aurora::planner {

/**
 * @brief 扇区计算工具
 *
 * 从 HumanoidPlanner 中提取的静态工具方法。
 * 计算围绕机器人位置的 8 方向数据价值扇区和 4 方向障碍物扇区。
 */
class SectorComputer {
public:
    /**
     * @brief 计算 8 方向数据价值扇区
     * @param costmap 代价地图
     * @param model 数据价值模型
     * @param x 机器人 x 坐标
     * @param y 机器人 y 坐标
     * @param theta 机器人朝向 (rad)
     * @param scan_range 扫描范围 (米)
     * @return 8 个方向的数据价值 [0, 1]
     */
    static std::array<double, 8> computeDataValueSectors(
        const CostMap& costmap,
        const HumanoidDataValueModel& model,
        double x, double y, double theta,
        double scan_range = 10.0) {

        std::array<double, 8> sectors{};

        for (int i = 0; i < 8; ++i) {
            double sector_angle = theta + i * (2.0 * M_PI / 8.0);
            double sector_x = x + scan_range * std::cos(sector_angle);
            double sector_y = y + scan_range * std::sin(sector_angle);

            int cell_x = static_cast<int>(sector_x / costmap.getResolution());
            int cell_y = static_cast<int>(sector_y / costmap.getResolution());
            if (costmap.isValidCell(cell_x, cell_y)) {
                double cost = costmap.getEffectiveCost(cell_x, cell_y);
                sectors[i] = std::clamp(1.0 - cost / 5.0, 0.0, 1.0);
            }

            auto result = model.evaluateLocationValue(
                x + 2.0 * std::cos(sector_angle),
                y + 2.0 * std::sin(sector_angle));
            sectors[i] = std::max(sectors[i], result.total_value);
        }

        return sectors;
    }

    /**
     * @brief 计算 4 方向障碍物扇区（射线检测）
     * @param costmap 代价地图
     * @param x 机器人 x 坐标
     * @param y 机器人 y 坐标
     * @param theta 机器人朝向 (rad)
     * @param scan_range 扫描范围 (米)
     * @return 4 个方向的障碍物距离
     */
    static std::array<double, 4> computeObstacleSectors(
        const CostMap& costmap,
        double x, double y, double theta,
        double scan_range = 5.0) {

        std::array<double, 4> sectors;
        sectors.fill(scan_range);

        double angles[4] = {theta, theta + M_PI / 2, theta + M_PI, theta - M_PI / 2};
        double resolution = costmap.getResolution();
        int num_steps = static_cast<int>(scan_range / resolution);

        for (int i = 0; i < 4; ++i) {
            for (int s = 1; s <= num_steps; ++s) {
                double ray_dist = s * resolution;
                double ray_x = x + ray_dist * std::cos(angles[i]);
                double ray_y = y + ray_dist * std::sin(angles[i]);

                int cell_x = static_cast<int>(ray_x / resolution);
                int cell_y = static_cast<int>(ray_y / resolution);

                if (!costmap.isValidCell(cell_x, cell_y)) {
                    sectors[i] = ray_dist;
                    break;
                }

                double cost = costmap.getEffectiveCost(cell_x, cell_y);
                if (cost > 2.0) {
                    sectors[i] = ray_dist;
                    break;
                }
            }
        }

        return sectors;
    }
};

} // namespace aurora::planner

#endif // SECTOR_COMPUTER_H
