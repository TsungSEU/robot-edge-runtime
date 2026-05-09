// preset_motion_library.h - 预设动作库
// 50+预定义机器人动作，支持复杂行为的快速实现
//
// 核心功能：
// 1. 预设动作定义（站立、坐下、行走、挥手等）
// 2. 动作插值和混合
// 3. 动作序列执行
// 4. 动作中断和恢复

#ifndef PRESET_MOTION_LIBRARY_H
#define PRESET_MOTION_LIBRARY_H

#include "gait_state_machine.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <memory>

namespace aurora::gait {

/**
 * @brief 动作关键帧
 */
struct MotionKeyframe {
    double time;                     // 时间（秒）
    std::vector<double> joint_positions;  // 关节位置（12个关节）
    std::vector<double> joint_velocities; // 关节速度（可选）

    MotionKeyframe(double t = 0)
        : time(t)
    {}
};

/**
 * @brief 预设动作
 */
struct PresetMotion {
    std::string name;                // 动作名称
    std::string category;            // 动作类别
    double duration;                 // 总持续时间（秒）
    std::vector<MotionKeyframe> keyframes;  // 关键帧序列

    // 动作属性
    bool interruptible;              // 是否可中断
    bool repeat;                     // 是否重复
    int repeat_count;                // 重复次数

    PresetMotion()
        : duration(0.0)
        , interruptible(true)
        , repeat(false)
        , repeat_count(1)
    {}
};

/**
 * @brief 动作执行状态
 */
enum class MotionExecutionState : uint8_t {
    IDLE,           // 空闲
    PREPARING,      // 准备中
    EXECUTING,      // 执行中
    PAUSED,         // 暂停
    COMPLETED,      // 完成
    INTERRUPTED,    // 中断
    ERROR           // 错误
};

/**
 * @brief 动作执行进度
 */
struct MotionExecutionProgress {
    MotionExecutionState state;
    double current_time;             // 当前时间（秒）
    double progress;                 // 进度 [0, 1]
    std::vector<double> current_joint_positions;  // 当前关节位置
    std::vector<double> current_joint_velocities; // 当前关节速度

    MotionExecutionProgress()
        : state(MotionExecutionState::IDLE)
        , current_time(0.0)
        , progress(0.0)
    {}
};

/**
 * @brief 动作执行配置
 */
struct MotionExecutionConfig {
    // 插值方法
    enum InterpolationMethod {
        LINEAR,         // 线性插值
        CUBIC_SPLINE,   // 三次样条插值
        QUINTIC         // 五次多项式插值
    };

    InterpolationMethod interpolation_method;
    double default_duration;
    bool allow_interruption;
    double transition_duration;      // 动作间过渡时间（秒）

    MotionExecutionConfig()
        : interpolation_method(CUBIC_SPLINE)
        , default_duration(2.0)
        , allow_interruption(true)
        , transition_duration(0.5)
    {}
};

/**
 * @brief 动作事件回调
 */
using MotionEventCallback = std::function<void(
    const std::string& motion_name,
    MotionExecutionState state,
    const MotionExecutionProgress& progress
)>;

/**
 * @brief 预设动作库
 *
 * 管理和执行预定义的机器人动作
 */
class PresetMotionLibrary {
public:
    explicit PresetMotionLibrary(
        const MotionExecutionConfig& config = {});

    ~PresetMotionLibrary() = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const MotionExecutionConfig& config);

    /**
     * @brief 获取配置
     */
    const MotionExecutionConfig& getConfig() const;

    /**
     * @brief 添加预设动作
     */
    void addMotion(const PresetMotion& motion);

    /**
     * @brief 批量添加预设动作
     */
    void addMotions(const std::vector<PresetMotion>& motions);

    /**
     * @brief 获取预设动作
     */
    const PresetMotion* getMotion(const std::string& name) const;

    /**
     * @brief 获取所有动作名称
     */
    std::vector<std::string> getMotionNames() const;

    /**
     * @brief 获取指定类别的动作
     */
    std::vector<std::string> getMotionsByCategory(const std::string& category) const;

    /**
     * @brief 执行动作
     * @param motion_name 动作名称
     * @return 成功返回true
     */
    bool executeMotion(const std::string& motion_name);

    /**
     * @brief 停止当前动作
     */
    void stopMotion();

    /**
     * @brief 暂停当前动作
     */
    void pauseMotion();

    /**
     * @brief 恢复当前动作
     */
    void resumeMotion();

    /**
     * @brief 更新动作执行
     * @param dt 时间步长（秒）
     * @return 当前关节位置
     */
    std::vector<double> update(double dt);

