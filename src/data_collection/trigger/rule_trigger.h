//
// Created by your name on 25-11-17.
// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#pragma once

#include "trigger_base.h"
#include "trigger/common/trigger_checker.h"
#include "aurora_edge_runtime/srv/trigger_recording.hpp"

#include <unordered_map>
#include <string>
#include <functional>
#include <memory>
#include <rclcpp/rclcpp.hpp>

namespace aurora::collector {

// Trigger-level state (separate from system-level SystemState)
enum class TriggerState {
    IDLE,
    TRIGGERED,
    UNTRIGGERED
};

class RuleTrigger : public TriggerBase, public rclcpp::Node {
public:
    RuleTrigger();
    ~RuleTrigger() override = default;

    bool proc() override;
    bool checkCondition() override;
    void registerVariableGetter(const std::string& var_name,
                                std::function<TriggerChecker::Value()> getter) override;
    void OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& subject) override;

private:
    // ROS2 service client for triggering recording
    rclcpp::Client<aurora_edge_runtime::srv::TriggerRecording>::SharedPtr trigger_client_;

    // Async trigger method
    void triggerRecordingViaService(const TriggerContext& context);

    TriggerChecker trigger_checker_;
    std::atomic<TriggerState> current_state_{TriggerState::IDLE};
    std::unordered_map<std::string, std::function<TriggerChecker::Value()>> variable_getters_;

    mutable std::unordered_map<std::string, TriggerChecker::Value> current_variables_;
};

}
