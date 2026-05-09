// test_foot_trajectory.cpp — Foot trajectory generation unit tests
#include <gtest/gtest.h>
#include "simulator/gait/foot_trajectory_generator.h"
#include "simulator/gait/ruckig_trajectory_adapter.h"

using namespace aurora::gait;

class FootTrajectoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen_ = std::make_unique<FootTrajectoryGenerator>();
    }
    std::unique_ptr<FootTrajectoryGenerator> gen_;
};

// ===== Swing Trajectory =====

TEST_F(FootTrajectoryTest, GenerateSwingTrajectoryReturnsPoints) {
    SwingTrajectoryParams params;
    params.start = {0.0, 0.05, 0.0};
    params.end = {0.25, 0.05, 0.0};
    params.height = 0.05;
    params.duration = 0.32;

    auto traj = gen_->generateSwingTrajectory(params);
    EXPECT_GT(traj.size(), 0u);
}

TEST_F(FootTrajectoryTest, SwingStartMatchesParams) {
    SwingTrajectoryParams params;
    params.start = {0.0, 0.05, 0.0};
    params.end = {0.25, 0.05, 0.0};
    params.height = 0.05;
    params.duration = 0.32;

    auto traj = gen_->generateSwingTrajectory(params);
    ASSERT_GT(traj.size(), 0u);
    EXPECT_NEAR(traj.front().position.x, params.start.x, 0.01);
}

TEST_F(FootTrajectoryTest, SwingEndMatchesParams) {
    SwingTrajectoryParams params;
    params.start = {0.0, 0.05, 0.0};
    params.end = {0.25, 0.05, 0.0};
    params.height = 0.05;
    params.duration = 0.32;

    auto traj = gen_->generateSwingTrajectory(params);
    ASSERT_GT(traj.size(), 0u);
    EXPECT_NEAR(traj.back().position.x, params.end.x, 0.01);
}

TEST_F(FootTrajectoryTest, SwingReachesHeight) {
    SwingTrajectoryParams params;
    params.start = {0.0, 0.05, 0.0};
    params.end = {0.25, 0.05, 0.0};
    params.height = 0.05;
    params.duration = 0.32;

    auto traj = gen_->generateSwingTrajectory(params);
    double max_z = -1e9;
    for (const auto& pt : traj) {
        max_z = std::max(max_z, pt.position.z);
    }
    EXPECT_GE(max_z, 0.03);  // Should reach close to specified height
}

// ===== Stance Trajectory =====

TEST_F(FootTrajectoryTest, GenerateStanceTrajectoryReturnsPoints) {
    StanceTrajectoryParams params;
    params.fixed_position = {0.1, 0.05, 0.0};
    params.duration = 0.48;

    auto traj = gen_->generateStanceTrajectory(params);
    EXPECT_GT(traj.size(), 0u);
}

// ===== Single Point Computation =====

TEST_F(FootTrajectoryTest, ComputeSwingPointAtStart) {
    SwingTrajectoryParams params;
    params.start = {0.0, 0.05, 0.0};
    params.end = {0.25, 0.05, 0.0};
    params.height = 0.05;
    params.duration = 0.32;

    auto pt = gen_->computeSwingPoint(params, 0.0);
    EXPECT_NEAR(pt.position.x, params.start.x, 0.01);
}

TEST_F(FootTrajectoryTest, ComputeSwingPointAtEnd) {
    SwingTrajectoryParams params;
    params.start = {0.0, 0.05, 0.0};
    params.end = {0.25, 0.05, 0.0};
    params.height = 0.05;
    params.duration = 0.32;

    auto pt = gen_->computeSwingPoint(params, 1.0);
    EXPECT_NEAR(pt.position.x, params.end.x, 0.01);
}

// ===== Trajectory Metrics =====

TEST_F(FootTrajectoryTest, ComputeTrajectoryLength) {
    std::vector<TrajectoryPoint> traj;
    TrajectoryPoint p1, p2;
    p1.position = {0.0, 0.0, 0.0};
    p2.position = {1.0, 0.0, 0.0};
    traj.push_back(p1);
    traj.push_back(p2);

    double length = FootTrajectoryGenerator::computeTrajectoryLength(traj);
    EXPECT_NEAR(length, 1.0, 0.01);
}

// ===== Config =====

TEST_F(FootTrajectoryTest, SetConfig) {
    FootTrajectoryGeneratorConfig config;
    config.swing_style = SwingTrajectoryStyle::CUBIC_SPLINE;
    config.trajectory_resolution = 100;
    gen_->setConfig(config);

    EXPECT_EQ(gen_->getConfig().trajectory_resolution, 100);
}
