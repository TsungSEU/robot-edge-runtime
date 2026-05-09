// gait_state_machine.h - Unitree风格的步态状态机
// 参考：Unitree四足机器人步态控制架构
//
// 核心设计原则：
// 1. 状态机驱动：每个腿独立管理自己的步态状态
// 2. 相位管理：通过相位偏移实现不同步态模式
// 3. 事件驱动：状态转换由步态事件触发
// 4. 可扩展：支持双足和四足配置

#ifndef GAIT_STATE_MACHINE_H
#define GAIT_STATE_MACHINE_H

#include <cstdint>
#include <cmath>
#include <array>
#include <vector>
#include <functional>
#include <mutex>
#include <string>
#include <memory>

namespace aurora::gait {

/**
 * @brief 腿部标识
 */
enum class LegID : int {
    FL = 0,  // 前左 (Front Left)
    FR = 1,  // 前右 (Front Right)
    RL = 2,  // 后左 (Rear Left)
    RR = 3,  // 后右 (Rear Right)
    LEFT = 4,   // 左脚 (双足)
    RIGHT = 5,  // 右脚 (双足)
    UNKNOWN = -1
};

/**
 * @brief 步态状态定义
 *
 * 状态转换图：
 *
 *     IDLE
 *       ↓ (start)
 *     STANCE
 *       ↓ (swing_trigger)
 *     SWING_UP
 *       ↓
 *     SWING_MID
 *       ↓
 *     SWING_DOWN
 *       ↓ (contact)
 *     CONTACT
 *       ↓ (stable)
 *     STANCE
 *       ↓ (stop)
 *     IDLE
 */
enum class GaitState : uint8_t {
    IDLE = 0,           // 空闲状态，腿不承重也不摆动
    STANCE = 1,         // 支撑相，腿承重并固定在地面
    SWING_UP = 2,       // 摆动上升，脚离地向上抬
    SWING_MID = 3,      // 摆动中段，脚在最高点附近
    SWING_DOWN = 4,     // 摆动下降，脚向地面移动
    CONTACT = 5,        // 接触相，脚刚接触地面但未完全承重
    EARLY_CONTACT = 6,  // 早期接触，障碍物触碰
    SLIP = 7,           // 打滑状态
    RECOVERY = 8        // 恢复状态
};

/**
 * @brief 步态模式
 */
enum class GaitMode : uint8_t {
    STAND = 0,          // 静止站立
    TROT = 1,           // 小跑步态 (对角腿交替，50%占空比)
    WALK = 2,           // 行走步态 (序列腿，75%占空比)
    GALLOP = 3,         // 奔跑步态
    PACE = 4,           // 同侧步态
    BIPED_WALK = 5,     // 双足行走
    BIPED_STAND = 6,    // 双足站立
    BIPED_HOP = 7,      // 双足跳跃
    CUSTOM = 255        // 自定义步态
};

/**
 * @brief 步态事件
 */
enum class GaitEvent : uint8_t {
    START = 0,          // 启动步态
    STOP = 1,           // 停止步态
    SWING_TRIGGER = 2,  // 触发摆动
    CONTACT_DETECTED = 3, // 检测到接触
    STABLE_ACHIEVED = 4, // 达到稳定
    SLIP_DETECTED = 5,  // 检测到打滑
    OBSTACLE_HIT = 6,   // 障碍物碰撞
    EMERGENCY_STOP = 7, // 紧急停止
    MODE_CHANGE = 8,    // 步态模式切换
    PHASE_RESET = 9     // 相位重置
};

/**
 * @brief 足端位置
 */
struct FootPosition {
    double x;  // 前后 (米)
    double y;  // 左右 (米)
    double z;  // 上下 (米)

    FootPosition(double x = 0, double y = 0, double z = 0)
        : x(x), y(y), z(z) {}

    FootPosition operator+(const FootPosition& other) const {
        return FootPosition(x + other.x, y + other.y, z + other.z);
    }

    FootPosition operator-(const FootPosition& other) const {
        return FootPosition(x - other.x, y - other.y, z - other.z);
    }

    FootPosition operator*(double scalar) const {
        return FootPosition(x * scalar, y * scalar, z * scalar);
    }

    double norm() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    double horizontalNorm() const {
        return std::sqrt(x * x + y * y);
    }
};

/**
 * @brief 足端速度
 */
struct FootVelocity {
    double vx;  // 前后速度 (米/秒)
    double vy;  // 左右速度 (米/秒)
    double vz;  // 上下速度 (米/秒)

