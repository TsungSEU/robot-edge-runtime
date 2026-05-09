// safety_system.cpp - 机器人安全系统实现
// 实现综合安全保护

#include "safety_system.h"
#include <algorithm>
#include <cmath>

namespace aurora::gait {

// ============================================================================
// SafetySystem 实现
// ============================================================================

SafetySystem::SafetySystem(const SafetySystemConfig& config)
    : config_(config)
    , status_()
    , last_input_time_(std::chrono::steady_clock::now())
    , last_watchdog_feed_(std::chrono::steady_clock::now())
    , event_callback_()
{
    // 初始化关节限位映射
    for (const auto& limit : config_.joint_limits) {
        joint_limits_map_[limit.joint_index] = limit;
    }
}

void SafetySystem::setConfig(const SafetySystemConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    // 重建关节限位映射
    joint_limits_map_.clear();
    for (const auto& limit : config_.joint_limits) {
        joint_limits_map_[limit.joint_index] = limit;
    }
}

const SafetySystemConfig& SafetySystem::getConfig() const {
    return config_;
}

const SafetySystemStatus& SafetySystem::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void SafetySystem::setJointLimit(const JointLimit& limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    joint_limits_map_[limit.joint_index] = limit;
}

void SafetySystem::setJointLimits(const std::vector<JointLimit>& limits) {
    std::lock_guard<std::mutex> lock(mutex_);

    joint_limits_map_.clear();
    for (const auto& limit : limits) {
        joint_limits_map_[limit.joint_index] = limit;
    }

    // 更新配置
    config_.joint_limits = limits;
}

bool SafetySystem::checkJointState(int joint_index, const JointState& state) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = joint_limits_map_.find(joint_index);
    if (it == joint_limits_map_.end()) {
        return false;  // 未找到限位配置
    }

    const JointLimit& limit = it->second;

    if (!limit.enabled) {
        return true;  // 未启用，跳过检查
    }

    std::string violation_message;
    bool valid = checkSingleJointLimit(limit, state, violation_message);

    if (!valid) {
        status_.joint_limit_violated = true;
        status_.violated_joint_name = limit.joint_name;
        triggerSafetyEvent(SafetyEvent::JOINT_LIMIT_VIOLATION, violation_message);
    }

    return valid;
}

bool SafetySystem::checkJointStates(const std::vector<JointState>& states) {
    std::lock_guard<std::mutex> lock(mutex_);

    bool all_valid = true;

    for (size_t i = 0; i < states.size(); ++i) {
        auto it = joint_limits_map_.find(static_cast<int>(i));
        if (it == joint_limits_map_.end()) {
            continue;
        }

        const JointLimit& limit = it->second;

        if (!limit.enabled) {
            continue;
        }

        std::string violation_message;
        bool valid = checkSingleJointLimit(limit, states[i], violation_message);

        if (!valid) {
            all_valid = false;
            status_.joint_limit_violated = true;
            status_.violated_joint_name = limit.joint_name;
            triggerSafetyEvent(SafetyEvent::JOINT_LIMIT_VIOLATION, violation_message);
            break;  // 找到第一个违规就停止
        }
    }

    return all_valid;
}

void SafetySystem::triggerEmergencyStop(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.emergency_stop_enabled) {
        status_.emergency_stop_active = true;
        triggerSafetyEvent(SafetyEvent::EMERGENCY_STOP_TRIGGERED, reason);
    }
}

void SafetySystem::clearEmergencyStop() {
    std::lock_guard<std::mutex> lock(mutex_);
    status_.emergency_stop_active = false;
}

bool SafetySystem::isEmergencyStopActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_.emergency_stop_active;
}

void SafetySystem::feedWatchdog() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_watchdog_feed_ = std::chrono::steady_clock::now();
    status_.watchdog_expired = false;
}

void SafetySystem::resetWatchdog() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_watchdog_feed_ = std::chrono::steady_clock::now();
    status_.watchdog_expired = false;
}

