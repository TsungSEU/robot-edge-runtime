// gait_trigger.cpp - 基于步态的采集触发器实现
//
// 设计原则：
// 1. 采集基于实际足端落点，而非规划路径点
// 2. 采集时机在双脚支撑的稳定期
// 3. 考虑步长约束，避免过于密集采集

#include "gait_trigger.h"
#include "aurora_edge_runtime/srv/trigger_recording.hpp"
#include "common/log/logger.h"
#include "common/performance_utils.h"
#include "common/ros2/qos_profiles.h"
#include "common/ros2/qos_callbacks.h"
#include "trigger/ITrigger.h"
#include <cmath>
#include <algorithm>
#include <thread>

namespace aurora::collector {

GaitTrigger::GaitTrigger()
    : rclcpp::Node("gait_trigger")
    , robot_position_(0.0, 0.0)
    , robot_yaw_(0.0) {

    // 创建订阅者
    odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
        "/robot/odom", aurora::common::qos::odometry(),
        std::bind(&GaitTrigger::odomCallback, this, std::placeholders::_1));

    joint_sub_ = this->create_subscription<sensor_msgs::msg::JointState>(
        "/robot/joint_states", aurora::common::qos::sensor_data(),
        std::bind(&GaitTrigger::jointCallback, this, std::placeholders::_1));

    aurora::common::qos::installDeadlineCallback<nav_msgs::msg::Odometry>(
        odom_sub_, "/robot/odom");
    aurora::common::qos::installDeadlineCallback<sensor_msgs::msg::JointState>(
        joint_sub_, "/robot/joint_states");

    // Initialize service client for trigger recording
    trigger_client_ = this->create_client<aurora_edge_runtime::srv::TriggerRecording>("/robot/trigger");

    if (!trigger_client_->wait_for_service(std::chrono::seconds(5))) {
        RCLCPP_WARN(this->get_logger(),
                   "TriggerRecording service not available after 5s, will retry on trigger");
    }

    RCLCPP_INFO(this->get_logger(), "GaitTrigger initialized");
    RCLCPP_INFO(this->get_logger(), "  Subscribed to: /robot/odom");
    RCLCPP_INFO(this->get_logger(), "  Subscribed to: /robot/joint_states");
    RCLCPP_INFO(this->get_logger(), "  Service client: /robot/trigger");
    RCLCPP_INFO(this->get_logger(), "  min_step_distance: %.2f m", min_step_distance_);
    RCLCPP_INFO(this->get_logger(), "  min_collection_interval: %.2f s", min_collection_interval_);
}

void GaitTrigger::setEventCallback(GaitEventCallback callback) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    event_callback_ = callback;
}

void GaitTrigger::odomCallback(const nav_msgs::msg::Odometry::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // 更新机器人位置
    robot_position_.x = msg->pose.pose.position.x;
    robot_position_.y = msg->pose.pose.position.y;

    // 提取航向角
    double qx = msg->pose.pose.orientation.x;
    double qy = msg->pose.pose.orientation.y;
    double qz = msg->pose.pose.orientation.z;
    double qw = msg->pose.pose.orientation.w;

    // 四元数转偏航角
    double siny_cosp = 2.0 * (qw * qz + qx * qy);
    double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
    // 性能优化：使用快速 atan2
    robot_yaw_ = aurora::performance::fastAtan2(siny_cosp, cosy_cosp);

    // 分析步态状态
    analyzeGaitState();

    // 检测足端着地事件
    detectFootstrike();
}

void GaitTrigger::jointCallback(const sensor_msgs::msg::JointState::SharedPtr msg) {
    // 从关节角度估算步态相位
    // 这是一个简化版本，实际应该从关节角度模式计算

    if (msg->position.size() < 12) {
        return;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);

    // 简化：使用髋关节角度估算步态相位
    // 左髋关节 (index 0-2) 和 右髋关节 (index 6-8)
    double left_hip_pitch = msg->position[2];   // left_hip_pitch
    double right_hip_pitch = msg->position[8];  // right_hip_pitch

    // 步态相位估算（简化版）
    // 当左腿在前摆时相位为 0-π，右腿在前摆时相位为 π-2π
    double phase_diff = left_hip_pitch - right_hip_pitch;
    gait_state_.phase = std::fmod(std::atan2(std::sin(phase_diff), std::cos(phase_diff)) + M_PI, 2.0 * M_PI);

    // 判断支撑/摆动状态（简化）
    gait_state_.left_foot_stance = (gait_state_.phase < M_PI);  // 0-π 左脚支撑
    gait_state_.right_foot_stance = (gait_state_.phase >= M_PI); // π-2π 右脚支撑

    // 更新稳定状态
    gait_state_.is_stable = isInStablePhase();
}

