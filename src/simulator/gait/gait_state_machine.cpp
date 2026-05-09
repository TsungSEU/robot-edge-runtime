// gait_state_machine.cpp - Unitree风格步态状态机实现

#include "gait_state_machine.h"
#include <stdexcept>
#include <algorithm>

namespace aurora::gait {

// ========== LegGaitStateMachine ==========

LegGaitStateMachine::LegGaitStateMachine(LegID leg_id) {
    state_.leg_id = leg_id;
    state_.state = GaitState::IDLE;
    state_.phase = 0;
    state_.phase_offset = 0;
    state_.swing_progress = 0;
    state_.stance_duration = 0;
    state_.swing_duration = 0;
    state_.is_loaded = false;
    state_.load_factor = 0;
    state_.step_count = 0;
}

void LegGaitStateMachine::setGaitParameters(const GaitParameters& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    params_ = params;
}

void LegGaitStateMachine::setPhaseOffset(double offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.phase_offset = GaitUtils::normalizePhase(offset);
}

const LegGaitState& LegGaitStateMachine::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::vector<GaitEvent> LegGaitStateMachine::update(double dt, double global_phase,
                                                     const FootPosition* foot_position) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<GaitEvent> events;

    // 计算本地相位
    double local_phase = GaitUtils::normalizePhase(global_phase + state_.phase_offset);
    state_.phase = local_phase;

    // 根据当前状态执行不同的更新逻辑
    switch (state_.state) {
        case GaitState::IDLE:
            // IDLE状态下等待START事件
            break;

        case GaitState::STANCE:
            updateStancePhase(dt);
            // 检查是否应该进入摆动相
            if (GaitUtils::isInSwingPhase(local_phase, params_.duty_factor)) {
                transitionState(GaitState::SWING_UP);
                events.push_back(GaitEvent::SWING_TRIGGER);
            }
            break;

        case GaitState::SWING_UP:
            updateSwingPhase(dt);
            // 检查是否到达中段
            if (state_.swing_progress >= params_.swing_apex_ratio) {
                transitionState(GaitState::SWING_MID);
            }
            // 检查是否应该结束摆动（相位进入支撑相）
            if (GaitUtils::isInStancePhase(local_phase, params_.duty_factor)) {
                if (foot_position && foot_position->z <= 0) {
                    transitionState(GaitState::CONTACT);
                    events.push_back(GaitEvent::CONTACT_DETECTED);
                } else {
                    // 还没有接触地面，继续摆动
                }
            }
            break;

        case GaitState::SWING_MID:
            updateSwingPhase(dt);
            // 检查是否应该进入下降段
            if (state_.swing_progress >= (1.0 - params_.swing_apex_ratio) / 2.0 + params_.swing_apex_ratio) {
                transitionState(GaitState::SWING_DOWN);
            }
            break;

        case GaitState::SWING_DOWN:
            updateSwingPhase(dt);
            // 检查是否接触地面或相位进入支撑相
            if (GaitUtils::isInStancePhase(local_phase, params_.duty_factor)) {
                transitionState(GaitState::CONTACT);
                events.push_back(GaitEvent::CONTACT_DETECTED);
            }
            break;

        case GaitState::CONTACT:
            // 接触相，等待稳定
            state_.is_loaded = true;
            state_.load_factor = std::min(1.0, state_.load_factor + dt * 5.0);  // 快速加载
            if (state_.load_factor >= 0.9) {
                transitionState(GaitState::STANCE);
                events.push_back(GaitEvent::STABLE_ACHIEVED);
            }
            break;

        case GaitState::EARLY_CONTACT:
            // 早期接触处理
            state_.swing_progress = 1.0;
            transitionState(GaitState::CONTACT);
            events.push_back(GaitEvent::CONTACT_DETECTED);
            break;

        case GaitState::SLIP:
            // 打滑状态，需要恢复
            state_.is_loaded = false;
            state_.load_factor = 0;
            // 简化处理：直接进入摆动相
            transitionState(GaitState::SWING_UP);
            break;

        case GaitState::RECOVERY:
            // 恢复状态
            state_.swing_progress = 1.0;
            transitionState(GaitState::CONTACT);
            break;
    }

    // 触发事件回调
    for (const auto& event : events) {
        triggerEvent(event);
    }

    return events;
}