void SafetySystem::checkInputTimeout() {
    if (!config_.input_timeout_enabled) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_input_time_);

    double elapsed_seconds = elapsed.count() / 1000.0;

    if (elapsed_seconds > config_.input_timeout_seconds) {
        status_.input_timeout_triggered = true;
        triggerSafetyEvent(SafetyEvent::INPUT_TIMEOUT,
                          "Input timeout: " + std::to_string(elapsed_seconds) + "s");
    }
}

void SafetySystem::updateInputTimestamp() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_input_time_ = std::chrono::steady_clock::now();
    status_.input_timeout_triggered = false;
}

void SafetySystem::checkWatchdog() {
    if (!config_.watchdog_enabled) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_watchdog_feed_);

    double elapsed_seconds = elapsed.count() / 1000.0;

    if (elapsed_seconds > config_.watchdog_timeout_seconds) {
        status_.watchdog_expired = true;
        triggerSafetyEvent(SafetyEvent::WATCHDOG_EXPIRED,
                          "Watchdog expired: " + std::to_string(elapsed_seconds) + "s");

        // 看门狗超时自动触发紧急停止
        triggerEmergencyStop("Watchdog expired");
    }
}

void SafetySystem::update(double dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查输入超时
    checkInputTimeout();

    // 检查看门狗
    checkWatchdog();

    // 如果有任何安全违规且紧急停止启用，触发紧急停止
    if (config_.emergency_stop_enabled && status_.hasViolation() &&
        !status_.emergency_stop_active) {

        triggerEmergencyStop("Safety violation detected");
    }
}

void SafetySystem::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    status_.reset();
    last_input_time_ = std::chrono::steady_clock::now();
    last_watchdog_feed_ = std::chrono::steady_clock::now();
}

void SafetySystem::setEventCallback(SafetyEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = std::move(callback);
}

std::string SafetySystem::getEventDescription(SafetyEvent event) {
    switch (event) {
        case SafetyEvent::EMERGENCY_STOP_TRIGGERED:
            return "Emergency Stop Triggered";
        case SafetyEvent::JOINT_LIMIT_VIOLATION:
            return "Joint Limit Violation";
        case SafetyEvent::INPUT_TIMEOUT:
            return "Input Timeout";
        case SafetyEvent::WATCHDOG_EXPIRED:
            return "Watchdog Expired";
        case SafetyEvent::VELOCITY_EXCEEDED:
            return "Velocity Exceeded";
        case SafetyEvent::POSITION_EXCEEDED:
            return "Position Exceeded";
        case SafetyEvent::EFFORT_EXCEEDED:
            return "Effort Exceeded";
        case SafetyEvent::COLLISION_DETECTED:
            return "Collision Detected";
        case SafetyEvent::STABILITY_LOST:
            return "Stability Lost";
        default:
            return "Unknown Safety Event";
    }
}

void SafetySystem::triggerSafetyEvent(SafetyEvent event, const std::string& message) {
    status_.last_event = event;
    status_.last_message = message;

    if (event_callback_) {
        event_callback_(event, message);
    }
}

bool SafetySystem::checkSingleJointLimit(
    const JointLimit& limit,
    const JointState& state,
    std::string& violation_message) {

    // 检查位置限制
    if (state.position < limit.min_position) {
        violation_message = limit.joint_name + " position below minimum: " +
                           std::to_string(state.position) + " < " +
                           std::to_string(limit.min_position);
        return false;
    }

    if (state.position > limit.max_position) {
        violation_message = limit.joint_name + " position above maximum: " +
                           std::to_string(state.position) + " > " +
                           std::to_string(limit.max_position);
        return false;
    }

    // 检查速度限制
    if (std::abs(state.velocity) > limit.max_velocity) {
        violation_message = limit.joint_name + " velocity exceeded: " +
                           std::to_string(std::abs(state.velocity)) + " > " +
                           std::to_string(limit.max_velocity);
        return false;
    }

    // 检查力矩限制
    if (std::abs(state.effort) > limit.max_effort) {
        violation_message = limit.joint_name + " effort exceeded: " +
                           std::to_string(std::abs(state.effort)) + " > " +
                           std::to_string(limit.max_effort);
        return false;
    }

    return true;
}

