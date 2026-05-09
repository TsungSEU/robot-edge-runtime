// foot_trajectory_generator.cpp - Unitree风格足端轨迹生成器实现
// 性能优化：使用快速三角函数替代标准库实现

#include "foot_trajectory_generator.h"
#include "ruckig_trajectory_adapter.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cmath>

// 第三阶段性能优化：快速三角函数
#include "common/performance_utils.h"

// 快速三角函数缓存 - 避免重复计算
// 使用 thread_local 缓存，每个线程独立
namespace aurora::gait {

namespace {
    thread_local const aurora::performance::FastTrigonometry* g_fast_trig = nullptr;

    inline const aurora::performance::FastTrigonometry& getFastTrig() {
        if (g_fast_trig == nullptr) {
            g_fast_trig = &aurora::performance::getFastTrig();
        }
        return *g_fast_trig;
    }
} // anonymous namespace

// ========== FootTrajectoryGenerator ==========

FootTrajectoryGenerator::FootTrajectoryGenerator(
    const FootTrajectoryGeneratorConfig& config)
    : config_(config)
    , ruckig_adapter_(nullptr)
    , ruckig_config_(nullptr)
    , ruckig_initialized_(false)
{
}

void FootTrajectoryGenerator::setConfig(const FootTrajectoryGeneratorConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const FootTrajectoryGeneratorConfig& FootTrajectoryGenerator::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

std::vector<TrajectoryPoint> FootTrajectoryGenerator::generateSwingTrajectory(
    const SwingTrajectoryParams& params) const {

    std::vector<TrajectoryPoint> trajectory;
    trajectory.reserve(static_cast<size_t>(config_.trajectory_resolution));

    for (int i = 0; i <= config_.trajectory_resolution; ++i) {
        double progress = static_cast<double>(i) / config_.trajectory_resolution;
        TrajectoryPoint point = computeSwingPoint(params, progress);
        trajectory.push_back(point);
    }

    // 检查约束
    if (config_.check_constraints) {
        if (!checkTrajectory(trajectory)) {
            trajectory = clipTrajectory(trajectory);
        }
    }

    // 平滑处理
    if (config_.enable_smoothing) {
        trajectory = smoothTrajectory(trajectory);
    }

    return trajectory;
}

std::vector<TrajectoryPoint> FootTrajectoryGenerator::generateStanceTrajectory(
    const StanceTrajectoryParams& params) const {

    std::vector<TrajectoryPoint> trajectory;
    int num_points = static_cast<int>(config_.trajectory_resolution * params.duration / 0.32);
    num_points = std::max(10, num_points);

    double dt = params.duration / num_points;

    for (int i = 0; i <= num_points; ++i) {
        double time = i * dt;
        TrajectoryPoint point = computeStancePoint(params, time);
        trajectory.push_back(point);
    }

    return trajectory;
}

TrajectoryPoint FootTrajectoryGenerator::computeSwingPoint(
    const SwingTrajectoryParams& params,
    double progress) const {

    // 裁剪进度到 [0, 1]
    progress = std::clamp(progress, 0.0, 1.0);

    switch (config_.swing_style) {
        case SwingTrajectoryStyle::SINUSOID:
            return generateSinusoidSwingPoint(params, progress);
        case SwingTrajectoryStyle::CUBIC_SPLINE:
            return generateCubicSplineSwingPoint(params, progress);
        case SwingTrajectoryStyle::QUINTIC:
            return generateQuinticSwingPoint(params, progress);
        case SwingTrajectoryStyle::CYCLOID:
            return generateCycloidSwingPoint(params, progress);
        case SwingTrajectoryStyle::COMBO:
            return generateComboSwingPoint(params, progress);
        case SwingTrajectoryStyle::RUCKIG:
            return generateRuckigSwingPoint(params, progress);
        default:
            return generateComboSwingPoint(params, progress);
    }
}

TrajectoryPoint FootTrajectoryGenerator::computeStancePoint(
    const StanceTrajectoryParams& params,
    double time) const {

    TrajectoryPoint point;
    point.position = params.fixed_position;
    point.velocity = FootVelocity(0, 0, 0);
    point.time = time;
    point.progress = std::min(1.0, time / params.duration);

    return point;
}

void FootTrajectoryGenerator::setConstraints(const TrajectoryConstraints& constraints) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.constraints = constraints;
}

const TrajectoryConstraints& FootTrajectoryGenerator::getConstraints() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.constraints;
}

