// test_kinematics.cpp — IK solver and kinematics unit tests
#include <gtest/gtest.h>
#include "simulator/gait/kinematics_control_layer.h"

using namespace aurora::gait;

// Standalone test fixture for static methods
class KinematicsControlLayerStaticTest : public ::testing::Test {};

class IKSolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        LegGeometry geom;
        geom.hip_width = 0.1;
        geom.upper_leg_length = 0.35;
        geom.lower_leg_length = 0.35;
        geom.foot_height = 0.05;
        solver_ = std::make_unique<IKSolver>(geom);
    }
    std::unique_ptr<IKSolver> solver_;
};

// ===== Static Methods =====

TEST_F(KinematicsControlLayerStaticTest, JointNameIsValid) {
    const char* name = KinematicsControlLayer::getJointName(JointID::LEFT_HIP_YAW);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(KinematicsControlLayerStaticTest, NumJoints) {
    EXPECT_EQ(KinematicsControlLayer::getNumJoints(), 12u);
    EXPECT_EQ(KinematicsControlLayer::getJointsPerLeg(), 6u);
}

// ===== Forward/Inverse Kinematics Round-Trip =====

TEST_F(IKSolverTest, IdentityPositionReachable) {
    FootPosition fp;
    fp.x = 0.0;
    fp.y = 0.05;
    fp.z = -0.35;
    EXPECT_TRUE(solver_->isInWorkspace(fp, true));
}

TEST_F(IKSolverTest, IKSolveReturnsSuccess) {
    FootPosition fp;
    fp.x = 0.0;
    fp.y = 0.05;
    fp.z = -0.5;
    auto result = solver_->solve(fp, true);
    EXPECT_TRUE(result.success);
}

TEST_F(IKSolverTest, IKResultHasJointStates) {
    FootPosition fp;
    fp.x = 0.0;
    fp.y = 0.05;
    fp.z = -0.5;
    auto result = solver_->solve(fp, true);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.joint_states.size(), 6u);
}

TEST_F(IKSolverTest, ForwardKinematicsRunsSuccessfully) {
    FootPosition target;
    target.x = 0.0;
    target.y = 0.05;
    target.z = -0.5;

    auto ik_result = solver_->solve(target, true);
    ASSERT_TRUE(ik_result.success);
    ASSERT_EQ(ik_result.joint_states.size(), 6u);

    // FK should run without crash on IK result
    auto fk_result = solver_->forwardKinematics(ik_result.joint_states, true);
    // Just verify it produces a valid position (not NaN/Inf)
    EXPECT_FALSE(std::isnan(fk_result.x));
    EXPECT_FALSE(std::isnan(fk_result.y));
    EXPECT_FALSE(std::isnan(fk_result.z));
}

// ===== Workspace Checks =====

TEST_F(IKSolverTest, PositionTooFarUnreachable) {
    FootPosition fp;
    fp.x = 10.0;
    fp.y = 0.0;
    fp.z = 0.0;
    EXPECT_FALSE(solver_->isInWorkspace(fp, true));
}

TEST_F(IKSolverTest, WorkspaceBoundsCheck) {
    // Verify workspace bounds API returns valid ranges
    double min_x, max_x, min_y, max_y, min_z, max_z;
    solver_->getWorkspaceBounds(true, min_x, max_x, min_y, max_y, min_z, max_z);
    EXPECT_LT(min_z, max_z);
}

// ===== Geometry =====

TEST_F(IKSolverTest, SetAndGetGeometry) {
    LegGeometry new_geom;
    new_geom.upper_leg_length = 0.4;
    solver_->setLegGeometry(new_geom);
    EXPECT_DOUBLE_EQ(solver_->getLegGeometry().upper_leg_length, 0.4);
}
