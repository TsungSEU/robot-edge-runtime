// test_gait_trigger_chain.cpp — Integration test for gait trigger logic
#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include "rl_planning_infer/maps/costmap.h"

// We test GaitTrigger's pure logic methods by creating a test harness
// that doesn't require ROS2 service/client infrastructure

// Re-implement the trigger logic from gait_trigger.h for testing
// This avoids the ROS2 service client creation in the constructor
// In production, this logic lives inside GaitTrigger::shouldTriggerCollection

class GaitTriggerLogicTest : public ::testing::Test {
protected:
    double min_step_distance_ = 0.15;       // 15cm
    double min_collection_interval_ = 1.0;  // 1 second

    // Mirror of GaitTrigger::shouldTriggerCollection logic
    bool shouldTrigger(const Point& current_pos,
                       const Point& last_collect_pos,
                       const std::chrono::steady_clock::time_point& last_collect_time) {
        // Check distance
        double dx = current_pos.x - last_collect_pos.x;
        double dy = current_pos.y - last_collect_pos.y;
        double distance = std::sqrt(dx * dx + dy * dy);
        if (distance < min_step_distance_) {
            return false;
        }

        // Check interval
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - last_collect_time).count();
        if (elapsed < min_collection_interval_) {
            return false;
        }

        return true;
    }
};

// ===== Distance-based triggering =====

TEST_F(GaitTriggerLogicTest, TriggerForLargeDistance) {
    Point current(5.0, 5.0);
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    EXPECT_TRUE(shouldTrigger(current, last, last_time));
}

TEST_F(GaitTriggerLogicTest, NoTriggerForSamePosition) {
    Point current(1.0, 1.0);
    Point last(1.0, 1.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    EXPECT_FALSE(shouldTrigger(current, last, last_time));
}

TEST_F(GaitTriggerLogicTest, NoTriggerForSmallDistance) {
    Point current(0.1, 0.0);  // 10cm
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    EXPECT_FALSE(shouldTrigger(current, last, last_time));
}

TEST_F(GaitTriggerLogicTest, TriggerAtExactThreshold) {
    Point current(0.15, 0.0);  // Exactly 15cm
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    EXPECT_TRUE(shouldTrigger(current, last, last_time));
}

// ===== Interval-based triggering =====

TEST_F(GaitTriggerLogicTest, NoTriggerTooFrequent) {
    Point current(5.0, 5.0);
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now();  // Just now
    EXPECT_FALSE(shouldTrigger(current, last, last_time));
}

TEST_F(GaitTriggerLogicTest, TriggerAfterInterval) {
    Point current(5.0, 5.0);
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    EXPECT_TRUE(shouldTrigger(current, last, last_time));
}

// ===== Diagonal distance =====

TEST_F(GaitTriggerLogicTest, TriggerForDiagonalMovement) {
    Point current(0.2, 0.2);  // ~28cm diagonal
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    EXPECT_TRUE(shouldTrigger(current, last, last_time));
}

TEST_F(GaitTriggerLogicTest, NoTriggerForSmallDiagonal) {
    Point current(0.1, 0.1);  // ~14cm diagonal
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    EXPECT_FALSE(shouldTrigger(current, last, last_time));
}

// ===== Parameter changes =====

TEST_F(GaitTriggerLogicTest, IncreasedMinDistanceRejectsMore) {
    min_step_distance_ = 0.5;  // 50cm
    Point current(0.3, 0.0);   // 30cm
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    EXPECT_FALSE(shouldTrigger(current, last, last_time));
}

TEST_F(GaitTriggerLogicTest, IncreasedIntervalRejectsMore) {
    min_collection_interval_ = 10.0;
    Point current(5.0, 5.0);
    Point last(0.0, 0.0);
    auto last_time = std::chrono::steady_clock::now() - std::chrono::seconds(3);
    EXPECT_FALSE(shouldTrigger(current, last, last_time));
}

// ===== State Machine + Trigger Chain =====

#include "state_machine/state_machine.h"

class StateMachineChainTest : public ::testing::Test {
protected:
    void SetUp() override {
        sm_ = &aurora::state_machine::StateMachine::getInstance();
        sm_->initialize();
        sm_->setCurrentState(aurora::state_machine::SystemState::INITIALIZING);
        sm_->setDegradeMode(false);
    }
    aurora::state_machine::StateMachine* sm_;
};

TEST_F(StateMachineChainTest, FullCycleInitToIdle) {
    sm_->setCurrentState(aurora::state_machine::SystemState::INITIALIZING);
    sm_->handleEvent(aurora::state_machine::StateEvent::INIT_COMPLETE);
    EXPECT_EQ(sm_->getCurrentState(), aurora::state_machine::SystemState::IDLE);
}

TEST_F(StateMachineChainTest, FullCyclePlanToNavigate) {
    sm_->setCurrentState(aurora::state_machine::SystemState::IDLE);
    sm_->handleEvent(aurora::state_machine::StateEvent::PLAN_REQUEST);
    EXPECT_EQ(sm_->getCurrentState(), aurora::state_machine::SystemState::PLANNING);
}

TEST_F(StateMachineChainTest, ShutdownFromIdle) {
    sm_->setCurrentState(aurora::state_machine::SystemState::IDLE);
    sm_->handleEvent(aurora::state_machine::StateEvent::SHUTDOWN_REQUEST);
    EXPECT_EQ(sm_->getCurrentState(), aurora::state_machine::SystemState::SHUTTING_DOWN);
}

TEST_F(StateMachineChainTest, ErrorAndRecovery) {
    // PLANNING handles ERROR_OCCURRED → transitions to ERROR
    sm_->setCurrentState(aurora::state_machine::SystemState::PLANNING);
    sm_->handleEvent(aurora::state_machine::StateEvent::ERROR_OCCURRED);
    EXPECT_EQ(sm_->getCurrentState(), aurora::state_machine::SystemState::ERROR);

    sm_->handleEvent(aurora::state_machine::StateEvent::RECOVERY_REQUEST);
    EXPECT_EQ(sm_->getCurrentState(), aurora::state_machine::SystemState::IDLE);
}
