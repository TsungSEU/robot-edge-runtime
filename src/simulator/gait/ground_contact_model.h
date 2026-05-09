// ground_contact_model.h - 地面接触模型
// 实现弹簧-阻尼接触模型，用于更真实的足端接触仿真
//
// 核心功能：
// 1. 弹簧-阻尼接触模型
// 2. 地面柔顺性（软/硬表面）
// 3. 摩擦模型（滑移检测）
// 4. 冲击力计算

#ifndef GROUND_CONTACT_MODEL_H
#define GROUND_CONTACT_MODEL_H

#include "gait_state_machine.h"
#include <vector>
#include <mutex>
#include <cmath>

namespace aurora::gait {

/**
 * @brief 地面属性
 */
enum class GroundSurfaceType : uint8_t {
    HARD,           // 硬地面（混凝土、石头）
    MEDIUM,         // 中等硬度（草地、泥土）
    SOFT,           // 软地面（沙地、雪地）
    CUSTOM          // 自定义
};

/**
 * @brief 接触状态
 */
enum class ContactState : uint8_t {
    NO_CONTACT,         // 无接触
    CONTACTING,         // 接触中
    SLIPPING,           // 打滑
    IMPACT              // 冲击
};

/**
 * @brief 地面属性配置
 */
struct GroundProperties {
    GroundSurfaceType surface_type;

    // 弹簧-阻尼参数
    double spring_stiffness;      // K (N/m)
    double damping_coefficient;   // B (Ns/m)

    // 摩擦参数
    double static_friction;       // μ_s
    double dynamic_friction;      // μ_d
    double friction_velocity_threshold;  // 打滑速度阈值 (m/s)

    // 地面柔顺性
    double compliance;            // 柔顺性 [0, 1]
    double penetration_depth;     // 最大穿透深度 (m)

    GroundProperties()
        : surface_type(GroundSurfaceType::HARD)
        , spring_stiffness(10000.0)
        , damping_coefficient(500.0)
        , static_friction(0.8)
        , dynamic_friction(0.6)
        , friction_velocity_threshold(0.01)
        , compliance(0.0)
        , penetration_depth(0.01)
    {}

    /**
     * @brief 获取预设地面属性
     */
    static GroundProperties hardSurface() {
        return GroundProperties();  // 默认就是硬地面
    }

    static GroundProperties mediumSurface() {
        GroundProperties props;
        props.surface_type = GroundSurfaceType::MEDIUM;
        props.spring_stiffness = 5000.0;
        props.damping_coefficient = 300.0;
        props.static_friction = 0.7;
        props.dynamic_friction = 0.5;
        props.compliance = 0.3;
        props.penetration_depth = 0.02;
        return props;
    }

    static GroundProperties softSurface() {
        GroundProperties props;
        props.surface_type = GroundSurfaceType::SOFT;
        props.spring_stiffness = 2000.0;
        props.damping_coefficient = 200.0;
        props.static_friction = 0.5;
        props.dynamic_friction = 0.4;
        props.compliance = 0.6;
        props.penetration_depth = 0.05;
        return props;
    }
};

/**
 * @brief 接触力
 */
struct ContactForce {
    double fx;  // X方向力 (N)
    double fy;  // Y方向力 (N)
    double fz;  // Z方向力（垂直向上为正）

    ContactForce(double x = 0, double y = 0, double z = 0)
        : fx(x), fy(y), fz(z)
    {}

    /**
     * @brief 计算合力大小
     */
    double norm() const {
        return std::sqrt(fx*fx + fy*fy + fz*fz);
    }

    /**
     * @brief 计算水平力大小
     */
    double horizontalNorm() const {
        return std::sqrt(fx*fx + fy*fy);
    }
};

/**
 * @brief 接触状态信息
 */
struct ContactInfo {
    ContactState state;
    FootPosition foot_position;
    FootVelocity foot_velocity;

    double penetration_depth;     // 穿透深度 (m)
    ContactForce contact_force;   // 接触力 (N)
    double friction_force;        // 摩擦力 (N)

    bool is_slipping;             // 是否打滑
    double slip_velocity;         // 滑移速度 (m/s)

    ContactInfo()
        : state(ContactState::NO_CONTACT)
        , foot_position()
        , foot_velocity()
        , penetration_depth(0.0)
        , contact_force()
        , friction_force(0.0)
        , is_slipping(false)
        , slip_velocity(0.0)
    {}
};

/**
 * @brief 地面接触模型配置
 */
struct GroundContactModelConfig {
    GroundProperties ground_properties;

