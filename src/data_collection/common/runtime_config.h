//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <string>
#include <chrono>

namespace aurora::config {

/**
 * @brief 数据采集参数
 *
 * 控制何时触发数据采集、如何判定 waypoint 到达等
 */
struct CollectionParams {
    double min_step_distance = 0.15;           // 最小采集步长 (米)
    double min_collection_interval = 0.5;      // 最小采集间隔 (秒)
    double position_tolerance = 0.5;           // 位置匹配容差 (米)
    double waypoint_reach_tolerance = 0.3;     // waypoint 到达判定容差 (米)
    std::chrono::milliseconds waypoint_sleep{100};  // waypoint 间等待
    double waypoint_wait_timeout = 30.0;       // waypoint 等待超时 (秒)
    double grid_resolution = 0.5;              // 网格分辨率 (米) - 从配置文件 common.grid_resolution 读取
    size_t upload_threshold = 1000;            // 上传阈值 (经验条数) - 从配置文件读取
};

/**
 * @brief 速度控制参数
 *
 * 用于 velocity_cmd 模式下的机器人运动控制
 */
struct VelocityControlParams {
    double max_forward = 1.0;     // 最大前进速度 (m/s)
    double max_lateral = 0.0;     // 最大横向速度 (m/s) — 当前未使用
    double max_angular = 1.5;     // 最大角速度 (rad/s)
    double kp_angular = 3.0;      // 角速度比例增益
    double control_period_ms = 50.0;  // 控制周期 (ms)
    double waypoint_tolerance = 0.5;  // 速度模式下 waypoint 到达容差 (米)
};

/**
 * @brief 运行时全局配置
 *
 * 集中管理所有运行时参数，替代散落在各处的魔法数字
 */
struct RuntimeConfig {
    // 子系统配置
    CollectionParams collection;
    VelocityControlParams velocity_control;

    // 路径配置
    std::string model_path;       // ONNX 模型文件路径
    std::string config_path = "config/planner_weights.yaml";  // 训练配置路径

    // 运行模式
    std::string action_type = "path_tracking";  // "path_tracking" 或 "velocity_cmd"

    // 可视化
    bool visualization_enabled = true;
    std::string visualization_frame = "odom";
    double trail_publish_interval_sec = 0.2;   // 轨迹发布间隔 (秒)，默认 0.2 (5Hz)

    // 主循环
    int max_cycles = 10;
    int buffer_warmup_sec = 10;

    /**
     * @brief 从命令行参数解析配置
     */
    static RuntimeConfig fromArgs(int argc, char** argv);
};

} // namespace aurora::config

#endif // RUNTIME_CONFIG_H
