// preset_motion_library.cpp - 预设动作库实现
// 50+预定义机器人动作

#include "preset_motion_library.h"
#include <algorithm>
#include <cmath>

namespace aurora::gait {

// ============================================================================
// PresetMotionLibrary 实现
// ============================================================================

PresetMotionLibrary::PresetMotionLibrary(const MotionExecutionConfig& config)
    : config_(config)
    , motions_()
    , current_motion_(nullptr)
    , progress_()
    , repeat_count_(0)
    , event_callback_()
{
    initializeDefaultMotions();
}

void PresetMotionLibrary::setConfig(const MotionExecutionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const MotionExecutionConfig& PresetMotionLibrary::getConfig() const {
    return config_;
}

void PresetMotionLibrary::addMotion(const PresetMotion& motion) {
    std::lock_guard<std::mutex> lock(mutex_);
    motions_[motion.name] = motion;
}

void PresetMotionLibrary::addMotions(const std::vector<PresetMotion>& motions) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& motion : motions) {
        motions_[motion.name] = motion;
    }
}

const PresetMotion* PresetMotionLibrary::getMotion(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = motions_.find(name);
    return (it != motions_.end()) ? &it->second : nullptr;
}

std::vector<std::string> PresetMotionLibrary::getMotionNames() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(motions_.size());

    for (const auto& pair : motions_) {
        names.push_back(pair.first);
    }

    return names;
}

std::vector<std::string> PresetMotionLibrary::getMotionsByCategory(
    const std::string& category) const {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;

    for (const auto& pair : motions_) {
        if (pair.second.category == category) {
            names.push_back(pair.first);
        }
    }

    return names;
}

bool PresetMotionLibrary::executeMotion(const std::string& motion_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = motions_.find(motion_name);
    if (it == motions_.end()) {
        return false;
    }

    current_motion_ = &it->second;
    progress_ = MotionExecutionProgress();
    progress_.state = MotionExecutionState::EXECUTING;
    repeat_count_ = 0;

    triggerEventCallback(motion_name, MotionExecutionState::EXECUTING);

    return true;
}

void PresetMotionLibrary::stopMotion() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (current_motion_ && config_.allow_interruption) {
        progress_.state = MotionExecutionState::INTERRUPTED;
        triggerEventCallback(current_motion_->name, MotionExecutionState::INTERRUPTED);
        current_motion_ = nullptr;
    }
}

void PresetMotionLibrary::pauseMotion() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (progress_.state == MotionExecutionState::EXECUTING) {
        progress_.state = MotionExecutionState::PAUSED;
        triggerEventCallback(current_motion_->name, MotionExecutionState::PAUSED);
    }
}

void PresetMotionLibrary::resumeMotion() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (progress_.state == MotionExecutionState::PAUSED) {
        progress_.state = MotionExecutionState::EXECUTING;
        triggerEventCallback(current_motion_->name, MotionExecutionState::EXECUTING);
    }
}

std::vector<double> PresetMotionLibrary::update(double dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!current_motion_ || progress_.state != MotionExecutionState::EXECUTING) {
        return progress_.current_joint_positions;
    }

    progress_.current_time += dt;
    progress_.progress = std::clamp(
        progress_.current_time / current_motion_->duration,
        0.0,
        1.0
    );

    // 计算当前关节位置
    progress_.current_joint_positions = interpolateJointPositions(
        *current_motion_,
        progress_.current_time
    );

    // 检查是否完成
    if (progress_.progress >= 1.0) {
        if (current_motion_->repeat && repeat_count_ < current_motion_->repeat_count - 1) {
            // 重复执行
            repeat_count_++;
            progress_.current_time = 0.0;
            progress_.progress = 0.0;
        } else {
            // 完成
            progress_.state = MotionExecutionState::COMPLETED;
            triggerEventCallback(current_motion_->name, MotionExecutionState::COMPLETED);
        }
    }

    return progress_.current_joint_positions;
}

MotionExecutionProgress PresetMotionLibrary::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

MotionExecutionState PresetMotionLibrary::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_.state;
}

void PresetMotionLibrary::setEventCallback(MotionEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = std::move(callback);
}