    /**
     * @brief 获取执行进度
     */
    MotionExecutionProgress getProgress() const;

    /**
     * @brief 获取当前执行状态
     */
    MotionExecutionState getState() const;

    /**
     * @brief 设置事件回调
     */
    void setEventCallback(MotionEventCallback callback);

    /**
     * @brief 初始化默认动作库
     */
    void initializeDefaultMotions();

    /**
     * @brief 从YAML文件加载动作
     */
    bool loadFromYAML(const std::string& filepath);

    /**
     * @brief 保存到YAML文件
     */
    bool saveToYAML(const std::string& filepath) const;

private:
    MotionExecutionConfig config_;
    std::unordered_map<std::string, PresetMotion> motions_;

    // 执行状态
    PresetMotion* current_motion_;
    MotionExecutionProgress progress_;
    int repeat_count_;

    // 回调
    MotionEventCallback event_callback_;

    mutable std::mutex mutex_;

    /**
     * @brief 插值计算关节位置
     */
    std::vector<double> interpolateJointPositions(
        const PresetMotion& motion,
        double time) const;

    // Allow MotionBlender and MotionTransitioner to access private methods
    friend class MotionBlender;
    friend class MotionTransitioner;

    /**
     * @brief 线性插值
     */
    double linearInterpolate(double y0, double y1, double t) const;

    /**
     * @brief 三次样条插值
     */
    double cubicSplineInterpolate(
        double y0, double y1, double v0, double v1, double t) const;

    /**
     * @brief 五次多项式插值
     */
    double quinticInterpolate(double y0, double y1, double t) const;

    /**
     * @brief 触发事件回调
     */
    void triggerEventCallback(
        const std::string& motion_name,
        MotionExecutionState state);

    /**
     * @brief 查找关键帧索引
     */
    std::pair<size_t, size_t> findKeyframeIndices(
        const PresetMotion& motion,
        double time) const;
};

/**
 * @brief 动作序列
 *
 * 顺序或并行执行多个动作
 */
class MotionSequence {
public:
    explicit MotionSequence() = default;
    ~MotionSequence() = default;

    /**
     * @brief 添加动作
     * @param motion_name 动作名称
     * @param delay 延迟时间（秒）
     */
    void appendMotion(const std::string& motion_name, double delay = 0.0);

    /**
     * @brief 清空序列
     */
    void clear();

    /**
     * @brief 获取序列长度
     */
    size_t size() const;

    /**
     * @brief 判断序列是否为空
     */
    bool empty() const;

private:
    struct SequenceItem {
        std::string motion_name;
        double delay;
    };

    std::vector<SequenceItem> sequence_;
};

/**
 * @brief 动作混合器
 *
 * 混合多个动作以创建复杂行为
 */
class MotionBlender {
public:
    explicit MotionBlender() = default;
    ~MotionBlender() = default;

    /**
     * @brief 混合两个动作
     * @param motion1 第一个动作
     * @param motion2 第二个动作
     * @param blend_factor 混合因子 [0, 1]
     * @return 混合后的关节位置
     */
    static std::vector<double> blendMotions(
        const PresetMotion& motion1,
        const PresetMotion& motion2,
        double blend_factor,
        double time);

    /**
     * @brief 混合多个动作
     */
    static std::vector<double> blendMultipleMotions(
        const std::vector<std::pair<PresetMotion, double>>& weighted_motions,
        double time);
};

/**
 * @brief 动作过渡器
 *
 * 平滑过渡到新动作
 */
class MotionTransitioner {
public:
    explicit MotionTransitioner(double transition_duration = 0.5);

    /**
     * @brief 开始过渡到新动作
     */
    void startTransition(
        const std::vector<double>& current_positions,
        const PresetMotion& target_motion);

    /**
     * @brief 更新过渡
     * @param dt 时间步长
     * @return 当前关节位置
     */
    std::vector<double> update(double dt);

    /**
     * @brief 判断过渡是否完成
     */
    bool isComplete() const;

    /**
     * @brief 获取过渡进度 [0, 1]
     */
    double getProgress() const;

private:
    double transition_duration_;
    double current_time_;
    std::vector<double> start_positions_;
    const PresetMotion* target_motion_;
    bool complete_;
    mutable std::mutex mutex_;

    /**
     * @brief 计算过渡位置
     */
    std::vector<double> computeTransitionPosition(double time) const;
};

} // namespace aurora::gait

#endif // PRESET_MOTION_LIBRARY_H