void LegGaitStateMachine::handleEvent(GaitEvent event) {
    std::lock_guard<std::mutex> lock(mutex_);

    switch (event) {
        case GaitEvent::START:
            if (state_.state == GaitState::IDLE) {
                transitionState(GaitState::STANCE);
            }
            break;

        case GaitEvent::STOP:
            transitionState(GaitState::IDLE);
            break;

        case GaitEvent::SWING_TRIGGER:
            if (state_.state == GaitState::STANCE) {
                transitionState(GaitState::SWING_UP);
            }
            break;

        case GaitEvent::CONTACT_DETECTED:
            if (state_.state == GaitState::SWING_UP ||
                state_.state == GaitState::SWING_MID ||
                state_.state == GaitState::SWING_DOWN) {
                transitionState(GaitState::CONTACT);
            }
            break;

        case GaitEvent::EMERGENCY_STOP:
            transitionState(GaitState::IDLE);
            break;

        case GaitEvent::PHASE_RESET:
            state_.phase = 0;
            state_.swing_progress = 0;
            break;

        default:
            break;
    }
}

void LegGaitStateMachine::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.state = GaitState::IDLE;
    state_.phase = 0;
    state_.swing_progress = 0;
    state_.stance_duration = 0;
    state_.swing_duration = 0;
    state_.is_loaded = false;
    state_.load_factor = 0;
    state_.step_count = 0;
}

void LegGaitStateMachine::setEventCallback(GaitEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = callback;
}

bool LegGaitStateMachine::isInStance() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.state == GaitState::STANCE || state_.state == GaitState::CONTACT;
}

bool LegGaitStateMachine::isInSwing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.state == GaitState::SWING_UP ||
           state_.state == GaitState::SWING_MID ||
           state_.state == GaitState::SWING_DOWN;
}

bool LegGaitStateMachine::isLoaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.is_loaded && state_.load_factor > 0.5;
}

void LegGaitStateMachine::setSwingTarget(const FootPosition& target) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.swing_target = target;
}

std::string LegGaitStateMachine::getStateDescription() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string desc = GaitUtils::legIDToString(state_.leg_id);
    desc += ": ";
    desc += GaitUtils::gaitStateToString(state_.state);
    desc += " (phase=";
    desc += std::to_string(state_.phase);
    desc += ", load=";
    desc += std::to_string(state_.load_factor);
    desc += ")";
    return desc;
}

void LegGaitStateMachine::transitionState(GaitState new_state) {
    if (state_.state == new_state) return;

    // 状态转换处理
    switch (new_state) {
        case GaitState::STANCE:
            state_.is_loaded = true;
            state_.load_factor = 1.0;
            state_.step_count++;
            break;

        case GaitState::SWING_UP:
            state_.is_loaded = false;
            state_.load_factor = 0.0;
            state_.swing_progress = 0.0;
            state_.stance_start = state_.position;
            break;

        case GaitState::CONTACT:
            state_.last_contact = state_.position;
            break;

        default:
            break;
    }

    state_.state = new_state;
}

void LegGaitStateMachine::checkStateTransition(double global_phase) {
    double local_phase = GaitUtils::normalizePhase(global_phase + state_.phase_offset);
    state_.phase = local_phase;

    // 简化的状态转换逻辑
    if (state_.state == GaitState::STANCE &&
        GaitUtils::isInSwingPhase(local_phase, params_.duty_factor)) {
        transitionState(GaitState::SWING_UP);
    } else if ((state_.state == GaitState::SWING_UP ||
                state_.state == GaitState::SWING_MID ||
                state_.state == GaitState::SWING_DOWN) &&
               GaitUtils::isInStancePhase(local_phase, params_.duty_factor)) {
        transitionState(GaitState::CONTACT);
    } else if (state_.state == GaitState::CONTACT) {
        transitionState(GaitState::STANCE);
    }
}

void LegGaitStateMachine::updateSwingPhase(double dt) {
    // 计算摆动进度
    double swing_period = params_.swing_duration;
    if (swing_period > 0) {
        state_.swing_progress += dt / swing_period;
        state_.swing_progress = std::min(1.0, state_.swing_progress);
    }
    state_.swing_duration += dt;
}

void LegGaitStateMachine::updateStancePhase(double dt) {
    state_.stance_duration += dt;
}

void LegGaitStateMachine::triggerEvent(GaitEvent event) {
    if (event_callback_) {
        event_callback_(state_.leg_id, event, state_);
    }
}

// ========== GaitCoordinator ==========

GaitCoordinator::GaitCoordinator(const GaitStateMachineConfig& config)
    : current_mode_(GaitMode::STAND)
    , config_(config)
    , global_phase_(0)
    , running_(false)
{
    // 默认创建4条腿的状态机
    setNumLegs(4);
}