    FootVelocity(double vx = 0, double vy = 0, double vz = 0)
        : vx(vx), vy(vy), vz(vz) {}

    double norm() const {
        return std::sqrt(vx * vx + vy * vy + vz * vz);
    }

    double horizontalNorm() const {
        return std::sqrt(vx * vx + vy * vy);
    }

    // Subtraction operator for velocity difference calculation
    FootVelocity operator-(const FootVelocity& other) const {
        return FootVelocity(vx - other.vx, vy - other.vy, vz - other.vz);
    }
};

/**
 * @brief 单腿步态状态
 */
struct LegGaitState {
    LegID leg_id;                    // 腿部标识
    GaitState state;                  // 当前状态
    double phase;                     // 相位 [0, 2π]
    double phase_offset;              // 相位偏移 (用于不同步态模式)

    FootPosition position;            // 当前足端位置 (相对于躯干)
    FootVelocity velocity;            // 当前足端速度

    double swing_progress;            // 摆动进度 [0, 1]
    double stance_duration;           // 当前支撑相持续时间 (秒)
    double swing_duration;            // 当前摆动相持续时间 (秒)

    FootPosition stance_start;        // 支撑相起始位置
    FootPosition swing_target;        // 摆动目标位置
    FootPosition last_contact;        // 上次接触位置

    bool is_loaded;                   // 是否承重
    double load_factor;               // 承重因子 [0, 1]

    uint32_t step_count;              // 步数计数

    LegGaitState()
        : leg_id(LegID::UNKNOWN)
        , state(GaitState::IDLE)
        , phase(0)
        , phase_offset(0)
        , position()
        , velocity()
        , swing_progress(0)
        , stance_duration(0)
        , swing_duration(0)
        , stance_start()
        , swing_target()
        , last_contact()
        , is_loaded(false)
        , load_factor(0)
        , step_count(0)
    {}
};

/**
 * @brief 步态参数
 */
struct GaitParameters {
    double step_frequency;            // 步频 (Hz)
    double step_height;              // 步高 (米)
    double step_length;              // 步长 (米)
    double stance_duration;          // 支撑相时长 (秒)
    double swing_duration;           // 摆动相时长 (秒)
    double duty_factor;              // 占空比 [0, 1]

    double swing_apex_ratio;         // 摆动顶点位置比例 [0, 1]
    double stance_depth;             // 支撑相下陷深度 (米)

    GaitParameters()
        : step_frequency(1.25)
        , step_height(0.05)
        , step_length(0.25)
        , stance_duration(0.48)
        , swing_duration(0.32)
        , duty_factor(0.6)
        , swing_apex_ratio(0.5)
        , stance_depth(0.0)
    {}

    /**
     * @brief 获取步态周期
     */
    double getPeriod() const {
        return 1.0 / step_frequency;
    }

    /**
     * @brief 从占空比计算支撑/摆动时长
     */
    void updateDurationsFromDutyFactor() {
        double period = getPeriod();
        stance_duration = period * duty_factor;
        swing_duration = period * (1.0 - duty_factor);
    }

    /**
     * @brief 从支撑/摆动时长计算占空比
     */
    void updateDutyFactorFromDurations() {
        double period = stance_duration + swing_duration;
        if (period > 0) {
            duty_factor = stance_duration / period;
            step_frequency = 1.0 / period;
        }
    }
};

/**
 * @brief 步态模式配置
 *
 * 定义不同步态模式的相位偏移
 */
struct GaitModeConfig {
    GaitMode mode;
    const char* name;
    double duty_factor;              // 占空比
    std::array<double, 4> phase_offsets;  // 四条腿的相位偏移 [FL, FR, RL, RR]

    static GaitModeConfig stand() {
        return GaitModeConfig{
            GaitMode::STAND, "stand", 1.0,
            {0.0, 0.0, 0.0, 0.0}  // 所有腿同相
        };
    }

    static GaitModeConfig trot() {
        return GaitModeConfig{
            GaitMode::TROT, "trot", 0.5,
            {0.0, M_PI, M_PI, 0.0}  // FL-RR 同相, FR-RL 同相，相差π
        };
    }

    static GaitModeConfig walk() {
        return GaitModeConfig{
            GaitMode::WALK, "walk", 0.75,
            {0.0, M_PI * 0.5, M_PI * 1.5, M_PI}  // 序列: FL -> FR -> RR -> RL
        };
    }

