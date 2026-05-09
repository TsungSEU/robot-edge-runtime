// kinematics_control_layer.h - 运动学控制层
// 连接足端轨迹和关节角度，实现Unitree风格的运动学控制
//
// 核心设计原则：
// 1. 解析式IK求解器：快速、稳定、无奇点
// 2. 关节限位检查：确保所有角度在安全范围内
// 3. 关节速度约束：平滑关节运动
// 4. 关节空间平滑：减少振动

#ifndef KINEMATICS_CONTROL_LAYER_H
#define KINEMATICS_CONTROL_LAYER_H

#include "gait_state_machine.h"
#include "foot_trajectory_generator.h"
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <mutex>
#include <string>
#include <cmath>

namespace aurora::gait {

/**
 * @brief 关节标识
 */
enum class JointID : int {
    // 左腿关节
    LEFT_HIP_YAW = 0,       // 左髋偏航
    LEFT_HIP_ROLL = 1,      // 左髋外展
    LEFT_HIP_PITCH = 2,     // 左髋屈伸
    LEFT_KNEE_PITCH = 3,    // 左膝屈伸
    LEFT_ANKLE_PITCH = 4,   // 左踝屈伸
    LEFT_ANKLE_ROLL = 5,    // 左踝内外翻

    // 右腿关节
    RIGHT_HIP_YAW = 6,      // 右髋偏航
    RIGHT_HIP_ROLL = 7,     // 右髋外展
    RIGHT_HIP_PITCH = 8,    // 右髋屈伸
    RIGHT_KNEE_PITCH = 9,   // 右膝屈伸
    RIGHT_ANKLE_PITCH = 10, // 右踝屈伸
    RIGHT_ANKLE_ROLL = 11,  // 右踝内外翻

    UNKNOWN = -1
};

/**
 * @brief 关节状态
 */
struct JointState {
    double position;        // 关节角度 (弧度)
    double velocity;        // 关节角速度 (弧度/秒)
    double acceleration;    // 关节角加速度 (弧度/秒²)
    double effort;          // 关节力矩 (N·m)

    JointState()
        : position(0), velocity(0), acceleration(0), effort(0)
    {}

    JointState(double pos, double vel = 0, double acc = 0, double eff = 0)
        : position(pos), velocity(vel), acceleration(acc), effort(eff)
    {}
};

/**
 * @brief 关节限位
 */
struct JointLimits {
    double min_position;    // 最小角度 (弧度)
    double max_position;    // 最大角度 (弧度)
    double max_velocity;    // 最大角速度 (弧度/秒)
    double max_effort;      // 最大力矩 (N·m)

    bool has_position_limits;
    bool has_velocity_limits;
    bool has_effort_limits;

    JointLimits()
        : min_position(-M_PI), max_position(M_PI)
        , max_velocity(10.0), max_effort(100.0)
        , has_position_limits(true)
        , has_velocity_limits(true)
        , has_effort_limits(true)
    {}

    JointLimits(double min_pos, double max_pos, double max_vel = 10.0, double max_eff = 100.0)
        : min_position(min_pos), max_position(max_pos)
        , max_velocity(max_vel), max_effort(max_eff)
        , has_position_limits(true)
        , has_velocity_limits(true)
        , has_effort_limits(true)
    {}

    /**
     * @brief 检查位置是否在限位内
     */
    bool checkPosition(double pos) const {
        if (!has_position_limits) return true;
        return pos >= min_position && pos <= max_position;
    }

    /**
     * @brief 裁剪位置到限位范围
     */
    double clipPosition(double pos) const {
        if (!has_position_limits) return pos;
        return std::clamp(pos, min_position, max_position);
    }

    /**
     * @brief 检查速度是否在限位内
     */
    bool checkVelocity(double vel) const {
        if (!has_velocity_limits) return true;
        return std::abs(vel) <= max_velocity;
    }

    /**
     * @brief 裁剪速度到限位范围
     */
    double clipVelocity(double vel) const {
        if (!has_velocity_limits) return vel;
        return std::clamp(vel, -max_velocity, max_velocity);
    }
};

/**
 * @brief 腿部几何参数
 */
struct LegGeometry {
    double hip_width;           // 髋关节宽度 (米)
    double upper_leg_length;    // 大腿长度 (米)
    double lower_leg_length;    // 小腿长度 (米)
    double foot_height;         // 脚厚度 (米)
    double hip_offset_z;        // 髋关节相对躯干的垂直偏移 (米)

    LegGeometry()
        : hip_width(0.1)
        , upper_leg_length(0.35)
        , lower_leg_length(0.35)
        , foot_height(0.05)
        , hip_offset_z(0.0)
    {}

    /**
     * @brief 获取总腿长
     */
    double getTotalLegLength() const {
        return upper_leg_length + lower_leg_length;
    }
};

/**
 * @brief IK求解结果
 */
struct IKResult {
    std::vector<JointState> joint_states;   // 关节状态
    bool success;                            // 是否成功求解
    std::string error_message;               // 错误信息
    double residual;                         // 残差（用于数值IK）
    int iterations;                          // 迭代次数