bool FootTrajectoryGenerator::checkTrajectory(
    const std::vector<TrajectoryPoint>& trajectory) const {

    return std::all_of(trajectory.begin(), trajectory.end(),
        [this](const TrajectoryPoint& p) {
            return config_.constraints.checkPosition(p.position);
        });
}

std::vector<TrajectoryPoint> FootTrajectoryGenerator::clipTrajectory(
    std::vector<TrajectoryPoint> trajectory) const {

    for (auto& point : trajectory) {
        point.position = config_.constraints.clipPosition(point.position);
    }
    return trajectory;
}

std::vector<TrajectoryPoint> FootTrajectoryGenerator::smoothTrajectory(
    const std::vector<TrajectoryPoint>& trajectory) const {

    if (trajectory.size() < 3) {
        return trajectory;
    }

    std::vector<TrajectoryPoint> smoothed = trajectory;
    double alpha = config_.smoothing_factor;

    // 前向滤波
    for (size_t i = 1; i < smoothed.size() - 1; ++i) {
        smoothed[i].position.x = alpha * smoothed[i].position.x +
            (1 - alpha) * 0.5 * (smoothed[i-1].position.x + smoothed[i+1].position.x);
        smoothed[i].position.y = alpha * smoothed[i].position.y +
            (1 - alpha) * 0.5 * (smoothed[i-1].position.y + smoothed[i+1].position.y);
        smoothed[i].position.z = alpha * smoothed[i].position.z +
            (1 - alpha) * 0.5 * (smoothed[i-1].position.z + smoothed[i+1].position.z);
    }

    return smoothed;
}

