//
// Created by Aurora Refactoring on 2026/4/24.
// Copyright (c) 2026 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
//

#include "application_runner.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <yaml-cpp/yaml.h>

#include "common/log/logger.h"

namespace aurora {

using namespace aurora::state_machine;
using namespace aurora::planner;
using namespace aurora::common;

ApplicationRunner::ApplicationRunner(const config::RuntimeConfig& config)
    : config_(config) {}

ApplicationRunner::~ApplicationRunner() = default;

bool ApplicationRunner::initialize() {
    // 1. 启动日志
    StartupMode startup_mode = StartupMode::CLI;
    const char* env_mode = std::getenv("AER_ENV");
    if (env_mode && strcmp(env_mode, "prod") == 0) {
        startup_mode = StartupMode::PROD;
    }

    StartupLogger& startup_logger = StartupLogger::instance();
    startup_logger.init(startup_mode);

    SensorConfigManager& sensor_mgr = SensorConfigManager::instance();
    if (!sensor_mgr.loadFromConfig("config/sensor_config.json")) {
        sensor_mgr.autoDetect();
    }
    SensorConfig sensors = sensor_mgr.getSensorConfig();
    startup_logger.setSensorConfig(sensors);

    SystemInfo info;
    info.module_name = "Aurora-Edge-Runtime";
    info.version = MODULE_VERSION;
    info.git_commit = GIT_COMMIT_HASH;
    info.build_time = BUILD_TIMESTAMP;
    info.environment = (startup_mode == StartupMode::PROD) ? "PROD" : "CLI";
    info.ros2_version = "ROS2 Humble";
    info.sensors = sensors;
    startup_logger.printStartupBanner(info);

    // 2. 日志系统
    LOG_INIT_STAGE_BEGIN(INIT_CONFIG);
    const char* log_path = std::getenv("AER_LOG_PATH");
    const char* csv_log_path = std::getenv("AER_CSV_LOG_PATH");
    std::string log_file = log_path ? log_path : "/tmp/aer.log";
    std::string csv_log_file = csv_log_path ? csv_log_path : "/tmp/aer.csv";

    Logger::instance()->Init(LOG_TO_CONSOLE | LOG_TO_FILE, LOG_LEVEL_INFO,
                             log_file.c_str(), csv_log_file.c_str());

    std::string jsonl_log_file = log_file;
    jsonl_log_file.replace(jsonl_log_file.rfind(".log"), 4, ".jsonl");
    StructuredLogWriter::instance().init(jsonl_log_file);

    std::string audit_log_file = log_file;
    audit_log_file.replace(audit_log_file.rfind(".log"), 4, ".audit.jsonl");
    audit::AuditLogger::instance().init(audit_log_file);

    rclcpp::init(0, nullptr);
    LOG_INIT_STAGE_END(INIT_CONFIG, true, "Logger and ROS2 initialized");

    // 3. 验证模型文件
    if (config_.model_path.empty()) {
        startup_logger.logError("No model path specified");
        return false;
    }
    std::ifstream model_file(config_.model_path);
    if (!model_file.good()) {
        std::cerr << "[ERROR] Model file not found: " << config_.model_path << std::endl;
        return false;
    }
    model_file.close();

    // 4. 创建 DCP
    LOG_INIT_STAGE_BEGIN(INIT_SENSOR);
    dcp_ = std::make_shared<DataCollectionPlanner>(config_);

    StateMachine& sm = StateMachine::getInstance();
    if (!sm.initialize()) {
        AD_ERROR(AppRunner, "Failed to initialize State Machine");
        return false;
    }

    if (!dcp_->initialize()) {
        AD_ERROR(AppRunner, "Failed to initialize DCP");
        return false;
    }
    LOG_INIT_STAGE_END(INIT_SENSOR, true, "DCP initialized");

    // 5. 状态机回调
    {
        state_machine::ActionCallbacks actions;
        actions.plan = [this]() -> bool {
            auto path = dcp_->planMission();
            return !path.empty();
        };
        actions.should_collect = []() -> bool { return true; };
        actions.collect = []() -> bool { return true; };
        actions.upload = [this]() -> bool {
            dcp_->uploadCollectedData();
            return true;
        };
        actions.on_shutdown = [this]() {
            AD_INFO(AppRunner, "State machine triggered shutdown");
            dcp_.reset();
        };
        sm.setActionCallbacks(actions);
        sm.setAuditLogCallback([](SystemState from, SystemState to,
                                   StateEvent event,
                                   const std::chrono::steady_clock::time_point&) {
            AD_INFO(AppRunner, "[AUDIT] %s -> %s (event: %s)",
                    StateMachine::stateToString(from),
                    StateMachine::stateToString(to),
                    StateMachine::eventToString(event));
        });
    }

    // 6. 连接机器人服务
    LOG_INIT_STAGE_BEGIN(INIT_SYNC);
    set_path_client_ = dcp_->create_client<aurora_edge_runtime::srv::SetTargetPath>("/robot/set_path");
    get_position_client_ = dcp_->create_client<aurora_edge_runtime::srv::GetCurrentPosition>("/robot/get_position");
    get_errors_client_ = dcp_->create_client<aurora_edge_runtime::srv::GetErrorStatistics>("/robot/get_errors");
    clear_errors_client_ = dcp_->create_client<aurora_edge_runtime::srv::ClearErrors>("/robot/clear_errors");

    {
        bool all_available = true;
        auto check_service = [&all_available](auto client, const char* name) -> bool {
            if (!client->wait_for_service(std::chrono::seconds(1))) {
                std::cerr << "[WARN] Service " << name << " not available" << std::endl;
                all_available = false;
                return false;
            }
            return true;
        };

        check_service(set_path_client_, "/robot/set_path");
        check_service(get_position_client_, "/robot/get_position");
        check_service(get_errors_client_, "/robot/get_errors");
        check_service(clear_errors_client_, "/robot/clear_errors");

        if (!all_available) {
            if (config_.action_type == "velocity_cmd") {
                std::cerr << "[WARN] Robot services not available, using velocity_cmd mode with odom" << std::endl;
                use_robot_services_ = false;
            } else {
                std::cerr << "[ERROR] Robot services not available and action_type is "
                          << config_.action_type << " (requires robot_sim)" << std::endl;
                return false;
            }
        }
    }
    LOG_INIT_STAGE_END(INIT_SYNC, true,
        use_robot_services_ ? "Robot services connected" : "velocity_cmd mode (no robot_sim)");

    // 7. 启动 ROS2 Executor
    LOG_INIT_STAGE_BEGIN(INIT_ENCODER);
    static rclcpp::executors::MultiThreadedExecutor executor(
        rclcpp::ExecutorOptions(), 4);
    executor.add_node(dcp_);
    executor_ = &executor;

    std::thread spin_thread([&executor]() { executor.spin(); });
    spin_thread.detach();
    LOG_INIT_STAGE_END(INIT_ENCODER, true, "ROS2 Executor started");

    // 8. 缓冲区预热
    LOG_INIT_STAGE_BEGIN(INIT_RECORDER);
    for (int i = config_.buffer_warmup_sec; i > 0; --i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    LOG_INIT_STAGE_END(INIT_RECORDER, true, "Buffer warmup completed");

    // 9. 设置任务区域
    PRINT_SYSTEM_READY();

    auto pos_response = callService<aurora_edge_runtime::srv::GetCurrentPosition>(
        get_position_client_,
        std::make_shared<aurora_edge_runtime::srv::GetCurrentPosition::Request>(), 5000);

    Point robot_pos(0.0, 0.0);
    if (pos_response) {
        robot_pos = Point(pos_response->position.x, pos_response->position.y);
    } else {
        robot_pos = dcp_->getPositionTracker().getCurrentPosition();
        std::cout << "[INFO] Using odom position: (" << robot_pos.x << ", " << robot_pos.y << ")" << std::endl;
    }

    MissionArea mission(robot_pos, 5.0);
    dcp_->setMissionArea(mission);

    return true;
}

int ApplicationRunner::run() {
    int cycle = 0;

    while (rclcpp::ok() && cycle < config_.max_cycles && !shutdown_requested_) {
        cycle++;
        std::cout << "\n=== Cycle " << cycle << " / " << config_.max_cycles << " ===" << std::endl;

        // 1. 规划
        auto collection_path = dcp_->planMission();
        if (collection_path.empty()) {
            AD_WARN(AppRunner, "No valid path planned");
            collection_path = {Point(1,1), Point(3,1), Point(5,1), Point(7,1), Point(9,1)};
        }

        // 2. 获取当前位置并调整路径
        auto pos_response = callService<aurora_edge_runtime::srv::GetCurrentPosition>(
            get_position_client_,
            std::make_shared<aurora_edge_runtime::srv::GetCurrentPosition::Request>(), 3000);
        if (pos_response) {
            Point current(pos_response->position.x, pos_response->position.y);
            collection_path.insert(collection_path.begin(), current);
        }

        // 3. 设置机器人控制模式
        if (config_.action_type == "velocity_cmd") {
            // velocity mode — 不需要 set_path
        } else {
            // path_tracking — 发送路径到模拟器
            auto set_path_req = std::make_shared<aurora_edge_runtime::srv::SetTargetPath::Request>();
            for (const auto& pt : collection_path) {
                geometry_msgs::msg::Point wp;
                wp.x = pt.x; wp.y = pt.y; wp.z = 0.0;
                set_path_req->waypoints.push_back(wp);
            }
            callService<aurora_edge_runtime::srv::SetTargetPath>(
                set_path_client_, set_path_req, 3000);
        }

        // 4. 执行采集
        dcp_->executeMission(collection_path);

        // 5. 上传
        const char* enable_upload = std::getenv("AER_ENABLE_UPLOAD");
        if (enable_upload && std::string(enable_upload) == "true") {
            dcp_->uploadCollectedData();
        }

        // 6. 报告指标
        dcp_->reportCoverageMetrics();
        auto stats = dcp_->getStats();
        std::cout << "  Reward avg: " << stats.average_reward
                  << " samples: " << stats.count << std::endl;

        // 7. 位置和误差统计
        auto pos_resp = callService<aurora_edge_runtime::srv::GetCurrentPosition>(
            get_position_client_,
            std::make_shared<aurora_edge_runtime::srv::GetCurrentPosition::Request>(), 3000);
        if (pos_resp) {
            std::cout << "  Robot pos: (" << std::fixed << std::setprecision(2)
                      << pos_resp->position.x << ", " << pos_resp->position.y << ")" << std::endl;
        }

        auto err_resp = callService<aurora_edge_runtime::srv::GetErrorStatistics>(
            get_errors_client_,
            std::make_shared<aurora_edge_runtime::srv::GetErrorStatistics::Request>(), 3000);
        if (err_resp && err_resp->total_waypoints > 0) {
            std::cout << "  Path error: avg=" << std::fixed << std::setprecision(3)
                      << err_resp->average_error << "m max=" << err_resp->max_error << "m" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // 最终统计
    dcp_->reportCoverageMetrics();
    auto final_stats = dcp_->getStats();
    std::cout << "\n=== Final: avg_reward=" << final_stats.average_reward
              << " cycles=" << cycle << " ===" << std::endl;

    return 0;
}

void ApplicationRunner::shutdown() {
    if (executor_) {
        executor_->cancel();
    }
    dcp_.reset();
    rclcpp::shutdown();
    Logger::instance()->Uninit();
}

template<typename ServiceT>
typename std::shared_ptr<typename ServiceT::Response>
ApplicationRunner::callService(
    typename rclcpp::Client<ServiceT>::SharedPtr client,
    typename std::shared_ptr<typename ServiceT::Request> request,
    int timeout_ms) {

    if (!client || !client->service_is_ready()) {
        return nullptr;
    }

    auto future = client->async_send_request(request);
    if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
        return nullptr;
    }

    auto response = future.get();
    if (!response->success) {
        return nullptr;
    }
    return response;
}

} // namespace aurora
