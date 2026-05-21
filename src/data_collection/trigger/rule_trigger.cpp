//
// Created by your name on 25-11-17.
// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#include "rule_trigger.h"
#include "aurora_edge_runtime/srv/trigger_recording.hpp"
#include "common/utils/utils.h"
#include "trigger/ITrigger.h"
#include <thread>

namespace aurora::collector {

RuleTrigger::RuleTrigger() : rclcpp::Node("rule_trigger") {
    // Initialize service client
    trigger_client_ = this->create_client<aurora_edge_runtime::srv::TriggerRecording>("/robot/trigger");

    // Wait for service availability with timeout
    if (!trigger_client_->wait_for_service(std::chrono::seconds(5))) {
        RCLCPP_WARN(this->get_logger(),
                   "TriggerRecording service not available after 5s, will retry on trigger");
    }
}

bool RuleTrigger::proc() {
    if (current_state_ == TriggerState::TRIGGERED) {
        AD_WARN(RuleTrigger, "Already triggered, skipping.");
        return true;
    }

    bool condition_met = checkCondition();
    if (!condition_met) {
        // 条件不满足，重置状态为未触发
        if (current_state_ != TriggerState::UNTRIGGERED) {
            current_state_ = TriggerState::UNTRIGGERED;
            AD_INFO(RuleTrigger, "Condition not met, reset state to Untriggered.");
        }
        return false;
    }

    // 条件满足，执行触发逻辑
    TriggerContext context;
    context.triggerTimestamp = common::GetCurrentTimestamp();
    context.triggerId = trigger_obj_->triggerId;
    context.triggerDesc = trigger_obj_->triggerDesc;
    context.pos = Point{0.0, 0.0};  // Default position

    current_state_.store(TriggerState::TRIGGERED);

    // Trigger recording via ROS2 service (async)
    triggerRecordingViaService(context);

    return true;
}

bool RuleTrigger::checkCondition() {
    // 解析触发条件表达式
    if (!trigger_checker_.parse(trigger_obj_->triggerCondition)) {
        AD_ERROR(RuleTrigger, "Failed to parse condition: %s",
                 trigger_checker_.lastError().c_str());
        return false;
    }
    current_variables_.clear();

    // 获取所有需要的变量值
    for (const auto& [var_name, getter] : variable_getters_) {
        try {
            current_variables_[var_name] = getter();
            AD_ERROR(RuleTrigger, "Got variable '%s'", var_name.c_str());
        } catch (const std::exception& e) {
            AD_ERROR(RuleTrigger, "Failed to get variable '%s': %s",
                     var_name.c_str(), e.what());
            return false;
        }
    }

    // 检查条件
    bool result = trigger_checker_.executeCheck(current_variables_);

    return result;
}

void RuleTrigger::registerVariableGetter(const std::string& var_name,
                                         std::function<TriggerChecker::Value()> getter) {
    variable_getters_[var_name] = std::move(getter);
}

void RuleTrigger::OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& subject)
{
    // AD_INFO(RuleTrigger, "Received message on topic %s", topic.c_str());
}

void RuleTrigger::triggerRecordingViaService(const TriggerContext& context) {
    if (!trigger_client_->service_is_ready()) {
        AD_ERROR(RuleTrigger, "TriggerRecording service not available");
        return;
    }

    auto request = std::make_shared<aurora_edge_runtime::srv::TriggerRecording::Request>();
    request->business_type = context.businessType.empty() ? "rule_trigger" : context.businessType;
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

    std::thread([this, future_shared]() {
        try {
            auto response = future_shared->future.get();
            if (response->success) {
                AD_INFO(RuleTrigger, "Recording triggered: %s", response->bag_path.c_str());
            } else {
                AD_WARN(RuleTrigger, "Failed: %s (cooldown: %lu ms)",
                       response->message.c_str(), response->cooldown_remaining);
            }
        } catch (const std::exception& e) {
            AD_ERROR(RuleTrigger, "Service call failed: %s", e.what());
        }
    }).detach();
}

}
