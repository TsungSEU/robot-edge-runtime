// test_safety_system.cpp — SafetySystem unit tests
#include <gtest/gtest.h>
#include "simulator/gait/safety_system.h"

using namespace aurora::gait;

class SafetySystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        SafetySystemConfig config;
        config.emergency_stop_enabled = true;
        config.input_timeout_enabled = false;
        config.watchdog_enabled = false;
        config.stability_check_enabled = false;
        safety_system_ = std::make_unique<SafetySystem>(config);
    }
    std::unique_ptr<SafetySystem> safety_system_;
};

// ===== Emergency Stop =====

TEST_F(SafetySystemTest, EmergencyStopNotActiveInitially) {
    EXPECT_FALSE(safety_system_->isEmergencyStopActive());
}

TEST_F(SafetySystemTest, TriggerEmergencyStop) {
    safety_system_->triggerEmergencyStop("Test trigger");
    EXPECT_TRUE(safety_system_->isEmergencyStopActive());
}

TEST_F(SafetySystemTest, ClearEmergencyStop) {
    safety_system_->triggerEmergencyStop("Test");
    safety_system_->clearEmergencyStop();
    EXPECT_FALSE(safety_system_->isEmergencyStopActive());
}

TEST_F(SafetySystemTest, EmergencyStopStaysActive) {
    safety_system_->triggerEmergencyStop("Test");
    // Should still be active after update
    safety_system_->update(0.02);
    EXPECT_TRUE(safety_system_->isEmergencyStopActive());
}

// ===== Config =====

TEST_F(SafetySystemTest, SetAndGetConfig) {
    SafetySystemConfig new_config;
    new_config.emergency_stop_enabled = false;
    safety_system_->setConfig(new_config);
    EXPECT_FALSE(safety_system_->getConfig().emergency_stop_enabled);
}

// ===== Event Callback =====

TEST_F(SafetySystemTest, EventCallbackOnEmergencyStop) {
    SafetyEvent received_event = SafetyEvent::EMERGENCY_STOP_TRIGGERED;
    int callback_count = 0;

    safety_system_->setEventCallback([&](SafetyEvent event, const std::string& msg) {
        received_event = event;
        callback_count++;
    });

    safety_system_->triggerEmergencyStop("callback test");
    EXPECT_GE(callback_count, 1);
}

// ===== Reset =====

TEST_F(SafetySystemTest, ResetClearsEmergencyStop) {
    safety_system_->triggerEmergencyStop("before reset");
    safety_system_->reset();
    EXPECT_FALSE(safety_system_->isEmergencyStopActive());
}

// ===== Event Description =====

TEST_F(SafetySystemTest, GetEventDescription) {
    std::string desc = SafetySystem::getEventDescription(SafetyEvent::EMERGENCY_STOP_TRIGGERED);
    EXPECT_FALSE(desc.empty());
}

// ===== Status =====

TEST_F(SafetySystemTest, StatusAvailable) {
    auto& status = safety_system_->getStatus();
    // Status should be accessible without crash
    (void)status;
}

// ===== Joint Limits API =====

TEST_F(SafetySystemTest, SetJointLimitsVector) {
    JointLimit limit;
    limit.min_position = -M_PI;
    limit.max_position = M_PI;
    limit.max_velocity = 10.0;
    safety_system_->setJointLimits(std::vector<JointLimit>(6, limit));
    // Should not crash — validates the API works
}
