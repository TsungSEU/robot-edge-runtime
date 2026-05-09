// test_state_machine.cpp — StateMachine unit tests
#include <gtest/gtest.h>
#include "state_machine/state_machine.h"

using namespace aurora::state_machine;

class StateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        sm_ = &StateMachine::getInstance();
        sm_->initialize();
        // Reset to known state
        sm_->setCurrentState(SystemState::INITIALIZING);
        sm_->setDegradeMode(false);
    }

    StateMachine* sm_;
};

// ===== State String Conversion =====

TEST_F(StateMachineTest, StateToStringAllStates) {
    EXPECT_STREQ(StateMachine::stateToString(SystemState::INITIALIZING), "INITIALIZING");
    EXPECT_STREQ(StateMachine::stateToString(SystemState::IDLE), "IDLE");
    EXPECT_STREQ(StateMachine::stateToString(SystemState::PLANNING), "PLANNING");
    EXPECT_STREQ(StateMachine::stateToString(SystemState::NAVIGATING), "NAVIGATING");
    EXPECT_STREQ(StateMachine::stateToString(SystemState::DATA_COLLECTING), "DATA_COLLECTING");
    EXPECT_STREQ(StateMachine::stateToString(SystemState::UPLOADING), "UPLOADING");
    EXPECT_STREQ(StateMachine::stateToString(SystemState::ERROR), "ERROR");
    EXPECT_STREQ(StateMachine::stateToString(SystemState::SHUTTING_DOWN), "SHUTTING_DOWN");
}

TEST_F(StateMachineTest, EventToStringAllEvents) {
    EXPECT_STREQ(StateMachine::eventToString(StateEvent::INIT_COMPLETE), "INIT_COMPLETE");
    EXPECT_STREQ(StateMachine::eventToString(StateEvent::PLAN_REQUEST), "PLAN_REQUEST");
    EXPECT_STREQ(StateMachine::eventToString(StateEvent::PLAN_COMPLETE), "PLAN_COMPLETE");
    EXPECT_STREQ(StateMachine::eventToString(StateEvent::ERROR_OCCURRED), "ERROR_OCCURRED");
    EXPECT_STREQ(StateMachine::eventToString(StateEvent::SHUTDOWN_REQUEST), "SHUTDOWN_REQUEST");
}

// ===== Initialization =====

TEST_F(StateMachineTest, InitializeTransitionsToIdle) {
    sm_->initialize();
    // initialize() starts in INITIALIZING and sends INIT_COMPLETE → IDLE
    EXPECT_EQ(sm_->getCurrentState(), SystemState::IDLE);
}

// ===== INITIALIZING → IDLE =====

TEST_F(StateMachineTest, InitCompleteTransitionsToIdle) {
    sm_->setCurrentState(SystemState::INITIALIZING);
    sm_->handleEvent(StateEvent::INIT_COMPLETE);
    EXPECT_EQ(sm_->getCurrentState(), SystemState::IDLE);
}

// ===== IDLE → PLANNING =====

TEST_F(StateMachineTest, PlanRequestTransitionsToPlanning) {
    sm_->setCurrentState(SystemState::IDLE);
    sm_->handleEvent(StateEvent::PLAN_REQUEST);
    EXPECT_EQ(sm_->getCurrentState(), SystemState::PLANNING);
}

// ===== PLANNING → NAVIGATING =====

TEST_F(StateMachineTest, PlanCompleteTransitionsToNavigating) {
    sm_->setCurrentState(SystemState::PLANNING);
    sm_->handleEvent(StateEvent::PLAN_COMPLETE);
    EXPECT_EQ(sm_->getCurrentState(), SystemState::NAVIGATING);
}

// ===== Any → SHUTTING_DOWN =====

TEST_F(StateMachineTest, ShutdownFromAnyState) {
    sm_->setCurrentState(SystemState::IDLE);
    sm_->handleEvent(StateEvent::SHUTDOWN_REQUEST);
    EXPECT_EQ(sm_->getCurrentState(), SystemState::SHUTTING_DOWN);
}

// ===== Error Handling =====

TEST_F(StateMachineTest, ErrorFromAnyState) {
    sm_->setCurrentState(SystemState::NAVIGATING);
    sm_->handleEvent(StateEvent::ERROR_OCCURRED);
    EXPECT_EQ(sm_->getCurrentState(), SystemState::ERROR);
}

TEST_F(StateMachineTest, RecoveryFromError) {
    sm_->setCurrentState(SystemState::ERROR);
    sm_->handleEvent(StateEvent::RECOVERY_REQUEST);
    EXPECT_EQ(sm_->getCurrentState(), SystemState::IDLE);
}

// ===== Audit Callback =====

TEST_F(StateMachineTest, AuditCallbackReceivesTransitions) {
    SystemState from_state = SystemState::INITIALIZING;
    SystemState to_state = SystemState::INITIALIZING;
    StateEvent received_event = StateEvent::INIT_COMPLETE;
    int callback_count = 0;

    sm_->setAuditLogCallback([&](SystemState from, SystemState to,
                                  StateEvent event,
                                  const std::chrono::steady_clock::time_point&) {
        from_state = from;
        to_state = to;
        received_event = event;
        callback_count++;
    });

    sm_->setCurrentState(SystemState::INITIALIZING);
    sm_->handleEvent(StateEvent::INIT_COMPLETE);

    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(from_state, SystemState::INITIALIZING);
    EXPECT_EQ(to_state, SystemState::IDLE);
    EXPECT_EQ(received_event, StateEvent::INIT_COMPLETE);
}

// ===== Degrade Mode =====

TEST_F(StateMachineTest, DegradeModeControl) {
    EXPECT_FALSE(sm_->isDegradeMode());
    sm_->setDegradeMode(true);
    EXPECT_TRUE(sm_->isDegradeMode());
    sm_->setDegradeMode(false);
    EXPECT_FALSE(sm_->isDegradeMode());
}

// ===== Singleton =====

TEST_F(StateMachineTest, SingletonIdentity) {
    auto& sm1 = StateMachine::getInstance();
    auto& sm2 = StateMachine::getInstance();
    EXPECT_EQ(&sm1, &sm2);
}

// ===== Action Callbacks =====

TEST_F(StateMachineTest, ActionCallbacksExecuted) {
    bool plan_called = false;
    bool navigate_called = false;

    ActionCallbacks actions;
    actions.plan = [&]() { plan_called = true; return true; };
    actions.navigate_step = [&]() { navigate_called = true; return false; };
    actions.should_collect = [&]() { return false; };
    actions.collect = [&]() { return true; };
    actions.upload = [&]() { return true; };
    actions.on_shutdown = [&]() {};

    sm_->setActionCallbacks(actions);

    sm_->setCurrentState(SystemState::IDLE);
    sm_->handleEvent(StateEvent::PLAN_REQUEST);
    EXPECT_TRUE(plan_called);
}