    static GaitModeConfig gallop() {
        return GaitModeConfig{
            GaitMode::GALLOP, "gallop", 0.4,
            {0.0, M_PI * 0.8, M_PI, M_PI * 1.8}  // 前腿领先
        };
    }

    static GaitModeConfig pace() {
        return GaitModeConfig{
            GaitMode::PACE, "pace", 0.5,
            {0.0, M_PI, 0.0, M_PI}  // 左侧腿同相，右侧腿同相
        };
    }

    static GaitModeConfig bipedWalk() {
        return GaitModeConfig{
            GaitMode::BIPED_WALK, "biped_walk", 0.6,
            {0.0, M_PI, 0.0, M_PI}  // 左右交替
        };
    }

    static GaitModeConfig bipedStand() {
        return GaitModeConfig{
            GaitMode::BIPED_STAND, "biped_stand", 1.0,
            {0.0, 0.0, 0.0, 0.0}
        };
    }
};

/**
 * @brief 步态状态机配置
 */
struct GaitStateMachineConfig {
    double update_rate;              // 更新频率 (Hz)
    bool use_contact_detection;      // 是否使用接触检测
    bool use_force_control;          // 是否使用力控制
    double early_contact_threshold;  // 早期接触力阈值 (N)
    double slip_threshold;           // 打滑检测阈值 (米/秒)

    GaitStateMachineConfig()
        : update_rate(50.0)
        , use_contact_detection(true)
        , use_force_control(false)
        , early_contact_threshold(10.0)
        , slip_threshold(0.1)
    {}
};

/**
 * @brief 步态事件回调
 */
using GaitEventCallback = std::function<void(LegID leg, GaitEvent event, const LegGaitState& state)>;

/**
 * @brief 步态状态机
 *
 * 管理单条腿的步态状态转换
 */
class LegGaitStateMachine {
public:
    explicit LegGaitStateMachine(LegID leg_id);
    ~LegGaitStateMachine() = default;

    /**
     * @brief 设置步态参数
     */
    void setGaitParameters(const GaitParameters& params);

    /**
     * @brief 设置相位偏移
     */
    void setPhaseOffset(double offset);

    /**
     * @brief 获取当前状态
     */
    const LegGaitState& getState() const;

    /**
     * @brief 更新状态机
     * @param dt 时间步长 (秒)
     * @param global_phase 全局相位
     * @param foot_position 当前足端位置 (可选，用于接触检测)
     * @return 发生的事件列表
     */
    std::vector<GaitEvent> update(double dt, double global_phase,
                                   const FootPosition* foot_position = nullptr);

    /**
     * @brief 处理外部事件
     */
    void handleEvent(GaitEvent event);

    /**
     * @brief 重置状态机
     */
    void reset();

    /**
     * @brief 设置事件回调
     */
    void setEventCallback(GaitEventCallback callback);

    /**
     * @brief 判断当前是否在支撑相
     */
    bool isInStance() const;

    /**
     * @brief 判断当前是否在摆动相
     */
    bool isInSwing() const;

    /**
     * @brief 判断是否承重
     */
    bool isLoaded() const;

    /**
     * @brief 设置摆动目标
     */
    void setSwingTarget(const FootPosition& target);

    /**
     * @brief 获取步态描述
     */
    std::string getStateDescription() const;

private:
    LegGaitState state_;
    GaitParameters params_;
    GaitEventCallback event_callback_;
    mutable std::mutex mutex_;

    /**
     * @brief 状态转换逻辑
     */
    void transitionState(GaitState new_state);
    void checkStateTransition(double global_phase);

    /**
     * @brief 摆动相位更新
     */
    void updateSwingPhase(double dt);

    /**
     * @brief 支撑相位更新
     */
    void updateStancePhase(double dt);

    /**
     * @brief 触发事件回调
     */
    void triggerEvent(GaitEvent event);
};

/**
 * @brief 多腿步态协调器
 *
 * 管理多条腿的步态状态机，确保协调运动
 */
class GaitCoordinator {
public:
    explicit GaitCoordinator(const GaitStateMachineConfig& config = {});
    ~GaitCoordinator() = default;

    /**
     * @brief 设置腿数量
     */
    void setNumLegs(size_t num_legs);

    /**
     * @brief 设置步态模式
     */
    void setGaitMode(GaitMode mode);

