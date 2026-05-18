//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#ifndef APPLICATION_RUNNER_H
#define APPLICATION_RUNNER_H

#include <memory>
#include <string>
#include <csignal>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/executors/multi_threaded_executor.hpp>

#include "data_collection_planner.h"
#include "state_machine/state_machine.h"
#include "aurora_edge_runtime/srv/set_target_path.hpp"
#include "aurora_edge_runtime/srv/get_current_position.hpp"
#include "aurora_edge_runtime/srv/get_error_statistics.hpp"
#include "aurora_edge_runtime/srv/clear_errors.hpp"
#include "common/startup_logger.h"
#include "common/sensor_config_manager.h"
#include "common/log/structured_log.h"
#include "common/audit/audit_logger.h"

namespace aurora {

/**
 * @brief 应用运行器
 *
 * 从 main.cpp 中提取的初始化、主循环、关闭逻辑。
 * main.cpp 仅保留信号处理和 ApplicationRunner 调用。
 */
class ApplicationRunner {
public:
    explicit ApplicationRunner(const config::RuntimeConfig& config);
    ~ApplicationRunner();

    /**
     * @brief 初始化所有子系统
     * @return true 成功, false 失败
     */
    bool initialize();

    /**
     * @brief 运行主循环（计划→执行→上传→反馈）
     * @return 退出码
     */
    int run();

    /**
     * @brief 优雅关闭
     */
    void shutdown();

    /**
     * @brief 请求关闭（由信号处理器调用）
     */
    void requestShutdown() { shutdown_requested_ = true; }

private:
    // 带超时的服务调用
    template<typename ServiceT>
    typename std::shared_ptr<typename ServiceT::Response>
    callService(typename rclcpp::Client<ServiceT>::SharedPtr client,
                typename std::shared_ptr<typename ServiceT::Request> request,
                int timeout_ms = 5000);

    config::RuntimeConfig config_;
    std::shared_ptr<DataCollectionPlanner> dcp_;
    rclcpp::executors::MultiThreadedExecutor* executor_ = nullptr;

    // Service clients
    rclcpp::Client<aurora_edge_runtime::srv::SetTargetPath>::SharedPtr set_path_client_;
    rclcpp::Client<aurora_edge_runtime::srv::GetCurrentPosition>::SharedPtr get_position_client_;
    rclcpp::Client<aurora_edge_runtime::srv::GetErrorStatistics>::SharedPtr get_errors_client_;
    rclcpp::Client<aurora_edge_runtime::srv::ClearErrors>::SharedPtr clear_errors_client_;

    bool use_robot_services_ = true;
    bool shutdown_requested_ = false;
};

} // namespace aurora

#endif // APPLICATION_RUNNER_H