// ============================================================================
// JointLimitValidator 实现
// ============================================================================

JointLimitValidator::JointLimitValidator(const std::vector<JointLimit>& limits) {
    for (const auto& limit : limits) {
        limits_map_[limit.joint_index] = limit;
    }
}

bool JointLimitValidator::validate(int joint_index, const JointState& state) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = limits_map_.find(joint_index);
    if (it == limits_map_.end()) {
        return true;  // 未找到限位配置，假设有效
    }

    const JointLimit& limit = it->second;

    if (!limit.enabled) {
        return true;
    }

    // 检查位置
    if (state.position < limit.min_position || state.position > limit.max_position) {
        return false;
    }

    // 检查速度
    if (std::abs(state.velocity) > limit.max_velocity) {
        return false;
    }

    // 检查力矩
    if (std::abs(state.effort) > limit.max_effort) {
        return false;
    }

    return true;
}

bool JointLimitValidator::validateAndClip(int joint_index, JointState& state) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = limits_map_.find(joint_index);
    if (it == limits_map_.end()) {
        return true;
    }

    const JointLimit& limit = it->second;
    bool valid = true;

    if (!limit.enabled) {
        return true;
    }

    // 裁剪位置
    if (state.position < limit.min_position) {
        state.position = limit.min_position;
        valid = false;
    } else if (state.position > limit.max_position) {
        state.position = limit.max_position;
        valid = false;
    }

    // 裁剪速度
    double velocity = std::clamp(state.velocity, -limit.max_velocity, limit.max_velocity);
    if (std::abs(velocity - state.velocity) > 1e-6) {
        state.velocity = velocity;
        valid = false;
    }

    // 裁剪力矩
    double effort = std::clamp(state.effort, -limit.max_effort, limit.max_effort);
    if (std::abs(effort - state.effort) > 1e-6) {
        state.effort = effort;
        valid = false;
    }

    return valid;
}

const JointLimit* JointLimitValidator::getJointLimit(int joint_index) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = limits_map_.find(joint_index);
    if (it != limits_map_.end()) {
        return &it->second;
    }

    return nullptr;
}

// ============================================================================
// WatchdogTimer 实现
// ============================================================================

WatchdogTimer::WatchdogTimer(double timeout_seconds, double feed_interval)
    : timeout_seconds_(timeout_seconds)
    , last_feed_time_(std::chrono::steady_clock::now())
{
    (void)feed_interval;  // 未使用，保留以备将来使用
}

void WatchdogTimer::feed() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_feed_time_ = std::chrono::steady_clock::now();
}

bool WatchdogTimer::isExpired() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_feed_time_);

    return (elapsed.count() / 1000.0) > timeout_seconds_;
}

void WatchdogTimer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_feed_time_ = std::chrono::steady_clock::now();
}

double WatchdogTimer::getTimeSinceLastFeed() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_feed_time_);

    return elapsed.count() / 1000.0;
}

// ============================================================================
// InputTimeoutDetector 实现
// ============================================================================

InputTimeoutDetector::InputTimeoutDetector(double timeout_seconds)
    : timeout_seconds_(timeout_seconds)
    , last_input_time_(std::chrono::steady_clock::now())
{
}

void InputTimeoutDetector::update() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_input_time_ = std::chrono::steady_clock::now();
}

bool InputTimeoutDetector::isTimeout() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_input_time_);

    return (elapsed.count() / 1000.0) > timeout_seconds_;
}

double InputTimeoutDetector::getTimeSinceLastInput() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_input_time_);

    return elapsed.count() / 1000.0;
}

void InputTimeoutDetector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_input_time_ = std::chrono::steady_clock::now();
}

} // namespace aurora::gait
