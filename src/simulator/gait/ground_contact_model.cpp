// ground_contact_model.cpp - 地面接触模型实现
// 实现弹簧-阻尼接触模型

#include "ground_contact_model.h"
#include <algorithm>
#include <cmath>

namespace aurora::gait {

// ============================================================================
// GroundContactModel 实现
// ============================================================================

GroundContactModel::GroundContactModel(const GroundContactModelConfig& config)
    : config_(config)
{
}

void GroundContactModel::setConfig(const GroundContactModelConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const GroundContactModelConfig& GroundContactModel::getConfig() const {
    return config_;
}

void GroundContactModel::setGroundProperties(const GroundProperties& properties) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.ground_properties = properties;
}

ContactInfo GroundContactModel::computeContact(
    const FootPosition& foot_position,
    const FootVelocity& foot_velocity) {

    std::lock_guard<std::mutex> lock(mutex_);

    ContactInfo info;
    info.foot_position = foot_position;
    info.foot_velocity = foot_velocity;

    // 计算穿透深度
    info.penetration_depth = computePenetrationDepth(foot_position);

    // 判断接触状态
    if (info.penetration_depth > config_.minimum_penetration_for_contact) {
        // 有接触
        double penetration = info.penetration_depth;

        // 计算垂直方向的反作用力（弹簧-阻尼）
        // F = K * x - B * v
        double spring_force = computeSpringForce(penetration);
        double damping_force = computeDampingForce(foot_velocity.vz);
        double normal_force = spring_force - damping_force;
        normal_force = std::max(0.0, normal_force);  // 地面只能推，不能拉

        info.contact_force.fz = normal_force;

        // 计算摩擦力
        info.friction_force = computeFrictionForce(normal_force, foot_velocity);

        // 水平方向的摩擦力
        FootVelocity horizontal_vel(foot_velocity.vx, foot_velocity.vy, 0);
        double horizontal_speed = horizontal_vel.horizontalNorm();

        if (horizontal_speed > 1e-6) {
            // 摩擦力方向与速度方向相反
            double scale = info.friction_force / horizontal_speed;
            info.contact_force.fx = -foot_velocity.vx * scale;
            info.contact_force.fy = -foot_velocity.vy * scale;
        } else {
            info.contact_force.fx = 0.0;
            info.contact_force.fy = 0.0;
        }

        // 检查是否打滑
        info.is_slipping = isSlipping(normal_force, info.contact_force.horizontalNorm());
        info.slip_velocity = horizontal_speed;

        // 判断接触状态类型
        double total_force = info.contact_force.norm();
        if (total_force > config_.impact_force_threshold) {
            info.state = ContactState::IMPACT;
        } else if (info.is_slipping) {
            info.state = ContactState::SLIPPING;
        } else {
            info.state = ContactState::CONTACTING;
        }
    } else {
        // 无接触
        info.state = ContactState::NO_CONTACT;
        info.contact_force = ContactForce();
        info.friction_force = 0.0;
        info.is_slipping = false;
        info.slip_velocity = 0.0;
    }

    return info;
}

std::vector<ContactInfo> GroundContactModel::computeContacts(
    const std::vector<FootPosition>& foot_positions,
    const std::vector<FootVelocity>& foot_velocities) {

    size_t num_legs = std::min(foot_positions.size(), foot_velocities.size());
    std::vector<ContactInfo> contacts;
    contacts.reserve(num_legs);

    for (size_t i = 0; i < num_legs; ++i) {
        contacts.push_back(computeContact(foot_positions[i], foot_velocities[i]));
    }

    return contacts;
}

bool GroundContactModel::hasContact(const FootPosition& foot_position) const {
    double penetration = computePenetrationDepth(foot_position);
    return penetration > config_.minimum_penetration_for_contact;
}

ContactForce GroundContactModel::computeGroundReactionForce(
    const FootPosition& foot_position,
    const FootVelocity& foot_velocity) {

    ContactInfo info = computeContact(foot_position, foot_velocity);
    return info.contact_force;
}

double GroundContactModel::computeFrictionForce(
    double normal_force,
    const FootVelocity& foot_velocity) const {

    if (normal_force <= 0) {
        return 0.0;
    }

    const GroundProperties& props = config_.ground_properties;

    // 计算水平速度
    double horizontal_speed = std::sqrt(
        foot_velocity.vx * foot_velocity.vx +
        foot_velocity.vy * foot_velocity.vy
    );

    if (horizontal_speed < props.friction_velocity_threshold) {
        // 静摩擦
        return normal_force * props.static_friction;
    } else {
        // 动摩擦
        double friction_coef = computeFrictionCoefficient(horizontal_speed);
        return normal_force * friction_coef;
    }
}

bool GroundContactModel::isSlipping(
    double normal_force,
    double horizontal_force) const {

    if (normal_force <= 0) {
        return false;
    }

    const GroundProperties& props = config_.ground_properties;

    // 计算实际摩擦系数
    double actual_friction = horizontal_force / normal_force;

    // 如果超过静摩擦系数，则打滑
    return actual_friction > props.static_friction;
}

double GroundContactModel::computePenetrationDepth(
    const FootPosition& foot_position) const {

    // 地面高度以下的穿透深度
    double penetration = config_.ground_height - foot_position.z;

    // 考虑足端半径
    penetration += config_.foot_radius;

    return std::max(0.0, penetration);
}

double GroundContactModel::computeSpringForce(double penetration) const {
    // F = K * x
    return config_.ground_properties.spring_stiffness * penetration;
}

double GroundContactModel::computeDampingForce(double velocity) const {
    // F = B * v
    return config_.ground_properties.damping_coefficient * velocity;
}

double GroundContactModel::computeFrictionCoefficient(double velocity) const {
    const GroundProperties& props = config_.ground_properties;

    // 在静摩擦和动摩擦之间插值
    double ratio = velocity / props.friction_velocity_threshold;
    ratio = std::clamp(ratio, 0.0, 1.0);

    // 线性插值
    return props.static_friction * (1.0 - ratio) +
           props.dynamic_friction * ratio;
}

// ============================================================================
// ContactHistoryRecorder 实现
// ============================================================================

ContactHistoryRecorder::ContactHistoryRecorder(size_t max_entries)
    : max_entries_(max_entries)
{
}

void ContactHistoryRecorder::addContact(const ContactInfo& contact) {
    std::lock_guard<std::mutex> lock(mutex_);

    history_.push_back(contact);

    // 限制历史记录大小
    if (history_.size() > max_entries_) {
        history_.erase(history_.begin());
    }
}

const std::vector<ContactInfo>& ContactHistoryRecorder::getHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
}