void GaitTrigger::analyzeGaitState() {
    // 分析当前是否处于稳定期（双脚支撑且相位在中间区域）

    // 支撑相中间区域（去除起止阶段的20%）
    double phase_margin = M_PI * 0.2;  // 每个相位去掉20%

    if (gait_state_.phase < M_PI) {
        // 左脚支撑相：相位在 [phase_margin, π - phase_margin] 时稳定
        gait_state_.left_foot_stance = true;
        gait_state_.right_foot_stance = false;

        double stable_start = phase_margin;
        double stable_end = M_PI - phase_margin;
        gait_state_.is_stable = (gait_state_.phase >= stable_start &&
                                   gait_state_.phase <= stable_end);
    } else {
        // 右脚支撑相：相位在 [π + phase_margin, 2π - phase_margin] 时稳定
        gait_state_.left_foot_stance = false;
        gait_state_.right_foot_stance = true;

        double stable_start = M_PI + phase_margin;
        double stable_end = 2.0 * M_PI - phase_margin;
        gait_state_.is_stable = (gait_state_.phase >= stable_start &&
                                   gait_state_.phase <= stable_end);
    }
}

bool GaitTrigger::isInStablePhase() const {
    // 检查是否处于稳定的双脚支撑期
    // 这里简化为：单脚支撑的中间阶段也算稳定
    // 实际机器人可能需要双脚都着地才稳定

    return gait_state_.is_stable;
}

void GaitTrigger::detectFootstrike() {
    // 检测足端着地事件（摆动相 → 支撑相切换）

    static bool last_left_stance = true;
    static bool last_right_stance = true;

    bool left_stance_changed = (gait_state_.left_foot_stance != last_left_stance);
    bool right_stance_changed = (gait_state_.right_foot_stance != last_right_stance);

    // 左脚着地：从摆动变为支撑
    if (gait_state_.left_foot_stance && left_stance_changed) {
        // 计算足端世界坐标位置
        Footprint fp(robot_position_.x, robot_position_.y, true, gait_state_.phase, true);

        // 性能优化：使用无锁环形缓冲区，自动覆盖最老的数据
        footprints_ring_.push(fp);

        if (event_callback_) {
            event_callback_(GaitEventType::FOOTSTRIKE, fp.position);
        }
    }

    // 右脚着地
    if (gait_state_.right_foot_stance && right_stance_changed) {
        Footprint fp(robot_position_.x, robot_position_.y, false, gait_state_.phase, true);

        // 性能优化：使用无锁环形缓冲区
        footprints_ring_.push(fp);

        if (event_callback_) {
            event_callback_(GaitEventType::FOOTSTRIKE, fp.position);
        }
    }

    last_left_stance = gait_state_.left_foot_stance;
    last_right_stance = gait_state_.right_foot_stance;

    // 检测稳定期事件（可用于采集）
    static bool last_stable = true;
    if (gait_state_.is_stable && !last_stable) {
        gait_state_.step_count++;

        if (event_callback_) {
            event_callback_(GaitEventType::STABLE_STANCE, robot_position_);
        }
    }
    last_stable = gait_state_.is_stable;
}