double FootTrajectoryGenerator::computeTrajectoryLength(
    const std::vector<TrajectoryPoint>& trajectory) {

    if (trajectory.empty()) return 0.0;

    double length = 0.0;
    for (size_t i = 1; i < trajectory.size(); ++i) {
        double dx = trajectory[i].position.x - trajectory[i-1].position.x;
        double dy = trajectory[i].position.y - trajectory[i-1].position.y;
        double dz = trajectory[i].position.z - trajectory[i-1].position.z;
        length += std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    return length;
}

double FootTrajectoryGenerator::computeTrajectoryDuration(
    const std::vector<TrajectoryPoint>& trajectory) {

    if (trajectory.size() < 2) return 0.0;
    return trajectory.back().time - trajectory.front().time;
}

// ========== 正弦轨迹生成 ==========

TrajectoryPoint FootTrajectoryGenerator::generateSinusoidSwingPoint(
    const SwingTrajectoryParams& params,
    double progress) const {

    TrajectoryPoint point;
    point.progress = progress;
    point.time = progress * params.duration;

    // 性能优化：使用快速三角函数
    const auto& trig = getFastTrig();

    // 使用正弦函数的平方: z = H * sin²(π * progress)
    // 这保证了起落点速度为0，轨迹平滑
    double sin_pi = trig.fastSin(M_PI * progress);
    double z_offset = params.height * sin_pi * sin_pi;

    // XY平面线性插值
    point.position.x = params.start.x + (params.end.x - params.start.x) * progress;
    point.position.y = params.start.y + (params.end.y - params.start.y) * progress;
    point.position.z = std::max(params.start.z, params.end.z) + z_offset;

    // 速度计算（导数）
    double dz_dt = 2.0 * M_PI * params.height * sin_pi * trig.fastCos(M_PI * progress) / params.duration;
    double vx = (params.end.x - params.start.x) / params.duration;
    double vy = (params.end.y - params.start.y) / params.duration;

    point.velocity = FootVelocity(vx, vy, dz_dt);

    return point;
}

// ========== 三次样条轨迹生成 ==========

TrajectoryPoint FootTrajectoryGenerator::generateCubicSplineSwingPoint(
    const SwingTrajectoryParams& params,
    double progress) const {

    TrajectoryPoint point;
    point.progress = progress;
    point.time = progress * params.duration;

    // 定义关键点：起点、顶点、终点
    double t_apex = params.apex_ratio;

    // XYZ位置计算
    if (progress < t_apex) {
        // 上升段：起点到顶点
        double t = progress / t_apex;
        point.position.x = cubicHermiteInterpolate(
            params.start.x, params.mid.x, 0, (params.mid.x - params.start.x) / t_apex, t);
        point.position.y = cubicHermiteInterpolate(
            params.start.y, params.mid.y, 0, (params.mid.y - params.start.y) / t_apex, t);
        point.position.z = cubicHermiteInterpolate(
            params.start.z, params.mid.z, 0, 0, t);
    } else {
        // 下降段：顶点到终点
        double t = (progress - t_apex) / (1.0 - t_apex);
        point.position.x = cubicHermiteInterpolate(
            params.mid.x, params.end.x, (params.end.x - params.mid.x) / (1.0 - t_apex), 0, t);
        point.position.y = cubicHermiteInterpolate(
            params.mid.y, params.end.y, (params.end.y - params.mid.y) / (1.0 - t_apex), 0, t);
        point.position.z = cubicHermiteInterpolate(
            params.mid.z, params.end.z, 0, 0, t);
    }

    return point;
}

// ========== 五次多项式轨迹生成 ==========

TrajectoryPoint FootTrajectoryGenerator::generateQuinticSwingPoint(
    const SwingTrajectoryParams& params,
    double progress) const {

    TrajectoryPoint point;
    point.progress = progress;
    point.time = progress * params.duration;

    // 五次多项式保证起落点速度和加速度都为0
    // p(s) = p0 + (p1 - p0) * (10s³ - 15s⁴ + 6s⁵)
    double s = progress;
    double blend = 10.0 * s*s*s - 15.0 * s*s*s*s + 6.0 * s*s*s*s*s;

    point.position.x = params.start.x + (params.end.x - params.start.x) * blend;
    point.position.y = params.start.y + (params.end.y - params.start.y) * blend;
    point.position.z = quinticInterpolate(params.start.z, params.mid.z, s);

    // 速度（导数）
    double blend_dot = 30.0 * s*s - 60.0 * s*s*s + 30.0 * s*s*s*s;
    double vx = (params.end.x - params.start.x) * blend_dot / params.duration;
    double vy = (params.end.y - params.start.y) * blend_dot / params.duration;

    // Z方向使用单独的五次多项式
    double h = params.mid.z - std::max(params.start.z, params.end.z);
    double z_dot = 30.0 * h * (s*s - 2.0 * s*s*s + s*s*s*s) / params.duration;

    point.velocity = FootVelocity(vx, vy, z_dot);

    return point;
}

// ========== 摆线轨迹生成 ==========

TrajectoryPoint FootTrajectoryGenerator::generateCycloidSwingPoint(
    const SwingTrajectoryParams& params,
    double progress) const {

    TrajectoryPoint point;
    point.progress = progress;
    point.time = progress * params.duration;

    // 摆线: z = H * (progress - sin(2π*progress)/(2π))
    // 这种轨迹起落点速度为0，加速度也为0
    double z_progress = progress - std::sin(2.0 * M_PI * progress) / (2.0 * M_PI);
    double z_offset = params.height * z_progress;

    point.position.x = params.start.x + (params.end.x - params.start.x) * progress;
    point.position.y = params.start.y + (params.end.y - params.start.y) * progress;
    point.position.z = std::max(params.start.z, params.end.z) + z_offset;

    // 速度
    double dz_dt = params.height * (1.0 - std::cos(2.0 * M_PI * progress)) / params.duration;
    double vx = (params.end.x - params.start.x) / params.duration;
    double vy = (params.end.y - params.start.y) / params.duration;

    point.velocity = FootVelocity(vx, vy, dz_dt);

    return point;
}

// ========== 组合轨迹生成 ==========

TrajectoryPoint FootTrajectoryGenerator::generateComboSwingPoint(
    const SwingTrajectoryParams& params,
    double progress) const {

    TrajectoryPoint point;
    point.progress = progress;
    point.time = progress * params.duration;

    // Z方向：使用正弦轨迹（平滑抬脚）
    double z_offset = params.height * std::sin(M_PI * progress) * std::sin(M_PI * progress);

    // XY方向：使用三次样条（更灵活的路径）
    double t = progress;
    double t2 = t * t;
    double t3 = t2 * t;

    // 三次Hermite样条，起落点速度为0
    double h01 = 2.0*t3 - 3.0*t2 + 1.0;
    double h11 = t3 - 2.0*t2 + t;
    double h02 = -2.0*t3 + 3.0*t2;
    double h12 = t3 - t2;

    // 设定中间点速度
    double mid_vx = (params.end.x - params.start.x);
    double mid_vy = (params.end.y - params.start.y);

    point.position.x = h01 * params.start.x + h02 * params.end.x;
    point.position.y = h01 * params.start.y + h02 * params.end.y;
    point.position.z = std::max(params.start.z, params.end.z) + z_offset;

    // 速度
    double dh01 = 6.0*t2 - 6.0*t;
    double dh02 = -6.0*t2 + 6.0*t;
    double dz_dt = 2.0 * M_PI * params.height * std::sin(M_PI * progress) * std::cos(M_PI * progress) / params.duration;
    double vx = (dh01 * params.start.x + dh02 * params.end.x) / params.duration;
    double vy = (dh01 * params.start.y + dh02 * params.end.y) / params.duration;

    point.velocity = FootVelocity(vx, vy, dz_dt);

    return point;
}

// ========== 辅助函数 ==========

double FootTrajectoryGenerator::cubicHermiteInterpolate(
    double p0, double p1, double v0, double v1, double t) const {

    double t2 = t * t;
    double t3 = t2 * t;

    double h00 = 2.0*t3 - 3.0*t2 + 1.0;
    double h10 = t3 - 2.0*t2 + t;
    double h01 = -2.0*t3 + 3.0*t2;
    double h11 = t3 - t2;

    return h00 * p0 + h10 * v0 + h01 * p1 + h11 * v1;
}

double FootTrajectoryGenerator::quinticInterpolate(double p0, double p1, double t) const {
    double t2 = t * t;
    double t3 = t2 * t;
    double t4 = t3 * t;
    double t5 = t4 * t;

    return p0 + (p1 - p0) * (10.0 * t3 - 15.0 * t4 + 6.0 * t5);
}

// ========== Ruckig轨迹生成 ==========

void FootTrajectoryGenerator::setRuckigConfig(const RuckigTrajectoryConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    ruckig_config_ = std::make_unique<RuckigTrajectoryConfig>(config);
    if (ruckig_adapter_) {
        ruckig_adapter_->setConfig(config);
    }
}

const RuckigTrajectoryConfig& FootTrajectoryGenerator::getRuckigConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // 如果未初始化，返回默认配置
    if (!ruckig_config_) {
        static const RuckigTrajectoryConfig default_config;
        return default_config;
    }
    return *ruckig_config_;
}