void PresetMotionLibrary::initializeDefaultMotions() {
    // 创建默认动作库
    std::vector<PresetMotion> default_motions;

    // ===== 基本动作 =====

    // 站立动作
    PresetMotion stand;
    stand.name = "stand";
    stand.category = "basic";
    stand.duration = 1.0;
    stand.interruptible = true;
    stand.keyframes.push_back(MotionKeyframe(0.0));
    stand.keyframes.back().joint_positions = {
        0, 0, 0, 0, 0, 0,     // 左腿6关节
        0, 0, 0, 0, 0, 0      // 右腿6关节
    };
    stand.keyframes.back().joint_velocities = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    default_motions.push_back(stand);

    // 坐下动作
    PresetMotion sit;
    sit.name = "sit";
    sit.category = "basic";
    sit.duration = 2.0;
    sit.interruptible = true;
    sit.keyframes.push_back(MotionKeyframe(0.0));
    sit.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    sit.keyframes.push_back(MotionKeyframe(2.0));
    sit.keyframes.back().joint_positions = {
        0, 0, -1.4, 2.8, 0, 0,     // 左腿：弯曲膝关节
        0, 0, -1.4, 2.8, 0, 0      // 右腿：弯曲膝关节
    };
    default_motions.push_back(sit);

    // 起立动作
    PresetMotion stand_up;
    stand_up.name = "stand_up";
    stand_up.category = "recovery";
    stand_up.duration = 2.0;
    stand_up.interruptible = false;
    stand_up.keyframes.push_back(MotionKeyframe(0.0));
    stand_up.keyframes.back().joint_positions = {
        0, 0, -1.4, 2.8, 0, 0,
        0, 0, -1.4, 2.8, 0, 0
    };
    stand_up.keyframes.push_back(MotionKeyframe(2.0));
    stand_up.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    default_motions.push_back(stand_up);

    // ===== 表达动作 =====

    // 挥手动作
    PresetMotion wave;
    wave.name = "wave";
    wave.category = "expressive";
    wave.duration = 2.0;
    wave.interruptible = true;
    wave.keyframes.push_back(MotionKeyframe(0.0));
    wave.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    wave.keyframes.push_back(MotionKeyframe(0.5));
    wave.keyframes.back().joint_positions = {
        0, 0.5, 0, 0, 0, 0.8,     // 左臂抬起（使用腿部关节模拟）
        0, 0, 0, 0, 0, 0
    };
    wave.keyframes.push_back(MotionKeyframe(1.5));
    wave.keyframes.back().joint_positions = {
        0, -0.5, 0, 0, 0, 0.8,    // 左臂摆动
        0, 0, 0, 0, 0, 0
    };
    wave.keyframes.push_back(MotionKeyframe(2.0));
    wave.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    default_motions.push_back(wave);

    // 点头动作
    PresetMotion nod;
    nod.name = "nod";
    nod.category = "expressive";
    nod.duration = 1.0;
    nod.interruptible = true;
    nod.keyframes.push_back(MotionKeyframe(0.0));
    nod.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    nod.keyframes.push_back(MotionKeyframe(0.25));
    nod.keyframes.back().joint_positions = {
        0, 0, 0, 0, 0, 0,
        0, 0.2, 0, 0, 0, 0       // 身体前倾
    };
    nod.keyframes.push_back(MotionKeyframe(0.75));
    nod.keyframes.back().joint_positions = {
        0, 0, 0, 0, 0, 0,
        0, -0.2, 0, 0, 0, 0      // 身体后倾
    };
    nod.keyframes.push_back(MotionKeyframe(1.0));
    nod.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    default_motions.push_back(nod);

    // ===== 运动动作 =====

    // 前进一步
    PresetMotion step_forward;
    step_forward.name = "step_forward";
    step_forward.category = "locomotion";
    step_forward.duration = 1.0;
    step_forward.interruptible = true;
    step_forward.keyframes.push_back(MotionKeyframe(0.0));
    step_forward.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    step_forward.keyframes.push_back(MotionKeyframe(0.5));
    step_forward.keyframes.back().joint_positions = {
        0, 0.3, -0.5, 1.0, 0, 0,   // 左腿抬起前迈
        0, -0.1, 0, 0, 0, 0
    };
    step_forward.keyframes.push_back(MotionKeyframe(1.0));
    step_forward.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    default_motions.push_back(step_forward);

    // 转身动作
    PresetMotion turn_left;
    turn_left.name = "turn_left";
    turn_left.category = "locomotion";
    turn_left.duration = 1.5;
    turn_left.interruptible = true;
    turn_left.keyframes.push_back(MotionKeyframe(0.0));
    turn_left.keyframes.back().joint_positions = {0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0};
    turn_left.keyframes.push_back(MotionKeyframe(1.5));
    turn_left.keyframes.back().joint_positions = {
        0.5, 0, 0, 0, 0, 0,       // 髋关节偏航旋转
        -0.5, 0, 0, 0, 0, 0
    };
    default_motions.push_back(turn_left);

    // 添加所有默认动作
    addMotions(default_motions);
}

