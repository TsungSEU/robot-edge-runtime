#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <chrono>
#include <atomic>

namespace aurora::state_machine {

// 系统状态枚举
enum class SystemState {
    INITIALIZING,           // 初始化中
    IDLE,                   // 空闲状态
    PLANNING,               // 路径规划中
    NAVIGATING,             // 导航中
    DATA_COLLECTING,        // 数据采集中
    UPLOADING,              // 数据上传中
    ERROR,                  // 错误状态
    SHUTTING_DOWN           // 关闭中
};

// 状态转换事件枚举
enum class StateEvent {
    INIT_COMPLETE,          // 初始化完成
    PLAN_REQUEST,           // 规划请求
    PLAN_COMPLETE,          // 规划完成
    NAVIGATION_START,       // 开始导航
    WAYPOINT_REACHED,       // 到达路径点
    TRIGGERED,              // 触发条件满足
    DATA_COLLECTED,         // 数据采集完成
    UPLOAD_REQUEST,         // 上传请求
    UPLOAD_COMPLETE,        // 上传完成
    ERROR_OCCURRED,         // 发生错误
    RECOVERY_REQUEST,       // 恢复请求
    SHUTDOWN_REQUEST        // 关闭请求
};

/**
 * @brief 状态机动作回调集合
 *
 * 外部系统（如 DataCollectionPlanner）注册这些回调，
 * 状态机在状态转换时调用对应动作。
 */
struct ActionCallbacks {
    std::function<bool()> plan;                     // 执行规划，返回是否成功
    std::function<bool()> navigate_step;            // 执行单步导航，返回是否到达终点
    std::function<bool()> should_collect;           // 判断是否需要采集（对接 TriggerManager）
    std::function<bool()> collect;                  // 执行采集，返回是否成功
    std::function<bool()> upload;                   // 执行上传，返回是否成功
    std::function<void()> on_shutdown;              // 关机回调
};

/**
 * @brief 审计日志回调
 *
 * 每次状态转换时调用，记录 from/to/event/timestamp。
 */
using AuditLogCallback = std::function<void(SystemState from, SystemState to,
                                            StateEvent event,
                                            const std::chrono::steady_clock::time_point& timestamp)>;

class StateMachine {
public:
    static StateMachine& getInstance();

    // 删除拷贝构造和赋值操作符
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;

    // 初始化状态机
    bool initialize();

    // 处理状态事件
    void handleEvent(StateEvent event);

    // 获取当前状态
    SystemState getCurrentState() const { return current_state_.load(); }

    // 设置当前状态
    void setCurrentState(SystemState state) { current_state_.store(state); }

    // 设置动作回调
    void setActionCallbacks(const ActionCallbacks& callbacks);

    // 设置审计日志回调
    void setAuditLogCallback(AuditLogCallback callback);

    // 降级模式控制
    void setDegradeMode(bool degraded);
    bool isDegradeMode() const { return degrade_mode_.load(); }

    // 状态字符串转换
    static const char* stateToString(SystemState state);
    static const char* eventToString(StateEvent event);

private:
    StateMachine();
    ~StateMachine() = default;

    // 状态处理函数
    void handleInitializing(StateEvent event);
    void handleIdle(StateEvent event);
    void handlePlanning(StateEvent event);
    void handleNavigating(StateEvent event);
    void handleDataCollection(StateEvent event);
    void handleUploading(StateEvent event);
    void handleError(StateEvent event);
    void handleShuttingDown(StateEvent event);

    // 状态转换辅助函数
    void transitionToState(SystemState new_state, StateEvent event);

    // 内部组件
    ActionCallbacks actions_;
    AuditLogCallback audit_callback_;

    // 当前状态
    std::atomic<SystemState> current_state_;

    // 降级模式标志
    std::atomic<bool> degrade_mode_{false};
};

} // namespace aurora::state_machine

#endif // STATE_MACHINE_H