    /**
     * @brief 获取当前步态模式
     */
    GaitMode getGaitMode() const;

    /**
     * @brief 设置步态参数
     */
    void setGaitParameters(const GaitParameters& params);

    /**
     * @brief 获取步态参数
     */
    const GaitParameters& getGaitParameters() const;

    /**
     * @brief 更新所有腿的步态状态
     * @param dt 时间步长
     * @return 发生的事件列表 (leg_id, event)
     */
    std::vector<std::pair<LegID, GaitEvent>> update(double dt);

    /**
     * @brief 获取指定腿的状态
     */
    const LegGaitState& getLegState(LegID leg_id) const;

    /**
     * @brief 获取所有腿的状态
     */
    const std::vector<LegGaitState>& getAllStates() const;

    /**
     * @brief 设置足端位置 (用于接触检测)
     */
    void setFootPosition(LegID leg_id, const FootPosition& position);

    /**
     * @brief 设置摆动目标
     */
    void setSwingTarget(LegID leg_id, const FootPosition& target);

    /**
     * @brief 启动步态
     */
    void start();

    /**
     * @brief 停止步态
     */
    void stop();

    /**
     * @brief 重置所有状态机
     */
    void reset();

    /**
     * @brief 判断是否有腿在摆动
     */
    bool hasSwingingLeg() const;

    /**
     * @brief 判断是否有腿在支撑
     */
    bool hasStanceLeg() const;

    /**
     * @brief 获取支撑腿数量
     */
    size_t getStanceLegCount() const;

    /**
     * @brief 设置事件回调
     */
    void setEventCallback(GaitEventCallback callback);

    /**
     * @brief 获取全局相位
     */
    double getGlobalPhase() const;

    /**
     * @brief 获取步态模式名称
     */
    static const char* getGaitModeName(GaitMode mode);

    /**
     * @brief 获取当前步态模式名称
     */
    const char* getCurrentGaitModeName() const;

private:
    std::vector<std::unique_ptr<LegGaitStateMachine>> leg_state_machines_;
    GaitParameters params_;
    GaitMode current_mode_;
    GaitStateMachineConfig config_;
    double global_phase_;
    bool running_;
    GaitEventCallback event_callback_;
    mutable std::mutex mutex_;

    /**
     * @brief 应用步态模式的相位偏移
     */
    void applyGaitModePhaseOffsets();

    /**
     * @brief 获取步态模式配置
     */
    GaitModeConfig getGaitModeConfig(GaitMode mode) const;
};

/**
 * @brief 工具函数
 */
class GaitUtils {
public:
    /**
     * @brief 将相位归一化到 [0, 2π]
     */
    static double normalizePhase(double phase) {
        while (phase < 0) phase += 2.0 * M_PI;
        while (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
        return phase;
    }

    /**
     * @brief 计算两个相位的差值
     */
    static double phaseDifference(double phase1, double phase2) {
        double diff = phase1 - phase2;
        while (diff < -M_PI) diff += 2.0 * M_PI;
        while (diff > M_PI) diff -= 2.0 * M_PI;
        return diff;
    }

    /**
     * @brief 判断相位是否在支撑相
     */
    static bool isInStancePhase(double phase, double duty_factor) {
        double swing_start = 2.0 * M_PI * duty_factor;
        double normalized = normalizePhase(phase);
        return normalized < swing_start;
    }

    /**
     * @brief 判断相位是否在摆动相
     */
    static bool isInSwingPhase(double phase, double duty_factor) {
        return !isInStancePhase(phase, duty_factor);
    }

    /**
     * @brief 计算摆动进度 [0, 1]
     */
    static double calculateSwingProgress(double phase, double duty_factor) {
        double swing_start = 2.0 * M_PI * duty_factor;
        double normalized = normalizePhase(phase);
        if (normalized < swing_start) {
            return 0.0;
        }
        return (normalized - swing_start) / (2.0 * M_PI * (1.0 - duty_factor));
    }

    /**
     * @brief LegID转字符串
     */
    static const char* legIDToString(LegID leg_id);

    /**
     * @brief 字符串转LegID
     */
    static LegID stringToLegID(const std::string& str);

    /**
     * @brief GaitState转字符串
     */
    static const char* gaitStateToString(GaitState state);

    /**
     * @brief GaitEvent转字符串
     */
    static const char* gaitEventToString(GaitEvent event);
};

} // namespace aurora::gait

#endif // GAIT_STATE_MACHINE_H