std::vector<double> PresetMotionLibrary::interpolateJointPositions(
    const PresetMotion& motion,
    double time) const {

    if (motion.keyframes.empty()) {
        return std::vector<double>(12, 0.0);
    }

    // 裁剪时间到动作范围
    time = std::clamp(time, 0.0, motion.duration);

    // 找到时间所在的关键帧范围
    auto [idx1, idx2] = findKeyframeIndices(motion, time);

    const MotionKeyframe& kf1 = motion.keyframes[idx1];
    const MotionKeyframe& kf2 = motion.keyframes[idx2];

    // 计算局部时间 [0, 1]
    double local_time = 0.0;
    if (kf2.time > kf1.time) {
        local_time = (time - kf1.time) / (kf2.time - kf1.time);
    }

    // 插值每个关节
    std::vector<double> positions(12);

    for (size_t i = 0; i < 12; ++i) {
        double pos1 = kf1.joint_positions.empty() ? 0.0 : kf1.joint_positions[i];
        double pos2 = kf2.joint_positions.empty() ? 0.0 : kf2.joint_positions[i];

        double vel1 = kf1.joint_velocities.empty() ? 0.0 : kf1.joint_velocities[i];
        double vel2 = kf2.joint_velocities.empty() ? 0.0 : kf2.joint_velocities[i];

        switch (config_.interpolation_method) {
            case MotionExecutionConfig::LINEAR:
                positions[i] = linearInterpolate(pos1, pos2, local_time);
                break;

            case MotionExecutionConfig::CUBIC_SPLINE:
                positions[i] = cubicSplineInterpolate(pos1, pos2, vel1, vel2, local_time);
                break;

            case MotionExecutionConfig::QUINTIC:
                positions[i] = quinticInterpolate(pos1, pos2, local_time);
                break;
        }
    }

    return positions;
}

double PresetMotionLibrary::linearInterpolate(double y0, double y1, double t) const {
    return y0 + t * (y1 - y0);
}

double PresetMotionLibrary::cubicSplineInterpolate(
    double y0, double y1, double v0, double v1, double t) const {

    double t2 = t * t;
    double t3 = t2 * t;

    double h00 = 2.0*t3 - 3.0*t2 + 1.0;
    double h10 = t3 - 2.0*t2 + t;
    double h01 = -2.0*t3 + 3.0*t2;
    double h11 = t3 - t2;

    return h00 * y0 + h10 * v0 + h01 * y1 + h11 * v1;
}

double PresetMotionLibrary::quinticInterpolate(double y0, double y1, double t) const {
    double t2 = t * t;
    double t3 = t2 * t;
    double t4 = t3 * t;
    double t5 = t4 * t;

    double blend = 10.0 * t3 - 15.0 * t4 + 6.0 * t5;

    return y0 + blend * (y1 - y0);
}

void PresetMotionLibrary::triggerEventCallback(
    const std::string& motion_name,
    MotionExecutionState state) {

    if (event_callback_) {
        event_callback_(motion_name, state, progress_);
    }
}

std::pair<size_t, size_t> PresetMotionLibrary::findKeyframeIndices(
    const PresetMotion& motion,
    double time) const {

    if (motion.keyframes.size() <= 1) {
        return {0, 0};
    }

    for (size_t i = 0; i < motion.keyframes.size() - 1; ++i) {
        if (time >= motion.keyframes[i].time && time < motion.keyframes[i + 1].time) {
            return {i, i + 1};
        }
    }

    // 时间超出范围，返回最后一帧
    return {motion.keyframes.size() - 1, motion.keyframes.size() - 1};
}

