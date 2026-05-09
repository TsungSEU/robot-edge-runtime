//
// Created by your name on 25-11-17.
// Copyright (c) 2025 T3CAIC. All rights reserved.
//

#include "trigger_manager.h"
#include "common/log/logger.h"
#include <cmath>
#include <algorithm>

namespace aurora::collector {

TriggerManager::TriggerManager()
    : last_trigger_position_(0.0, 0.0)
    , last_trigger_time_(std::chrono::steady_clock::now()) {
}

bool TriggerManager::initialize(const StrategyConfig& config) {
    if (!initTriggers(config)) {
        AD_ERROR(TriggerManager, "Trigger initialization failed.");
        return false;
    }

    if (!initTriggerChecker(trigger_instances_))
    {
        AD_ERROR(TriggerManager, "Trigger checker initialization failed.");
        return false;
    }

    AD_INFO(TriggerManager, "TriggerManager initialized with Gait-Based trigger mode");
    AD_INFO(TriggerManager, "  min_step_distance: %.2f m", gait_config_.min_step_distance);
    AD_INFO(TriggerManager, "  cooldown_duration: %.2f s", gait_config_.cooldown_duration);

    return true;
}

bool TriggerManager::initTriggers(const StrategyConfig& config) {
    thread_pool_ = std::make_shared<ThreadPool>(1);
    std::vector<std::pair<std::string, int>> enabled_triggers;

    //获取触发器使能列表和线程数
    {
        for (const auto& s : config.strategies) {
            if (s.trigger.enabled) {
                enabled_triggers.emplace_back(
                    s.trigger.triggerId,
                    s.trigger.priority
                );
            }
        }
    }

    if (enabled_triggers.empty())
    {
        AD_WARN(TriggerManager, "No enabled triggers found.");
        return false;
    }

    //创建触发器实例并初始化
    bool success = true;
    //算子创建和初始化
    auto trigger = createTrigger();
    if (!trigger) {
        AD_ERROR(TriggerManager, "Trigger creation failed.");
        success = false;

        return false;
    }

    for (const auto& [id, priority] : enabled_triggers)
    {
        bool init_success = trigger->init(id, config);
        if (!init_success) {
            AD_ERROR(TriggerManager, "Trigger init failed for %s", id.c_str());
            success = false;
            return false;
        }
    }

    trigger_instances_ = std::dynamic_pointer_cast<TriggerBase>(trigger);

    return success;
}

bool TriggerManager::initTriggerChecker(std::shared_ptr<TriggerBase> trigger) {
    if (!trigger) return false;
    trigger->registerVariableGetter("speed", [this]() -> TriggerChecker::Value {
        return 5;
        // return message_provider_->getChassisVehicleMps(); // m/s
    });

    trigger->registerVariableGetter("automode", [this]() -> TriggerChecker::Value {
        // return message_provider_->getAutoModeEnable();
    });

    trigger->registerVariableGetter("gear", [this]() -> TriggerChecker::Value {
        // return message_provider_->getGear();
    });

    trigger->registerVariableGetter("aeb_decel_req", [this]() -> TriggerChecker::Value {
        // return message_provider_->getAebDecelReq();
    });

    return true;
}


std::shared_ptr<TriggerBase> TriggerManager::createTrigger() {
    // If trigger_id is not registered, create as RuleTrigger anyway to handle all configured triggers
    return std::make_shared<RuleTrigger>();
}

std::shared_ptr<TriggerBase> TriggerManager::getTrigger() const {
    return trigger_instances_ ? trigger_instances_ : nullptr;
}

void TriggerManager::setGaitTriggerConfig(const GaitTriggerConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    gait_config_ = config;
    AD_INFO(TriggerManager, "Gait trigger config updated:");
    AD_INFO(TriggerManager, "  min_step_distance: %.2f m", gait_config_.min_step_distance);
    AD_INFO(TriggerManager, "  cooldown_duration: %.2f s", gait_config_.cooldown_duration);
}

bool TriggerManager::shouldTriggerGaitBased(const Point& position) {
    // 计算与上次触发位置的距离
    double dx = position.x - last_trigger_position_.x;
    double dy = position.y - last_trigger_position_.y;
    double distance = std::sqrt(dx * dx + dy * dy);

    // 检查冷却时间
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_trigger_time_).count() / 1000.0;

