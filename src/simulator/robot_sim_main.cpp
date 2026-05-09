// robot_sim_main.cpp - Standalone Robot Simulator executable
// This runs the robot simulator as a separate process with ROS2 services

#include "robot_simulator_v2.h"
#include "aurora_edge_runtime/srv/set_target_path.hpp"
#include "aurora_edge_runtime/srv/get_current_position.hpp"
#include "aurora_edge_runtime/srv/get_error_statistics.hpp"
#include "aurora_edge_runtime/srv/clear_errors.hpp"
#include <rclcpp/executors/multi_threaded_executor.hpp>

using namespace aurora::sim;

/**
 * @brief Service node wrapper that provides ROS2 services for RobotSimulatorV2
 */
class RobotServiceNode : public rclcpp::Node {
public:
    RobotServiceNode(std::shared_ptr<RobotSimulatorV2> robot_sim)
        : rclcpp::Node("robot_service_node"), robot_sim_(robot_sim) {

        // SetTargetPath service
        set_path_service_ = this->create_service<aurora_edge_runtime::srv::SetTargetPath>(
            "/robot/set_path",
            [this](const std::shared_ptr<aurora_edge_runtime::srv::SetTargetPath::Request> request,
                   std::shared_ptr<aurora_edge_runtime::srv::SetTargetPath::Response> response) {
                std::vector<std::pair<double, double>> path;
                for (const auto& pt : request->waypoints) {
                    path.push_back({pt.x, pt.y});
                }
                robot_sim_->setTargetPath(path);
                response->success = true;
                response->message = "Path set with " + std::to_string(path.size()) + " waypoints";
                RCLCPP_INFO(this->get_logger(), "SetTargetPath service called: %zu waypoints", path.size());
            }
        );

        // GetCurrentPosition service
        get_position_service_ = this->create_service<aurora_edge_runtime::srv::GetCurrentPosition>(
            "/robot/get_position",
            [this](const std::shared_ptr<aurora_edge_runtime::srv::GetCurrentPosition::Request> request,
                   std::shared_ptr<aurora_edge_runtime::srv::GetCurrentPosition::Response> response) {
                auto pos = robot_sim_->getCurrentPosition();
                response->success = true;
                response->position.x = pos[0];
                response->position.y = pos[1];
                response->position.z = pos[2];
                // Note: We don't have access to robot_yaw_ from here, so we'll skip it
                response->yaw = 0.0;
                response->message = "Position retrieved successfully";
                RCLCPP_DEBUG(this->get_logger(), "GetPosition service called: (%.2f, %.2f, %.2f)",
                            pos[0], pos[1], pos[2]);
            }
        );

        // GetErrorStatistics service
        get_errors_service_ = this->create_service<aurora_edge_runtime::srv::GetErrorStatistics>(
            "/robot/get_errors",
            [this](const std::shared_ptr<aurora_edge_runtime::srv::GetErrorStatistics::Request> request,
                   std::shared_ptr<aurora_edge_runtime::srv::GetErrorStatistics::Response> response) {
                auto [avg, max, within, total] = robot_sim_->getErrorStatistics();
                response->success = true;
                response->average_error = avg;
                response->max_error = max;
                response->waypoints_within_tolerance = within;
                response->total_waypoints = total;
                response->message = "Error statistics retrieved";
                RCLCPP_DEBUG(this->get_logger(), "GetErrorStatistics service called: avg=%.3f, max=%.3f",
                            avg, max);
            }
        );

        // ClearErrors service
        clear_errors_service_ = this->create_service<aurora_edge_runtime::srv::ClearErrors>(
            "/robot/clear_errors",
            [this](const std::shared_ptr<aurora_edge_runtime::srv::ClearErrors::Request> request,
                   std::shared_ptr<aurora_edge_runtime::srv::ClearErrors::Response> response) {
                robot_sim_->clearCollectionErrors();
                response->success = true;
                response->message = "Errors cleared successfully";
                RCLCPP_INFO(this->get_logger(), "ClearErrors service called");
            }
        );

        RCLCPP_INFO(this->get_logger(), "ROS2 services initialized:");
        RCLCPP_INFO(this->get_logger(), "  - /robot/set_path");
        RCLCPP_INFO(this->get_logger(), "  - /robot/get_position");
        RCLCPP_INFO(this->get_logger(), "  - /robot/get_errors");
        RCLCPP_INFO(this->get_logger(), "  - /robot/clear_errors");
    }

private:
    std::shared_ptr<RobotSimulatorV2> robot_sim_;
    rclcpp::Service<aurora_edge_runtime::srv::SetTargetPath>::SharedPtr set_path_service_;
    rclcpp::Service<aurora_edge_runtime::srv::GetCurrentPosition>::SharedPtr get_position_service_;
    rclcpp::Service<aurora_edge_runtime::srv::GetErrorStatistics>::SharedPtr get_errors_service_;
    rclcpp::Service<aurora_edge_runtime::srv::ClearErrors>::SharedPtr clear_errors_service_;
};

int main(int argc, char** argv) {
    try {
        std::cout << R"(
╔══════════════════════════════════════════════════════════════════╗
║              Robot Simulator V2 - Standalone Process             ║
║                                                                  ║
║  ROS2 Services:                                                  ║
║    - /robot/set_path        Set target path                      ║
║    - /robot/get_position    Get current position                 ║
║    - /robot/get_errors      Get error statistics                 ║
║    - /robot/clear_errors    Clear error history                  ║
║                                                                  ║
║  ROS2 Topics:                                                    ║
║    - /robot/odom            Odometry data (50Hz)                 ║
║    - /robot/joint_states    Joint states (50Hz)                  ║
║    - /robot/imu             IMU data (50Hz)                      ║
║                                                                  ║
║  Press Ctrl+C to stop the simulator                              ║
╚══════════════════════════════════════════════════════════════════╝
)" << std::endl;

        // Initialize ROS2
        rclcpp::init(argc, argv);

        // Create robot simulator node
        std::cout << "\n[RobotSim] Initializing RobotSimulatorV2..." << std::endl;
        auto robot_sim = std::make_shared<RobotSimulatorV2>();

        // Start the simulation
        robot_sim->startSimulation();
        std::cout << "[RobotSim] Simulation started at 50Hz" << std::endl;

        // Create service node wrapper
        auto service_node = std::make_shared<RobotServiceNode>(robot_sim);
        std::cout << "[RobotSim] Services available:" << std::endl;
        std::cout << "  - /robot/set_path" << std::endl;
        std::cout << "  - /robot/get_position" << std::endl;
        std::cout << "  - /robot/get_errors" << std::endl;
        std::cout << "  - /robot/clear_errors" << std::endl;

        // Create multi-threaded executor
        rclcpp::executors::MultiThreadedExecutor executor(
            rclcpp::ExecutorOptions(), 2  // Use 2 threads
        );
        executor.add_node(robot_sim);
        executor.add_node(service_node);

        // Spin the executor
        std::cout << "\n[RobotSim] Ready. Waiting for service requests..." << std::endl;
        executor.spin();

        // Cleanup
        std::cout << "\n[RobotSim] Shutting down..." << std::endl;
        robot_sim->stopSimulation();
        robot_sim.reset();
        service_node.reset();

        rclcpp::shutdown();
        std::cout << "[RobotSim] Shutdown complete" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[RobotSim] Exception occurred: " << e.what() << std::endl;
        return -1;
    }
}