void GaitCoordinator::setNumLegs(size_t num_legs) {
    std::lock_guard<std::mutex> lock(mutex_);

    leg_state_machines_.clear();
    for (size_t i = 0; i < num_legs; ++i) {
        LegID leg_id = static_cast<LegID>(i % 4);  // 循环使用FL, FR, RL, RR
        leg_state_machines_.push_back(
            std::make_unique<LegGaitStateMachine>(leg_id));
    }
}

void GaitCoordinator::setGaitMode(GaitMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (current_mode_ != mode) {
        current_mode_ = mode;

        // 应用步态模式的相位偏移
        applyGaitModePhaseOffsets();

        // 更新占空比
        auto config = getGaitModeConfig(mode);
        params_.duty_factor = config.duty_factor;
        params_.updateDurationsFromDutyFactor();

        // 触发模式切换事件
        if (event_callback_) {
            for (auto& machine : leg_state_machines_) {
                auto state = machine->getState();
                event_callback_(state.leg_id, GaitEvent::MODE_CHANGE, state);
            }
        }
    }
}

GaitMode GaitCoordinator::getGaitMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_mode_;
}

void GaitCoordinator::setGaitParameters(const GaitParameters& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    params_ = params;

    // 更新所有状态机的参数
    for (auto& machine : leg_state_machines_) {
        machine->setGaitParameters(params);
    }
}

const GaitParameters& GaitCoordinator::getGaitParameters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return params_;
}

std::vector<std::pair<LegID, GaitEvent>> GaitCoordinator::update(double dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::pair<LegID, GaitEvent>> all_events;

    if (!running_) {
        return all_events;
    }

    // 更新全局相位
    double phase_increment = 2.0 * M_PI * params_.step_frequency * dt;
    global_phase_ = GaitUtils::normalizePhase(global_phase_ + phase_increment);

    // 更新每条腿的状态机
    for (auto& machine : leg_state_machines_) {
        // 获取当前足端位置（如果已设置）
        auto state = machine->getState();
        const FootPosition* foot_pos = nullptr;  // 可以通过额外方法设置

        auto events = machine->update(dt, global_phase_, foot_pos);

        for (const auto& event : events) {
            all_events.push_back({state.leg_id, event});
        }
    }

    return all_events;
}

const LegGaitState& GaitCoordinator::getLegState(LegID leg_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& machine : leg_state_machines_) {
        if (machine->getState().leg_id == leg_id) {
            return machine->getState();
        }
    }

    throw std::runtime_error("Leg not found");
}

const std::vector<LegGaitState>& GaitCoordinator::getAllStates() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 缓存状态（注意：这是解锁后的状态，实际使用需要注意）
    static thread_local std::vector<LegGaitState> cached_states;
    cached_states.clear();

    for (const auto& machine : leg_state_machines_) {
        cached_states.push_back(machine->getState());
    }

    return cached_states;
}

void GaitCoordinator::setFootPosition(LegID leg_id, const FootPosition& position) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& machine : leg_state_machines_) {
        if (machine->getState().leg_id == leg_id) {
            // 需要添加可变访问方法
            break;
        }
    }
}

void GaitCoordinator::setSwingTarget(LegID leg_id, const FootPosition& target) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& machine : leg_state_machines_) {
        if (machine->getState().leg_id == leg_id) {
            machine->setSwingTarget(target);
            break;
        }
    }
}

void GaitCoordinator::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    global_phase_ = 0;

    // 启动所有状态机
    for (auto& machine : leg_state_machines_) {
        machine->handleEvent(GaitEvent::START);
    }
}

void GaitCoordinator::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;

    // 停止所有状态机
    for (auto& machine : leg_state_machines_) {
        machine->handleEvent(GaitEvent::STOP);
    }
}

void GaitCoordinator::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    global_phase_ = 0;

    for (auto& machine : leg_state_machines_) {
        machine->reset();
    }
}

bool GaitCoordinator::hasSwingingLeg() const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& machine : leg_state_machines_) {
        if (machine->isInSwing()) {
            return true;
        }
    }
    return false;
}

bool GaitCoordinator::hasStanceLeg() const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& machine : leg_state_machines_) {
        if (machine->isInStance()) {
            return true;
        }
    }
    return false;
}

size_t GaitCoordinator::getStanceLegCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    for (const auto& machine : leg_state_machines_) {
        if (machine->isInStance()) {
            count++;
        }
    }
    return count;
}