void ContactHistoryRecorder::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

ContactInfo ContactHistoryRecorder::getLatestContact() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (history_.empty()) {
        return ContactInfo();
    }

    return history_.back();
}

size_t ContactHistoryRecorder::countSlipEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return std::count_if(history_.begin(), history_.end(),
                        [](const ContactInfo& info) {
                            return info.is_slipping;
                        });
}

double ContactHistoryRecorder::getAveragePenetration() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (history_.empty()) {
        return 0.0;
    }

    double total = 0.0;
    size_t count = 0;

    for (const auto& info : history_) {
        if (info.state != ContactState::NO_CONTACT) {
            total += info.penetration_depth;
            ++count;
        }
    }

    return (count > 0) ? (total / count) : 0.0;
}

// ============================================================================
// ImpactDetector 实现
// ============================================================================

ImpactDetector::ImpactDetector(double force_threshold)
    : force_threshold_(force_threshold)
    , max_impact_force_(0.0)
    , previous_contact_()
{
}

bool ImpactDetector::detectImpact(const ContactInfo& contact) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否有接触状态变化
    bool previous_contact = previous_contact_.state != ContactState::NO_CONTACT;
    bool current_contact = contact.state != ContactState::NO_CONTACT;

    // 检测冲击：从无接触变为有接触，且力超过阈值
    bool impact = false;

    if (!previous_contact && current_contact) {
        double force = contact.contact_force.norm();
        if (force > force_threshold_) {
            impact = true;
            max_impact_force_ = std::max(max_impact_force_, force);
        }
    }

    previous_contact_ = contact;

    return impact;
}

double ImpactDetector::getMaxImpactForce() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return max_impact_force_;
}

void ImpactDetector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    max_impact_force_ = 0.0;
    previous_contact_ = ContactInfo();
}

} // namespace aurora::gait
