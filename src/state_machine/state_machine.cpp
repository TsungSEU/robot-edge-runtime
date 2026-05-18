#include "state_machine.h"
#include "common/log/logger.h"
#include "common/log/structured_log.h"
#include "common/audit/audit_logger.h"
#include <iostream>
#include <sstream>

namespace aurora::state_machine {

StateMachine& StateMachine::getInstance() {
    static StateMachine instance;
    return instance;
}

StateMachine::StateMachine()
    : current_state_(SystemState::INITIALIZING) {
}

bool StateMachine::initialize() {
    AD_INFO(StateMachine, "Initializing state machine");
    transitionToState(SystemState::IDLE, StateEvent::INIT_COMPLETE);
    AD_INFO(StateMachine, "State machine initialized successfully");
    return true;
}

void StateMachine::setActionCallbacks(const ActionCallbacks& callbacks) {
    actions_ = callbacks;
}

void StateMachine::setAuditLogCallback(AuditLogCallback callback) {
    audit_callback_ = std::move(callback);
}

void StateMachine::setDegradeMode(bool degraded) {
    degrade_mode_.store(degraded);
    if (degraded) {
        AD_WARN(StateMachine, "Entering degrade mode: non-critical operations reduced");
    } else {
        AD_INFO(StateMachine, "Exiting degrade mode: normal operations restored");
    }
}

// ===== 状态字符串转换 =====

const char* StateMachine::stateToString(SystemState state) {
    switch (state) {
        case SystemState::INITIALIZING:    return "INITIALIZING";
        case SystemState::IDLE:            return "IDLE";
        case SystemState::PLANNING:        return "PLANNING";
        case SystemState::NAVIGATING:      return "NAVIGATING";
        case SystemState::DATA_COLLECTING: return "DATA_COLLECTING";
        case SystemState::UPLOADING:       return "UPLOADING";
        case SystemState::ERROR:           return "ERROR";
        case SystemState::SHUTTING_DOWN:   return "SHUTTING_DOWN";
    }
    return "UNKNOWN";
}

const char* StateMachine::eventToString(StateEvent event) {
    switch (event) {
        case StateEvent::INIT_COMPLETE:     return "INIT_COMPLETE";
        case StateEvent::PLAN_REQUEST:      return "PLAN_REQUEST";
        case StateEvent::PLAN_COMPLETE:     return "PLAN_COMPLETE";
        case StateEvent::NAVIGATION_START:  return "NAVIGATION_START";
        case StateEvent::WAYPOINT_REACHED:  return "WAYPOINT_REACHED";
        case StateEvent::TRIGGERED:         return "TRIGGERED";
        case StateEvent::DATA_COLLECTED:    return "DATA_COLLECTED";
        case StateEvent::UPLOAD_REQUEST:    return "UPLOAD_REQUEST";
        case StateEvent::UPLOAD_COMPLETE:   return "UPLOAD_COMPLETE";
        case StateEvent::ERROR_OCCURRED:    return "ERROR_OCCURRED";
        case StateEvent::RECOVERY_REQUEST:  return "RECOVERY_REQUEST";
        case StateEvent::SHUTDOWN_REQUEST:  return "SHUTDOWN_REQUEST";
    }
    return "UNKNOWN";
}

// ===== 事件分发 =====

void StateMachine::handleEvent(StateEvent event) {
    switch (current_state_.load()) {
        case SystemState::INITIALIZING:
            handleInitializing(event);
            break;
        case SystemState::IDLE:
            handleIdle(event);
            break;
        case SystemState::PLANNING:
            handlePlanning(event);
            break;
        case SystemState::NAVIGATING:
            handleNavigating(event);
            break;
        case SystemState::DATA_COLLECTING:
            handleDataCollection(event);
            break;
        case SystemState::UPLOADING:
            handleUploading(event);
            break;
        case SystemState::ERROR:
            handleError(event);
            break;
        case SystemState::SHUTTING_DOWN:
            handleShuttingDown(event);
            break;
    }
}

// ===== 状态处理函数 =====

void StateMachine::handleInitializing(StateEvent event) {
    if (event == StateEvent::INIT_COMPLETE) {
        transitionToState(SystemState::IDLE, event);
    } else {
        AD_WARN(StateMachine, "Unexpected event %s in INITIALIZING state", eventToString(event));
    }
}

void StateMachine::handleIdle(StateEvent event) {
    switch (event) {
        case StateEvent::PLAN_REQUEST:
            AD_INFO(StateMachine, "Starting mission planning");
            transitionToState(SystemState::PLANNING, event);
            break;
        case StateEvent::UPLOAD_REQUEST:
            AD_INFO(StateMachine, "Starting data upload");
            transitionToState(SystemState::UPLOADING, event);
            break;
        case StateEvent::SHUTDOWN_REQUEST:
            AD_INFO(StateMachine, "Shutting down system");
            transitionToState(SystemState::SHUTTING_DOWN, event);
            break;
        default:
            AD_WARN(StateMachine, "Unexpected event %s in IDLE state", eventToString(event));
            break;
    }
}

void StateMachine::handlePlanning(StateEvent event) {
    switch (event) {
        case StateEvent::PLAN_COMPLETE:
            AD_INFO(StateMachine, "Mission planning completed");
            transitionToState(SystemState::NAVIGATING, event);
            break;
        case StateEvent::ERROR_OCCURRED:
            AD_ERROR(StateMachine, "Error occurred during planning");
            transitionToState(SystemState::ERROR, event);
            break;
        default:
            AD_WARN(StateMachine, "Unexpected event %s in PLANNING state", eventToString(event));
            break;
    }
}

void StateMachine::handleNavigating(StateEvent event) {
    switch (event) {
        case StateEvent::WAYPOINT_REACHED:
            // 使用真实触发判断（对接 TriggerManager），而非硬编码 % 5
            if (actions_.should_collect && actions_.should_collect()) {
                AD_INFO(StateMachine, "Trigger condition met at waypoint, starting collection");
                transitionToState(SystemState::DATA_COLLECTING, StateEvent::TRIGGERED);
            } else if (actions_.navigate_step && !actions_.navigate_step()) {
                // navigate_step 返回 false 表示路径完成
                AD_INFO(StateMachine, "Path completed, requesting upload");
                transitionToState(SystemState::UPLOADING, StateEvent::UPLOAD_REQUEST);
            }
            break;
        case StateEvent::TRIGGERED:
            AD_INFO(StateMachine, "External trigger received, starting collection");
            transitionToState(SystemState::DATA_COLLECTING, event);
            break;
        case StateEvent::ERROR_OCCURRED:
            AD_ERROR(StateMachine, "Error occurred during navigation");
            transitionToState(SystemState::ERROR, event);
            break;
        default:
            AD_WARN(StateMachine, "Unexpected event %s in NAVIGATING state", eventToString(event));
            break;
    }
}

void StateMachine::handleDataCollection(StateEvent event) {
    switch (event) {
        case StateEvent::DATA_COLLECTED:
            AD_INFO(StateMachine, "Data collection completed, resuming navigation");
            transitionToState(SystemState::NAVIGATING, event);
            break;
        case StateEvent::ERROR_OCCURRED:
            AD_ERROR(StateMachine, "Error occurred during data collection");
            transitionToState(SystemState::ERROR, event);
            break;
        default:
            AD_WARN(StateMachine, "Unexpected event %s in DATA_COLLECTING state", eventToString(event));
            break;
    }
}

void StateMachine::handleUploading(StateEvent event) {
    switch (event) {
        case StateEvent::UPLOAD_COMPLETE:
            AD_INFO(StateMachine, "Data upload completed");
            transitionToState(SystemState::IDLE, event);
            break;
        case StateEvent::ERROR_OCCURRED:
            AD_ERROR(StateMachine, "Error occurred during data upload");
            // 降级模式：上传失败不进 ERROR，回 IDLE 继续采集
            if (degrade_mode_.load()) {
                AD_WARN(StateMachine, "Upload failed in degrade mode, returning to IDLE");
                transitionToState(SystemState::IDLE, event);
            } else {
                transitionToState(SystemState::ERROR, event);
            }
            break;
        default:
            AD_WARN(StateMachine, "Unexpected event %s in UPLOADING state", eventToString(event));
            break;
    }
}

void StateMachine::handleError(StateEvent event) {
    switch (event) {
        case StateEvent::RECOVERY_REQUEST:
            AD_INFO(StateMachine, "Attempting system recovery");
            setDegradeMode(true);
            transitionToState(SystemState::IDLE, event);
            break;
        case StateEvent::SHUTDOWN_REQUEST:
            AD_INFO(StateMachine, "Shutting down due to error");
            transitionToState(SystemState::SHUTTING_DOWN, event);
            break;
        default:
            AD_WARN(StateMachine, "Unexpected event %s in ERROR state", eventToString(event));
            break;
    }
}

void StateMachine::handleShuttingDown(StateEvent event) {
    AD_INFO(StateMachine, "Event %s received during shutdown, ignoring", eventToString(event));
}

// ===== 状态转换 =====

void StateMachine::transitionToState(SystemState new_state, StateEvent event) {
    SystemState old_state = current_state_.exchange(new_state);

    // 审计日志
    auto now = std::chrono::steady_clock::now();

    // Structured log with JSON context
    std::ostringstream audit_ctx;
    audit_ctx << "{\"from\":\"" << stateToString(old_state)
              << "\",\"to\":\"" << stateToString(new_state)
              << "\",\"event\":\"" << eventToString(event) << "\"}";
    AD_INFO_S(StateMachine, audit_ctx.str().c_str(),
              "State transition: %s -> %s (event: %s)",
              stateToString(old_state), stateToString(new_state), eventToString(event));

    // Audit trail
    AUDIT_LOG(STATE_TRANSITION, "StateMachine", audit_ctx.str());

    if (audit_callback_) {
        audit_callback_(old_state, new_state, event, now);
    }

    // 根据新状态执行相应动作
    switch (new_state) {
        case SystemState::PLANNING:
            if (actions_.plan) {
                bool success = actions_.plan();
                handleEvent(success ? StateEvent::PLAN_COMPLETE : StateEvent::ERROR_OCCURRED);
            }
            break;
        case SystemState::DATA_COLLECTING:
            if (actions_.collect) {
                bool success = actions_.collect();
                handleEvent(success ? StateEvent::DATA_COLLECTED : StateEvent::ERROR_OCCURRED);
            }
            break;
        case SystemState::UPLOADING:
            if (actions_.upload) {
                bool success = actions_.upload();
                handleEvent(success ? StateEvent::UPLOAD_COMPLETE : StateEvent::ERROR_OCCURRED);
            }
            break;
        case SystemState::SHUTTING_DOWN:
            if (actions_.on_shutdown) {
                actions_.on_shutdown();
            }
            AD_INFO(StateMachine, "System shutdown complete");
            break;
        default:
            break;
    }
}

} // namespace aurora::state_machine