bool GaitTrigger::shouldTriggerCollection(
    const Point& current_pos,
    const Point& last_collect_pos,
    const std::chrono::steady_clock::time_point& last_collect_time) {

    std::lock_guard<std::mutex> lock(state_mutex_);

    // 1. 必须在稳定期
    if (!gait_state_.is_stable) {
        return false;
    }

    // 2. 检查时间间隔
    auto now = std::chrono::steady_clock::now();
    double time_since_last = std::chrono::duration<double>(now - last_collect_time).count();
    if (time_since_last < min_collection_interval_) {
        return false;
    }

    // 3. 检查步长距离 (性能优化：使用快速逆平方根)
    double dx = current_pos.x - last_collect_pos.x;
    double dy = current_pos.y - last_collect_pos.y;
    double dist_sq = dx * dx + dy * dy;
    // 先比较平方值，避免不必要的 sqrt
    if (dist_sq < min_step_distance_ * min_step_distance_) {
        return false;
    }
    double distance = std::sqrt(dist_sq);  // 只在需要时计算

    // 4. 检查是否与历史足迹过于接近
    // 性能优化：遍历环形缓冲区中的足迹
    double min_dist_sq = min_step_distance_ * 0.3;
    min_dist_sq *= min_dist_sq;  // 预计算平方值

    // 创建临时向量来遍历环形缓冲区
    footprints_cache_.clear();
    Footprint fp;
    while (footprints_ring_.pop(fp)) {
        footprints_cache_.push_back(fp);
    }
    // 重新放回环形缓冲区（因为 pop 会移除）
    for (const auto& cached_fp : footprints_cache_) {
        footprints_ring_.push(cached_fp);
    }

    for (const auto& fp : footprints_cache_) {
        double fdx = current_pos.x - fp.position.x;
        double fdy = current_pos.y - fp.position.y;
        double fdist_sq = fdx * fdx + fdy * fdy;
        if (fdist_sq < min_dist_sq) {  // 使用平方比较避免 sqrt
            return false;
        }
    }

    // 所有检查通过
    RCLCPP_INFO(this->get_logger(), "Collection triggered at (%.2f, %.2f), step: %.2f m, time: %.2f s, phase: %.2f",
               current_pos.x, current_pos.y, distance, time_since_last, gait_state_.phase);

    return true;
}

Footprint GaitTrigger::getLastStableFootprint() const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // 性能优化：从环形缓冲区获取最近的足迹
    // 由于 LockFreeRingBuffer 不直接支持随机访问，
    // 我们需要遍历来获取最后一个元素
    Footprint result;
    Footprint fp;
    while (footprints_ring_.pop(fp)) {
        result = fp;
        footprints_ring_.push(fp);  // 放回去
    }
    return result;
}

std::vector<Footprint> GaitTrigger::getFootprints() const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // 性能优化：从环形缓冲区复制所有足迹到缓存
    footprints_cache_.clear();
    Footprint fp;
    while (footprints_ring_.pop(fp)) {
        footprints_cache_.push_back(fp);
    }
    // 重新放回环形缓冲区
    for (const auto& cached_fp : footprints_cache_) {
        footprints_ring_.push(cached_fp);
    }

    return footprints_cache_;
}

void GaitTrigger::setMinStepDistance(double distance) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    min_step_distance_ = distance;
    RCLCPP_INFO(this->get_logger(), "Min step distance updated: %.2f m", min_step_distance_);
}

void GaitTrigger::setMinCollectionInterval(double seconds) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    min_collection_interval_ = seconds;
    RCLCPP_INFO(this->get_logger(), "Min collection interval updated: %.2f s", min_collection_interval_);
}

void GaitTrigger::triggerRecordingViaService(const TriggerContext& context) {
    if (!trigger_client_->service_is_ready()) {
        RCLCPP_ERROR(this->get_logger(),
                    "TriggerRecording service not available, cannot trigger recording");
        return;
    }

    // Create service request
    auto request = std::make_shared<aurora_edge_runtime::srv::TriggerRecording::Request>();
    request->business_type = context.businessType;
    request->trigger_id = context.triggerId;
    request->trigger_timestamp = context.triggerTimestamp;
    request->trigger_desc = context.triggerDesc;
    request->pos.x = context.pos.x;
    request->pos.y = context.pos.y;
    request->pos.z = 0.0;

    // Send async request and store in shared_ptr for thread safety
    auto future_shared = std::make_shared<decltype(trigger_client_->async_send_request(request))>(
        trigger_client_->async_send_request(request)
    );

    // Handle response asynchronously
    std::thread([this, future_shared]() {
        try {
            auto response = future_shared->future.get();
            if (response->success) {
                RCLCPP_INFO(this->get_logger(),
                           "Recording triggered successfully: %s",
                           response->bag_path.c_str());
            } else {
                RCLCPP_WARN(this->get_logger(),
                           "Recording trigger failed: %s (cooldown: %lu ms)",
                           response->message.c_str(),
                           response->cooldown_remaining);
            }
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(),
                        "Service call failed: %s", e.what());
        }
    }).detach();
}

} // namespace aurora::collector