    // 碰撞检测参数
    double ground_height;          // 地面高度 (m)
    double foot_radius;            // 足端半径 (m) - 用于球形足端模型

    // 计算参数
    double minimum_penetration_for_contact;  // 最小穿透深度触发接触 (m)
    double impact_force_threshold;           // 冲击力阈值 (N)

    GroundContactModelConfig()
        : ground_properties()
        , ground_height(0.0)
        , foot_radius(0.02)
        , minimum_penetration_for_contact(1e-4)
        , impact_force_threshold(100.0)
    {}
};

/**
 * @brief 地面接触模型
 *
 * 基于弹簧-阻尼模型的地面接触仿真
 */
class GroundContactModel {
public:
    explicit GroundContactModel(
        const GroundContactModelConfig& config = {});

    ~GroundContactModel() = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const GroundContactModelConfig& config);

    /**
     * @brief 获取配置
     */
    const GroundContactModelConfig& getConfig() const;

    /**
     * @brief 设置地面属性
     */
    void setGroundProperties(const GroundProperties& properties);

    /**
     * @brief 计算接触力
     * @param foot_position 足端位置
     * @param foot_velocity 足端速度
     * @return 接触信息
     */
    ContactInfo computeContact(
        const FootPosition& foot_position,
        const FootVelocity& foot_velocity);

    /**
     * @brief 批量计算多条腿的接触力
     */
    std::vector<ContactInfo> computeContacts(
        const std::vector<FootPosition>& foot_positions,
        const std::vector<FootVelocity>& foot_velocities);

    /**
     * @brief 检查是否有接触
     */
    bool hasContact(const FootPosition& foot_position) const;

    /**
     * @brief 计算地面对足端的反作用力
     */
    ContactForce computeGroundReactionForce(
        const FootPosition& foot_position,
        const FootVelocity& foot_velocity);

    /**
     * @brief 计算摩擦力
     */
    double computeFrictionForce(
        double normal_force,
        const FootVelocity& foot_velocity) const;

    /**
     * @brief 判断是否打滑
     */
    bool isSlipping(
        double normal_force,
        double horizontal_force) const;

    /**
     * @brief 计算穿透深度
     */
    double computePenetrationDepth(const FootPosition& foot_position) const;

private:
    GroundContactModelConfig config_;
    mutable std::mutex mutex_;

    /**
     * @brief 计算弹簧力
     */
    double computeSpringForce(double penetration) const;

    /**
     * @brief 计算阻尼力
     */
    double computeDampingForce(double velocity) const;

    /**
     * @brief 计算摩擦系数
     */
    double computeFrictionCoefficient(double velocity) const;
};

/**
 * @brief 接触历史记录器
 *
 * 记录接触状态历史用于打滑检测和分析
 */
class ContactHistoryRecorder {
public:
    explicit ContactHistoryRecorder(size_t max_entries = 1000);

    /**
     * @brief 添加接触记录
     */
    void addContact(const ContactInfo& contact);

    /**
     * @brief 获取接触历史
     */
    const std::vector<ContactInfo>& getHistory() const;

    /**
     * @brief 清空历史
     */
    void clear();

    /**
     * @brief 获取最近的接触记录
     */
    ContactInfo getLatestContact() const;

    /**
     * @brief 统计打滑事件次数
     */
    size_t countSlipEvents() const;

    /**
     * @brief 计算平均穿透深度
     */
    double getAveragePenetration() const;

private:
    std::vector<ContactInfo> history_;
    size_t max_entries_;
    mutable std::mutex mutex_;
};

/**
 * @brief 冲击检测器
 *
 * 检测足端冲击事件
 */
class ImpactDetector {
public:
    explicit ImpactDetector(double force_threshold = 100.0);

    /**
     * @brief 检测冲击
     */
    bool detectImpact(const ContactInfo& contact);

    /**
     * @brief 获取最大冲击力
     */
    double getMaxImpactForce() const;

    /**
     * @brief 重置
     */
    void reset();

private:
    double force_threshold_;
    double max_impact_force_;
    ContactInfo previous_contact_;
    mutable std::mutex mutex_;
};

} // namespace aurora::gait

#endif // GROUND_CONTACT_MODEL_H
