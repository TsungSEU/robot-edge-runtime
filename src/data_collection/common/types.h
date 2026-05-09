//
// Created by aurora on 2025-03-11.
// Copyright (c) 2025 T3CAIC. All rights reserved.
//

#ifndef DATA_COLLECTION_TYPES_H
#define DATA_COLLECTION_TYPES_H

#include <string>
#include <vector>
#include "rl_planning_infer/maps/costmap.h"  // For Point type

namespace aurora::collector {

/**
 * @brief 数据采集点结构
 *
 * 表示单个数据采集点的位置、内容和时间戳
 */
struct DataPoint {
    Point position;              // 位置坐标
    std::string sensor_data;     // 传感器数据内容
    double timestamp;            // 时间戳（秒）

    DataPoint(const Point& pos = Point(),
              const std::string& data = "", double time = 0.0)
        : position(pos), sensor_data(data), timestamp(time) {}
};

/**
 * @brief 任务区域结构
 *
 * 定义数据采集任务的地理区域范围
 */
struct MissionArea {
    Point center;                // 区域中心坐标
    double radius;               // 区域半径（米）

    MissionArea(const Point& c = Point(), double r = 0.0)
        : center(c), radius(r) {}
};

} // namespace aurora::collector

#endif // DATA_COLLECTION_TYPES_H
