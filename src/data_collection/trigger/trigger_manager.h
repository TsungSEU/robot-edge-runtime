#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <shared_mutex>
#include <chrono>
#include "channel/message_provider.h"
#include "strategy/strategy_parser.h"
#include "trigger_base.h"
#include "rule_trigger.h"
#include "state_machine/state_machine.h"
#include "ThreadPool/ThreadPool.h"

namespace aurora::collector {

// Forward declaration for SparseRegion
struct SparseRegion {
    Point center;
    double radius;

    bool contains(const Point& position) const {
        double dx = position.x - center.x;
        double dy = position.y - center.y;
        return (dx * dx + dy * dy) <= (radius * radius);
    }

    double getDistance(const Point& position) const {
        double dx = position.x - center.x;
        double dy = position.y - center.y;
        double distance = std::sqrt(dx * dx + dy * dy);
        return std::max(0.0, distance - radius);
    }
};

/**
 * @brief 步态触发配置（用于双足机器人）
 */
struct GaitTriggerConfig {
    double min_step_distance;        // 最小步长阈值 (米) - 小于此值不触发
    double min_stance_duration;      // 最小支撑持续时间 (秒)
    double cooldown_duration;        // 触发冷却时间 (秒)
    double stability_threshold;     // 稳定性阈值

    // 默认配置：基于真实双足机器人步态
    GaitTriggerConfig()
        : min_step_distance(0.15)      // 步长至少15cm
        , min_stance_duration(0.3)      // 支撑相至少300ms
        , cooldown_duration(1.0)        // 触发间隔至少1秒
        , stability_threshold(0.8) {}   // 稳定性阈值0.8
};

class TriggerManager {
public:
    TriggerManager();
    ~TriggerManager() = default;

    using TriggerCallback = std::function<void(const TriggerContext&)>;

    bool initialize(const StrategyConfig& config);
    bool start();
    bool shouldTrigger(const Point& position);
    void setTriggerCallback(TriggerCallback cb);
    void notifyTriggerContext(const TriggerContext& ctx);

    /**
     * @brief Lightweight notification: notify trigger that a position was visited
     * @param pos The visited position
     * @param timestamp Visit timestamp
     *
     * This is a simplified interface for position tracking without full TriggerContext.
     * Use this when you only need to notify about position visits (e.g., for trigger state updates).
     * For actual recording triggers, use the ROS2 /robot/trigger service.
     */
    void notifyPositionVisited(const Point& pos, uint64_t timestamp);

    /**
     * @brief 设置步态触发配置（用于双足机器人）
     */
    void setGaitTriggerConfig(const GaitTriggerConfig& config);

    /**
     * @brief 检查给定点是否位于稀疏区域
     */
    bool isInSparseArea(const Point& position);

    /**
     * @brief 计算给定点到最近稀疏区域的距离
     */
    double getDistanceToNearestSparseArea(const Point& position);

    /**
     * @brief 更新机器人实际位置（来自odometry）
     * @param position 当前机器人位置
     *
     * 对于基于步态的触发，必须使用机器人实际位置而不是规划waypoint
     */
    void updateRobotPosition(const Point& position);

    std::shared_ptr<TriggerBase> getTrigger() const;
    state_machine::SystemState getCurrentState() const { return current_state; }

private:
    bool initTriggerChecker(std::shared_ptr<TriggerBase> trigger);
    bool initTriggers(const StrategyConfig& config);
    std::shared_ptr<TriggerBase> createTrigger();
    bool shouldTriggerGaitBased(const Point& position);

    std::atomic<state_machine::SystemState> current_state{state_machine::SystemState::IDLE};

private:
    std::shared_ptr<ThreadPool> thread_pool_;
    std::unordered_map<std::string,
                       std::function<std::function<TriggerChecker::Value()>()>> variable_getter_factories_;

    std::shared_ptr<MessageProvider> message_provider_;
    std::shared_ptr<TriggerBase> trigger_instances_;
    mutable std::shared_mutex mutex_;
    TriggerCallback callback_;

    // 稀疏区域列表
    std::vector<SparseRegion> sparse_areas_;

    // 步态触发配置
    GaitTriggerConfig gait_config_;

    // 上次触发位置和时间（用于避免重复采集）
    Point last_trigger_position_{0.0, 0.0};
    std::chrono::steady_clock::time_point last_trigger_time_;

    // 机器人实际位置（来自odometry，用于步态触发）
    Point last_robot_position_{0.0, 0.0};
    bool robot_position_initialized_{false};

    // 采集历史记录（用于检查重复）
    std::vector<Point> collection_history_;
    static constexpr size_t MAX_HISTORY_SIZE = 100;
};

}