std::shared_ptr<RuckigTrajectoryAdapter> FootTrajectoryGenerator::getRuckigAdapter() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!ruckig_initialized_) {
        // 初始化Ruckig适配器
        if (!ruckig_config_) {
            ruckig_config_ = std::make_unique<RuckigTrajectoryConfig>();
        }
        ruckig_adapter_ = std::make_shared<RuckigTrajectoryAdapter>(*ruckig_config_);
        ruckig_initialized_ = true;
    }

    return ruckig_adapter_;
}

TrajectoryPoint FootTrajectoryGenerator::generateRuckigSwingPoint(
    const SwingTrajectoryParams& params,
    double progress) const {

    // 延迟初始化Ruckig适配器
    if (!ruckig_initialized_) {
        if (!ruckig_config_) {
            ruckig_config_ = std::make_unique<RuckigTrajectoryConfig>();
        }
        ruckig_adapter_ = std::make_shared<RuckigTrajectoryAdapter>(*ruckig_config_);
        ruckig_initialized_ = true;

        // 计算并缓存轨迹
        FootVelocity start_vel(0, 0, 0);
        FootVelocity target_vel(0, 0, 0);

        ruckig_adapter_->calculateMinimumTimeTrajectory(
            params.start,
            start_vel,
            params.end,
            target_vel
        );
    }

    // 使用Ruckig计算指定进度的轨迹点
    double time = progress * params.duration;
    return ruckig_adapter_->getPointAtTime(time);
}

// ========== MultiLegTrajectoryGenerator ==========