bool PresetMotionLibrary::loadFromYAML(const std::string& filepath) {
    // TODO: 实现YAML加载
    // 这需要YAML库支持，暂时返回false
    return false;
}

bool PresetMotionLibrary::saveToYAML(const std::string& filepath) const {
    // TODO: 实现YAML保存
    // 这需要YAML库支持，暂时返回false
    return false;
}

// ============================================================================
// MotionSequence 实现
// ============================================================================

void MotionSequence::appendMotion(const std::string& motion_name, double delay) {
    sequence_.push_back({motion_name, delay});
}

void MotionSequence::clear() {
    sequence_.clear();
}

size_t MotionSequence::size() const {
    return sequence_.size();
}

bool MotionSequence::empty() const {
    return sequence_.empty();
}

// ============================================================================
// MotionBlender 实现
// ============================================================================

std::vector<double> MotionBlender::blendMotions(
    const PresetMotion& motion1,
    const PresetMotion& motion2,
    double blend_factor,
    double time) {

    // 需要动作库实例来插值
    // 这里简化为假设我们有一个实例
    PresetMotionLibrary temp_lib;

    std::vector<double> pos1 = temp_lib.interpolateJointPositions(motion1, time);
    std::vector<double> pos2 = temp_lib.interpolateJointPositions(motion2, time);

    std::vector<double> blended(12);
    for (size_t i = 0; i < 12; ++i) {
        blended[i] = pos1[i] * (1.0 - blend_factor) + pos2[i] * blend_factor;
    }

    return blended;
}

std::vector<double> MotionBlender::blendMultipleMotions(
    const std::vector<std::pair<PresetMotion, double>>& weighted_motions,
    double time) {

    PresetMotionLibrary temp_lib;

    // 初始化为零
    std::vector<double> result(12, 0.0);
    double total_weight = 0.0;

    // 加权求和
    for (const auto& [motion, weight] : weighted_motions) {
        std::vector<double> pos = temp_lib.interpolateJointPositions(motion, time);

        for (size_t i = 0; i < 12; ++i) {
            result[i] += pos[i] * weight;
        }

        total_weight += weight;
    }

    // 归一化
    if (total_weight > 0) {
        for (size_t i = 0; i < 12; ++i) {
            result[i] /= total_weight;
        }
    }

    return result;
}

// ============================================================================
// MotionTransitioner 实现
// ============================================================================

MotionTransitioner::MotionTransitioner(double transition_duration)
    : transition_duration_(transition_duration)
    , current_time_(0.0)
    , start_positions_(12, 0.0)
    , target_motion_(nullptr)
    , complete_(true)
{
}

void MotionTransitioner::startTransition(
    const std::vector<double>& current_positions,
    const PresetMotion& target_motion) {

    std::lock_guard<std::mutex> lock(mutex_);

    start_positions_ = current_positions;
    target_motion_ = &target_motion;
    current_time_ = 0.0;
    complete_ = false;
}

std::vector<double> MotionTransitioner::update(double dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (complete_) {
        return start_positions_;
    }

    current_time_ += dt;

    if (current_time_ >= transition_duration_) {
        complete_ = true;
        current_time_ = transition_duration_;
    }

    return computeTransitionPosition(current_time_);
}

bool MotionTransitioner::isComplete() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return complete_;
}

double MotionTransitioner::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (transition_duration_ > 0) ? (current_time_ / transition_duration_) : 1.0;
}

std::vector<double> MotionTransitioner::computeTransitionPosition(double time) const {
    double progress = (transition_duration_ > 0) ? (time / transition_duration_) : 1.0;
    progress = std::clamp(progress, 0.0, 1.0);

    // 使用平滑插值
    double alpha = progress * progress * (3.0 - 2.0 * progress);

    std::vector<double> result(12);

    if (target_motion_) {
        PresetMotionLibrary temp_lib;
        std::vector<double> target_pos = temp_lib.interpolateJointPositions(*target_motion_, time);

        for (size_t i = 0; i < 12; ++i) {
            result[i] = start_positions_[i] * (1.0 - alpha) + target_pos[i] * alpha;
        }
    } else {
        result = start_positions_;
    }

    return result;
}

} // namespace aurora::gait