    if (time_since_last < gait_config_.cooldown_duration) {
        AD_DEBUG(TriggerManager, "In cooldown: %.2f s < %.2f s",
                time_since_last, gait_config_.cooldown_duration);
        return false;
    }

    // 检查最小步长距离
    if (distance < gait_config_.min_step_distance) {
        AD_DEBUG(TriggerManager, "Distance too small: %.2f m < %.2f m",
                distance, gait_config_.min_step_distance);
        return false;
    }

    // 检查是否与历史采集点过于接近
    for (const auto& hist_point : collection_history_) {
        double hdx = position.x - hist_point.x;
        double hdy = position.y - hist_point.y;
        double hdist = std::sqrt(hdx * hdx + hdy * hdy);
        if (hdist < gait_config_.min_step_distance * 0.5) {
            AD_DEBUG(TriggerManager, "Too close to history point: %.2f m", hdist);
            return false;
        }
    }

    // 所有检查通过，允许触发
    last_trigger_position_ = position;
    last_trigger_time_ = now;

    // 添加到历史记录
    collection_history_.push_back(position);
    if (collection_history_.size() > MAX_HISTORY_SIZE) {
        collection_history_.erase(collection_history_.begin());
    }

    AD_INFO(TriggerManager, "Trigger accepted at (%.2f, %.2f), step distance: %.2f m, time: %.2f s",
           position.x, position.y, distance, time_since_last);

    return true;
}

bool TriggerManager::shouldTrigger(const Point& position) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // 对于基于步态的触发，position参数（规划waypoint）被忽略
    // 而是使用内部追踪的机器人实际位置（last_robot_position_）
    // 这是因为步态触发应该基于实际足端着地位置，而非规划路径点

    // 如果没有机器人位置信息，回退到使用规划位置（兼容旧逻辑）
    if (!robot_position_initialized_) {
        AD_DEBUG(TriggerManager, "Robot position not initialized, using planned waypoint");
        return shouldTriggerGaitBased(position);
    }

    // 使用机器人实际位置进行触发判断
    Point actual_position = last_robot_position_;
    AD_DEBUG(TriggerManager, "Using actual robot position (%.2f, %.2f) instead of waypoint (%.2f, %.2f)",
             actual_position.x, actual_position.y, position.x, position.y);

    return shouldTriggerGaitBased(actual_position);
}

bool TriggerManager::isInSparseArea(const Point& position) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (const auto& sparse_region : sparse_areas_) {
        if (sparse_region.contains(position)) {
            return true;
        }
    }

    return false;
}

double TriggerManager::getDistanceToNearestSparseArea(const Point& position) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    double min_distance = std::numeric_limits<double>::max();

    // 遍历所有稀疏区域，计算最小距离
    for (const auto& sparse_region : sparse_areas_) {
        double distance = sparse_region.getDistance(position);
        if (distance < min_distance) {
            min_distance = distance;
        }
    }

    // 若点在稀疏区域内，返回 0
    if (min_distance == 0.0 || isInSparseArea(position)) {
        return 0.0;
    }

    return min_distance;
}

bool TriggerManager::start() {
    // 启动触发管理器
    current_state = state_machine::SystemState::IDLE;
    AD_INFO(TriggerManager, "Trigger manager started");
    return true;
}

void TriggerManager::setTriggerCallback(TriggerCallback cb) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    callback_ = std::move(cb);
}

void TriggerManager::notifyTriggerContext(const TriggerContext& ctx) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (callback_) callback_(ctx);
}

void TriggerManager::notifyPositionVisited(const Point& pos, uint64_t timestamp) {
    // Create minimal TriggerContext for notification
    TriggerContext ctx;
    ctx.pos = pos;
    ctx.triggerTimestamp = timestamp;
    // Note: businessType, triggerId, triggerDesc are left empty as they're not needed for position tracking

    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (callback_) callback_(ctx);
}

void TriggerManager::updateRobotPosition(const Point& position) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    last_robot_position_ = position;
    robot_position_initialized_ = true;
}

}