MultiLegTrajectoryGenerator::MultiLegTrajectoryGenerator(
    const FootTrajectoryGeneratorConfig& config)
{
    // 创建4条腿的轨迹生成器
    for (int i = 0; i < 4; ++i) {
        generators_.push_back(
            std::make_shared<FootTrajectoryGenerator>(config));
    }
}

std::vector<std::vector<TrajectoryPoint>> MultiLegTrajectoryGenerator::generateAllSwingTrajectories(
    const std::vector<SwingTrajectoryParams>& params) const {

    std::vector<std::vector<TrajectoryPoint>> trajectories;

    size_t num_legs = std::min(params.size(), generators_.size());
    for (size_t i = 0; i < num_legs; ++i) {
        trajectories.push_back(generators_[i]->generateSwingTrajectory(params[i]));
    }

    return trajectories;
}

std::vector<std::vector<TrajectoryPoint>> MultiLegTrajectoryGenerator::generateAllStanceTrajectories(
    const std::vector<StanceTrajectoryParams>& params) const {

    std::vector<std::vector<TrajectoryPoint>> trajectories;

    size_t num_legs = std::min(params.size(), generators_.size());
    for (size_t i = 0; i < num_legs; ++i) {
        trajectories.push_back(generators_[i]->generateStanceTrajectory(params[i]));
    }

    return trajectories;
}

std::shared_ptr<FootTrajectoryGenerator> MultiLegTrajectoryGenerator::getLegGenerator(LegID leg_id) {
    int index = static_cast<int>(leg_id) % 4;
    if (index >= 0 && index < static_cast<int>(generators_.size())) {
        return generators_[index];
    }
    return nullptr;
}

const std::vector<std::shared_ptr<FootTrajectoryGenerator>>&
MultiLegTrajectoryGenerator::getGenerators() const {
    return generators_;
}

// ========== TrajectoryVisualizer ==========

std::string TrajectoryVisualizer::trajectoryToString(
    const std::vector<TrajectoryPoint>& trajectory,
    const std::string& prefix) {

    std::ostringstream oss;
    oss << prefix << "Trajectory with " << trajectory.size() << " points:\n";

    for (size_t i = 0; i < trajectory.size(); i += std::max(size_t(1), trajectory.size() / 10)) {
        const auto& p = trajectory[i];
        oss << prefix << "  [" << i << "] t=" << std::fixed << std::setprecision(3) << p.time
            << " prog=" << p.progress
            << " pos=(" << p.position.x << ", " << p.position.y << ", " << p.position.z << ")";
        if (i + 1 < trajectory.size()) {
            oss << "\n";
        }
    }

    return oss.str();
}

std::string TrajectoryVisualizer::trajectoryToCSV(
    const std::vector<TrajectoryPoint>& trajectory) {

    std::ostringstream oss;
    oss << "time,progress,x,y,z,vx,vy,vz\n";

    for (const auto& p : trajectory) {
        oss << p.time << ","
            << p.progress << ","
            << p.position.x << ","
            << p.position.y << ","
            << p.position.z << ","
            << p.velocity.vx << ","
            << p.velocity.vy << ","
            << p.velocity.vz << "\n";
    }

    return oss.str();
}

void TrajectoryVisualizer::printTrajectoryInfo(
    const std::vector<TrajectoryPoint>& trajectory) {

    if (trajectory.empty()) {
        std::cout << "Empty trajectory" << std::endl;
        return;
    }

    double length = FootTrajectoryGenerator::computeTrajectoryLength(trajectory);
    double duration = FootTrajectoryGenerator::computeTrajectoryDuration(trajectory);

    std::cout << "Trajectory Info:" << std::endl;
    std::cout << "  Points: " << trajectory.size() << std::endl;
    std::cout << "  Length: " << length << " m" << std::endl;
    std::cout << "  Duration: " << duration << " s" << std::endl;
    std::cout << "  Start: (" << trajectory.front().position.x << ", "
              << trajectory.front().position.y << ", "
              << trajectory.front().position.z << ")" << std::endl;
    std::cout << "  End: (" << trajectory.back().position.x << ", "
              << trajectory.back().position.y << ", "
              << trajectory.back().position.z << ")" << std::endl;
}

} // namespace aurora::gait