    IKResult()
        : joint_states(6, JointState())
        , success(false)
        , residual(0)
        , iterations(0)
    {}

    IKResult(size_t num_joints)
        : joint_states(num_joints, JointState())
        , success(false)
        , residual(0)
        , iterations(0)
    {}
};

/**
 * @brief IK求解器类型
 */
enum class IKSolverType : uint8_t {
    ANALYTICAL,      // 解析式求解器（快速、精确）
    NUMERICAL,       // 数值求解器（灵活、较慢）
    HYBRID,          // 混合求解器（结合两者优点）
    DampedLeastSquares  // 阻尼最小二乘（处理奇异点）
};

/**
 * @brief IK求解器配置
 */
struct IKSolverConfig {
    IKSolverType solver_type;           // 求解器类型
    double max_iterations;              // 最大迭代次数（数值求解）
    double tolerance;                   // 收敛容差
    double damping_factor;              // 阻尼因子
    bool check_joint_limits;            // 是否检查关节限位
    bool check_workspace;               // 是否检查工作空间
    double max_reach_extension;         // 最大延伸比例 (0-1)

    IKSolverConfig()
        : solver_type(IKSolverType::ANALYTICAL)
        , max_iterations(100)
        , tolerance(1e-6)
        , damping_factor(0.01)
        , check_joint_limits(true)
        , check_workspace(true)
        , max_reach_extension(0.98)
    {}
};

/**
 * @brief 运动学控制层配置
 */
struct KinematicsControlConfig {
    double update_rate;                 // 更新频率 (Hz)
    double max_joint_acceleration;      // 最大关节加速度 (rad/s²)
    double velocity_smoothing_factor;   // 速度平滑因子 [0, 1]
    double position_smoothing_factor;   // 位置平滑因子 [0, 1]
    bool enable_velocity_profile;       // 是否启用速度剖面
    bool enable_collision_check;        // 是否启用碰撞检查

    KinematicsControlConfig()
        : update_rate(50.0)
        , max_joint_acceleration(50.0)
        , velocity_smoothing_factor(0.8)
        , position_smoothing_factor(0.5)
        , enable_velocity_profile(true)
        , enable_collision_check(false)
    {}
};

/**
 * @brief 逆运动学求解器
 *
 * 基于腿部几何参数，将足端位置转换为关节角度
 */
class IKSolver {
public:
    explicit IKSolver(const LegGeometry& geometry = LegGeometry(),
                      const IKSolverConfig& config = IKSolverConfig());

    ~IKSolver() = default;

    /**
     * @brief 设置腿部几何
     */
    void setLegGeometry(const LegGeometry& geometry);

    /**
     * @brief 获取腿部几何
     */
    const LegGeometry& getLegGeometry() const;

    /**
     * @brief 设置关节限位
     */
    void setJointLimits(const std::vector<JointLimits>& limits);

    /**
     * @brief 获取关节限位
     */
    const std::vector<JointLimits>& getJointLimits() const;

    /**
     * @brief 求解逆运动学
     * @param foot_position 足端位置 (相对于髋关节)
     * @param is_left 是否是左腿
     * @return IK求解结果
     */
    IKResult solve(const FootPosition& foot_position, bool is_left) const;

    /**
     * @brief 求解逆运动学（带当前关节状态，用于连续性）
     */
    IKResult solve(const FootPosition& foot_position,
                   const std::vector<JointState>& current_joints,
                   bool is_left) const;

    /**
     * @brief 正运动学
     * @param joint_states 关节状态
     * @param is_left 是否是左腿
     * @return 足端位置 (相对于髋关节)
     */
    FootPosition forwardKinematics(const std::vector<JointState>& joint_states,
                                   bool is_left) const;

    /**
     * @brief 检查足端位置是否在工作空间内
     */
    bool isInWorkspace(const FootPosition& foot_position, bool is_left) const;

    /**
     * @brief 获取工作空间边界
     */
    void getWorkspaceBounds(bool is_left,
                            double& min_x, double& max_x,
                            double& min_y, double& max_y,
                            double& min_z, double& max_z) const;

private:
    LegGeometry geometry_;
    IKSolverConfig config_;
    std::vector<JointLimits> joint_limits_;
    mutable std::mutex mutex_;

    // 解析式IK求解
    IKResult solveAnalytical(const FootPosition& foot_position, bool is_left) const;

    // 数值IK求解
    IKResult solveNumerical(const FootPosition& foot_position,
                            const std::vector<JointState>& initial_joints,
                            bool is_left) const;

    // 阻尼最小二乘IK求解
    IKResult solveDampedLS(const FootPosition& foot_position,
                           const std::vector<JointState>& initial_joints,
                           bool is_left) const;

    // 关节限位检查和裁剪
    bool applyJointLimits(IKResult& result) const;
};

/**
 * @brief 运动学控制层
 *
 * 协调足端轨迹和关节角度，实现平滑的关节运动
 */
class KinematicsControlLayer {
public:
    explicit KinematicsControlLayer(
        const KinematicsControlConfig& config = KinematicsControlConfig());

    ~KinematicsControlLayer() = default;

