// motion_mode_manager.cpp - 运动模式管理器实现
// 实现多种机器人运动模式的管理和切换

#include "motion_mode_manager.h"
#include <algorithm>
#include <stdexcept>

namespace aurora::gait {

// ============================================================================
// MotionModeManager 实现
// ============================================================================

MotionModeManager::MotionModeManager(
    const MotionModeManagerConfig& config)
    : config_(config)
    , current_mode_(config.default_mode)
    , previous_mode_(config.default_mode)
    , target_mode_(config.default_mode)
    , transitioning_(false)
    , transition_progress_(0.0)
    , transition_time_(0.0)
    , event_callback_()
{
    initializeDefaultModeParams();
}

void MotionModeManager::setConfig(const MotionModeManagerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const MotionModeManagerConfig& MotionModeManager::getConfig() const {
    return config_;
}

ModeTransitionResult MotionModeManager::requestModeTransition(
    MotionMode target_mode, bool force) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查目标模式是否有效
    if (target_mode == MotionMode::UNKNOWN) {
        return ModeTransitionResult::INVALID_TARGET;
    }

    // 检查是否已经在该模式
    if (current_mode_ == target_mode) {
        return ModeTransitionResult::SUCCESS;
    }

    // 检查转换是否被允许
    if (!force && !isTransitionAllowed(current_mode_, target_mode)) {
        triggerEventCallback(current_mode_, target_mode,
                           ModeEvent::TRANSITION_FAILED,
                           ModeTransitionResult::NOT_ALLOWED);
        return ModeTransitionResult::NOT_ALLOWED;
    }

    // 安全检查
    if (config_.enable_safety_checks && !force) {
        if (!validateTransitionSafety(current_mode_, target_mode)) {
            triggerEventCallback(current_mode_, target_mode,
                               ModeEvent::SAFETY_TRIGGERED,
                               ModeTransitionResult::SAFETY_VIOLATION);
            return ModeTransitionResult::SAFETY_VIOLATION;
        }
    }

    // 开始转换
    previous_mode_ = current_mode_;
    target_mode_ = target_mode;
    transitioning_ = true;
    transition_progress_ = 0.0;
    transition_time_ = 0.0;

    triggerEventCallback(current_mode_, target_mode,
                       ModeEvent::TRANSITION_STARTED,
                       ModeTransitionResult::SUCCESS);

    return ModeTransitionResult::SUCCESS;
}

ModeTransitionResult MotionModeManager::emergencyModeSwitch(
    MotionMode target_mode) {

    return requestModeTransition(target_mode, true);
}

MotionMode MotionModeManager::getCurrentMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_mode_;
}

MotionMode MotionModeManager::getPreviousMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return previous_mode_;
}

bool MotionModeManager::isTransitionAllowed(MotionMode from, MotionMode to) const {
    // 定义允许的模式转换
    switch (from) {
        case MotionMode::PASSIVE:
            return to == MotionMode::DAMPING ||
                   to == MotionMode::JOINT ||
                   to == MotionMode::STAND;

        case MotionMode::DAMPING:
            return to == MotionMode::PASSIVE ||
                   to == MotionMode::JOINT ||
                   to == MotionMode::STAND;

        case MotionMode::JOINT:
            return to == MotionMode::PASSIVE ||
                   to == MotionMode::DAMPING ||
                   to == MotionMode::STAND ||
                   to == MotionMode::LOCOMOTION;

        case MotionMode::STAND:
            return to == MotionMode::PASSIVE ||
                   to == MotionMode::JOINT ||
                   to == MotionMode::LOCOMOTION;

        case MotionMode::LOCOMOTION:
            return to == MotionMode::JOINT ||
                   to == MotionMode::STAND;

        default:
            return false;
    }
}

void MotionModeManager::setModeParams(
    MotionMode mode,
    std::shared_ptr<MotionModeParams> params) {

    std::lock_guard<std::mutex> lock(mutex_);
    mode_params_[static_cast<int>(mode)] = params;
}

void MotionModeManager::setEventCallback(ModeEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = std::move(callback);
}

void MotionModeManager::update(double dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!transitioning_) {
        return;
    }

    // 更新转换进度
    transition_time_ += dt;
    transition_progress_ = std::min(transition_time_ / config_.transition_timeout, 1.0);

    // 检查转换是否完成
    if (transition_progress_ >= 1.0) {
        executeModeTransition(current_mode_, target_mode_);
        transitioning_ = false;
        triggerEventCallback(previous_mode_, current_mode_,
                           ModeEvent::TRANSITION_COMPLETED,
                           ModeTransitionResult::SUCCESS);
    } else if (transition_time_ > config_.transition_timeout * 1.5) {
        // 超时
        transitioning_ = false;
        triggerEventCallback(current_mode_, target_mode_,
                           ModeEvent::TRANSITION_FAILED,
                           ModeTransitionResult::TIMEOUT);
    }
}

void MotionModeManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_mode_ = config_.default_mode;
    previous_mode_ = config_.default_mode;
    target_mode_ = config_.default_mode;
    transitioning_ = false;
    transition_progress_ = 0.0;
    transition_time_ = 0.0;
}

bool MotionModeManager::isTransitioning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transitioning_;
}

double MotionModeManager::getTransitionProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transition_progress_;
}

void MotionModeManager::initializeDefaultModeParams() {
    // PASSIVE模式
    mode_params_[static_cast<int>(MotionMode::PASSIVE)] =
        std::make_shared<PassiveModeParams>();

    // DAMPING模式
    mode_params_[static_cast<int>(MotionMode::DAMPING)] =
        std::make_shared<DampingModeParams>();

    // JOINT模式
    mode_params_[static_cast<int>(MotionMode::JOINT)] =
        std::make_shared<JointModeParams>();

    // STAND模式
    mode_params_[static_cast<int>(MotionMode::STAND)] =
        std::make_shared<StandModeParams>();

    // LOCOMOTION模式
    mode_params_[static_cast<int>(MotionMode::LOCOMOTION)] =
        std::make_shared<LocomotionModeParams>();
}

bool MotionModeManager::validateTransitionSafety(
    MotionMode from, MotionMode to) const {

    // 基本安全检查：
    // 1. 从PASSIVE转换需要确保机器人稳定
    if (from == MotionMode::PASSIVE) {
        // 这里可以添加更复杂的稳定性检查
        // 目前简化为只允许转换到DAMPING或STAND
        return to == MotionMode::DAMPING || to == MotionMode::STAND;
    }

    // 2. 转换到LOCOMOTION需要确保机器人处于稳定状态
    if (to == MotionMode::LOCOMOTION) {
        return from == MotionMode::STAND || from == MotionMode::JOINT;
    }

    return true;
}

void MotionModeManager::executeModeTransition(MotionMode from, MotionMode to) {
    // 执行实际的模式转换
    previous_mode_ = from;
    current_mode_ = to;

    triggerEventCallback(from, to, ModeEvent::MODE_CHANGED,
                       ModeTransitionResult::SUCCESS);
}

void MotionModeManager::triggerEventCallback(
    MotionMode from,
    MotionMode to,
    ModeEvent event,
    ModeTransitionResult result) {

    if (event_callback_) {
        event_callback_(from, to, event, result);
    }
}

// ============================================================================
// InputSourceArbitrator 实现
// ============================================================================

void InputSourceArbitrator::registerInputSource(
    const std::string& source_id,
    InputSourcePriority priority) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已注册
    for (const auto& source : sources_) {
        if (source.id == source_id) {
            return;  // 已注册
        }
    }

    // 添加新源
    sources_.push_back({source_id, priority, false});
}

void InputSourceArbitrator::unregisterInputSource(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    sources_.erase(
        std::remove_if(sources_.begin(), sources_.end(),
                      [&source_id](const SourceInfo& info) {
                          return info.id == source_id;
                      }),
        sources_.end()
    );
}

std::string InputSourceArbitrator::getActiveSource() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return getHighestPrioritySource();
}

bool InputSourceArbitrator::requestActivation(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找源
    auto it = std::find_if(sources_.begin(), sources_.end(),
                          [&source_id](const SourceInfo& info) {
                              return info.id == source_id;
                          });

    if (it == sources_.end()) {
        return false;  // 源未注册
    }

    // 检查优先级
    std::string current_highest = getHighestPrioritySource();
    if (!current_highest.empty()) {
        auto current_it = std::find_if(sources_.begin(), sources_.end(),
                                      [&current_highest](const SourceInfo& info) {
                                          return info.id == current_highest;
                                      });

        if (current_it != sources_.end() &&
            current_it->priority > it->priority) {
            return false;  // 优先级不够高
        }
    }

    // 激活源，停用其他源
    for (auto& source : sources_) {
        source.active = (source.id == source_id);
    }

    return true;
}

void InputSourceArbitrator::releaseActivation(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(sources_.begin(), sources_.end(),
                          [&source_id](const SourceInfo& info) {
                              return info.id == source_id;
                          });

    if (it != sources_.end()) {
        it->active = false;
    }
}

std::string InputSourceArbitrator::getHighestPrioritySource() const {
    if (sources_.empty()) {
        return "";
    }

    auto it = std::max_element(sources_.begin(), sources_.end(),
                              [](const SourceInfo& a, const SourceInfo& b) {
                                  if (!a.active) return true;
                                  if (!b.active) return false;
                                  return static_cast<int>(a.priority) < static_cast<int>(b.priority);
                              });

    return (it != sources_.end() && it->active) ? it->id : "";
}

} // namespace aurora::gait