void GaitCoordinator::setEventCallback(GaitEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = callback;

    // 设置所有状态机的回调
    for (auto& machine : leg_state_machines_) {
        machine->setEventCallback(callback);
    }
}

double GaitCoordinator::getGlobalPhase() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return global_phase_;
}

const char* GaitCoordinator::getGaitModeName(GaitMode mode) {
    switch (mode) {
        case GaitMode::STAND:       return "stand";
        case GaitMode::TROT:        return "trot";
        case GaitMode::WALK:        return "walk";
        case GaitMode::GALLOP:      return "gallop";
        case GaitMode::PACE:        return "pace";
        case GaitMode::BIPED_WALK:  return "biped_walk";
        case GaitMode::BIPED_STAND: return "biped_stand";
        case GaitMode::BIPED_HOP:   return "biped_hop";
        case GaitMode::CUSTOM:      return "custom";
        default:                    return "unknown";
    }
}

const char* GaitCoordinator::getCurrentGaitModeName() const {
    return getGaitModeName(current_mode_);
}

void GaitCoordinator::applyGaitModePhaseOffsets() {
    auto config = getGaitModeConfig(current_mode_);

    for (size_t i = 0; i < leg_state_machines_.size() && i < 4; ++i) {
        leg_state_machines_[i]->setPhaseOffset(config.phase_offsets[i]);
    }
}

GaitModeConfig GaitCoordinator::getGaitModeConfig(GaitMode mode) const {
    switch (mode) {
        case GaitMode::STAND:       return GaitModeConfig::stand();
        case GaitMode::TROT:        return GaitModeConfig::trot();
        case GaitMode::WALK:        return GaitModeConfig::walk();
        case GaitMode::GALLOP:      return GaitModeConfig::gallop();
        case GaitMode::PACE:        return GaitModeConfig::pace();
        case GaitMode::BIPED_WALK:  return GaitModeConfig::bipedWalk();
        case GaitMode::BIPED_STAND: return GaitModeConfig::bipedStand();
        default:                    return GaitModeConfig::stand();
    }
}

// ========== GaitUtils ==========

const char* GaitUtils::legIDToString(LegID leg_id) {
    switch (leg_id) {
        case LegID::FL:    return "FL";
        case LegID::FR:    return "FR";
        case LegID::RL:    return "RL";
        case LegID::RR:    return "RR";
        case LegID::LEFT:  return "LEFT";
        case LegID::RIGHT: return "RIGHT";
        default:           return "UNKNOWN";
    }
}

LegID GaitUtils::stringToLegID(const std::string& str) {
    if (str == "FL") return LegID::FL;
    if (str == "FR") return LegID::FR;
    if (str == "RL") return LegID::RL;
    if (str == "RR") return LegID::RR;
    if (str == "LEFT") return LegID::LEFT;
    if (str == "RIGHT") return LegID::RIGHT;
    return LegID::UNKNOWN;
}

const char* GaitUtils::gaitStateToString(GaitState state) {
    switch (state) {
        case GaitState::IDLE:          return "IDLE";
        case GaitState::STANCE:        return "STANCE";
        case GaitState::SWING_UP:      return "SWING_UP";
        case GaitState::SWING_MID:     return "SWING_MID";
        case GaitState::SWING_DOWN:    return "SWING_DOWN";
        case GaitState::CONTACT:       return "CONTACT";
        case GaitState::EARLY_CONTACT: return "EARLY_CONTACT";
        case GaitState::SLIP:          return "SLIP";
        case GaitState::RECOVERY:      return "RECOVERY";
        default:                       return "UNKNOWN";
    }
}

const char* GaitUtils::gaitEventToString(GaitEvent event) {
    switch (event) {
        case GaitEvent::START:             return "START";
        case GaitEvent::STOP:              return "STOP";
        case GaitEvent::SWING_TRIGGER:     return "SWING_TRIGGER";
        case GaitEvent::CONTACT_DETECTED:  return "CONTACT_DETECTED";
        case GaitEvent::STABLE_ACHIEVED:   return "STABLE_ACHIEVED";
        case GaitEvent::SLIP_DETECTED:     return "SLIP_DETECTED";
        case GaitEvent::OBSTACLE_HIT:      return "OBSTACLE_HIT";
        case GaitEvent::EMERGENCY_STOP:    return "EMERGENCY_STOP";
        case GaitEvent::MODE_CHANGE:       return "MODE_CHANGE";
        case GaitEvent::PHASE_RESET:       return "PHASE_RESET";
        default:                           return "UNKNOWN";
    }
}

} // namespace aurora::gait