    /**
     * @brief 设置IK求解器
     */
    void setIKSolver(std::shared_ptr<IKSolver> solver);

    /**
     * @brief 获取IK求解器
     */
    std::shared_ptr<IKSolver> getIKSolver() const;

    /**
     * @brief 更新关节状态
     * @param dt 时间步长
     * @param target_positions 目标足端位置 (每条腿)
     * @param current_joints 当前关节状态
     * @return 新的关节状态
     */
    std::vector<JointState> update(double dt,
                                   const std::vector<FootPosition>& target_positions,
                                   const std::vector<JointState>& current_joints);

    /**
     * @brief 从轨迹更新关节状态
     */
    std::vector<JointState> updateFromTrajectory(double dt,
                                                  const std::vector<TrajectoryPoint>& trajectory_points,
                                                  LegID leg_id,
                                                  const std::vector<JointState>& current_joints);

    /**
     * @brief 设置关节目标位置
     */
    void setJointTargets(const std::vector<JointState>& targets);

    /**
     * @brief 获取当前关节状态
     */
    const std::vector<JointState>& getJointStates() const;

    /**
     * @brief 获取关节速度
     */
    const std::vector<double>& getJointVelocities() const;

    /**
     * @brief 设置配置
     */
    void setConfig(const KinematicsControlConfig& config);

    /**
     * @brief 获取配置
     */
    const KinematicsControlConfig& getConfig() const;

    /**
     * @brief 重置控制层
     */
    void reset();

    /**
     * @brief 获取关节名称
     */
    static const char* getJointName(JointID joint_id);

    /**
     * @brief 获取关节数量
     */
    static constexpr size_t getNumJoints() { return 12; }

    /**
     * @brief 获取单腿关节数量
     */
    static constexpr size_t getJointsPerLeg() { return 6; }

private:
    KinematicsControlConfig config_;
    std::shared_ptr<IKSolver> ik_solver_;
    std::vector<JointState> joint_states_;
    std::vector<double> joint_velocities_;
    mutable std::mutex mutex_;

    /**
     * @brief 平滑关节速度
     */
    void smoothJointVelocities(std::vector<JointState>& joints,
                                const std::vector<JointState>& prev_joints,
                                double dt);

    /**
     * @brief 应用加速度约束
     */
    void applyAccelerationConstraints(std::vector<JointState>& joints,
                                       const std::vector<JointState>& prev_joints,
                                       double dt);

    /**
     * @brief 验证关节状态
     */
    bool validateJointStates(const std::vector<JointState>& joints) const;
};

/**
 * @brief 多腿运动学控制器
 *
 * 管理多条腿的运动学控制
 */
class MultiLegKinematicsController {
public:
    explicit MultiLegKinematicsController(
        const KinematicsControlConfig& config = KinematicsControlConfig());

    ~MultiLegKinematicsController() = default;

    /**
     * @brief 设置腿数量
     */
    void setNumLegs(size_t num_legs);

    /**
     * @brief 更新所有腿的关节状态
     */
    std::vector<std::vector<JointState>> update(double dt,
                                                 const std::vector<FootPosition>& target_positions);

    /**
     * @brief 获取指定腿的关节状态
     */
    const std::vector<JointState>& getLegJointStates(LegID leg_id) const;

    /**
     * @brief 获取所有关节状态
     */
    const std::vector<std::vector<JointState>>& getAllJointStates() const;

    /**
     * @brief 设置腿部几何
     */
    void setLegGeometry(const LegGeometry& geometry);

    /**
     * @brief 设置关节限位
     */
    void setJointLimits(const std::vector<std::vector<JointLimits>>& limits);

    /**
     * @brief 重置所有控制器
     */
    void reset();

private:
    std::vector<std::unique_ptr<KinematicsControlLayer>> controllers_;
    std::vector<std::shared_ptr<IKSolver>> ik_solvers_;
    LegGeometry leg_geometry_;
    mutable std::mutex mutex_;
};

/**
 * @brief 工具函数
 */
class KinematicsUtils {
public:
    /**
     * @brief 关节ID转字符串
     */
    static const char* jointIDToString(JointID joint_id);

    /**
     * @brief 字符串转关节ID
     */
    static JointID stringToJointID(const std::string& str);

    /**
     * @brief 腿ID到关节索引的映射
     */
    static std::vector<int> legIDToJointIndices(LegID leg_id);

    /**
     * @brief 判断是否是左腿关节
     */
    static bool isLeftLegJoint(JointID joint_id);

    /**
     * @brief 判断是否是右腿关节
     */
    static bool isRightLegJoint(JointID joint_id);

    /**
     * @brief 获取腿部ID
     */
    static LegID getLegIDFromJoint(JointID joint_id);

    /**
     * @brief 关节角度归一化到 [-π, π]
     */
    static double normalizeAngle(double angle);

    /**
     * @brief 计算角度差
     */
    static double angleDifference(double angle1, double angle2);

    /**
     * @brief 插值关节角度
     */
    static JointState interpolateJoint(const JointState& j1, const JointState& j2, double t);
};

} // namespace aurora::gait

#endif // KINEMATICS_CONTROL_LAYER_H
