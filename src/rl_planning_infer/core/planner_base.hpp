// dcp/planner/planner_base.hpp
#ifndef PLANNER_BASE_HPP
#define PLANNER_BASE_HPP

#include <vector>
#include <string>
#include <map>
#include <atomic>
#include <cstdint>
#include <chrono>
#include "../maps/costmap.h"

// Forward declarations for trajectory types
namespace aurora::planner {
    struct AutoTrajectory;
    using Trajectory = AutoTrajectory;
}

namespace aurora::planner {

    struct PlannerInput {
        Point start;
        Point goal;
        CostMap* costmap;

        PlannerInput(const Point& s = Point(), const Point& g = Point(), CostMap* cm = nullptr)
            : start(s), goal(g), costmap(cm) {}
    };

    /**
     * @brief 规划器性能统计快照
     */
    struct PlannerStatsSnapshot {
        uint64_t total_plans = 0;
        double avg_planning_time_ms = 0.0;
        uint64_t total_inferences = 0;
        double avg_inference_time_ms = 0.0;
        double min_planning_time_ms = std::numeric_limits<double>::max();
        double max_planning_time_ms = 0.0;
        uint64_t error_count = 0;

        PlannerStatsSnapshot() = default;
    };

    /**
     * @brief 规划器性能统计（线程安全）
     */
    struct PlannerStats {
        std::atomic<uint64_t> total_plans{0};
        std::atomic<double> avg_planning_time_ms{0.0};
        std::atomic<uint64_t> total_inferences{0};
        std::atomic<double> avg_inference_time_ms{0.0};
        std::atomic<double> min_planning_time_ms{std::numeric_limits<double>::max()};
        std::atomic<double> max_planning_time_ms{0.0};
        std::atomic<uint64_t> error_count{0};

        PlannerStats() = default;

        // 禁止拷贝
        PlannerStats(const PlannerStats&) = delete;
        PlannerStats& operator=(const PlannerStats&) = delete;

        void recordPlanning(double latency_ms) {
            total_plans++;
            double current_avg = avg_planning_time_ms.load();
            avg_planning_time_ms = current_avg + (latency_ms - current_avg) / total_plans.load();

            // 更新最小延迟
            double current_min = min_planning_time_ms.load();
            while (latency_ms < current_min) {
                if (min_planning_time_ms.compare_exchange_weak(current_min, latency_ms)) {
                    break;
                }
            }

            // 更新最大延迟
            double current_max = max_planning_time_ms.load();
            while (latency_ms > current_max) {
                if (max_planning_time_ms.compare_exchange_weak(current_max, latency_ms)) {
                    break;
                }
            }
        }

        void recordInference(double latency_ms) {
            total_inferences++;
            double current_avg = avg_inference_time_ms.load();
            avg_inference_time_ms = current_avg + (latency_ms - current_avg) / total_inferences.load();
        }

        void recordError() {
            error_count++;
        }

        void reset() {
            total_plans = 0;
            avg_planning_time_ms = 0.0;
            total_inferences = 0;
            avg_inference_time_ms = 0.0;
            min_planning_time_ms = std::numeric_limits<double>::max();
            max_planning_time_ms = 0.0;
            error_count = 0;
        }

        /**
         * @brief 获取统计快照（线程安全）
         */
        PlannerStatsSnapshot getSnapshot() const {
            PlannerStatsSnapshot snapshot;
            snapshot.total_plans = total_plans.load();
            snapshot.avg_planning_time_ms = avg_planning_time_ms.load();
            snapshot.total_inferences = total_inferences.load();
            snapshot.avg_inference_time_ms = avg_inference_time_ms.load();
            snapshot.min_planning_time_ms = min_planning_time_ms.load();
            snapshot.max_planning_time_ms = max_planning_time_ms.load();
            snapshot.error_count = error_count.load();
            return snapshot;
        }
    };

    /**
     * @brief 规划器模式枚举
     */
    enum class PlannerMode : int {
        AUTO = 0,        // 自动驾驶模式
        HUMANOID = 1     // 人形机器人模式 (43-dim state, 3-dim velocity action)
    };

    /**
     * @brief 规划器基类 - 增强版
     *
     * 提供统一的接口和生命周期管理
     */
    class PlannerBase {
    public:
        virtual ~PlannerBase() = default;

        // ===== 核心规划接口 =====

        /**
         * @brief 重置规划器状态
         */
        virtual void reset() = 0;

        /**
         * @brief 规划轨迹
         * @param input 规划输入
         * @return 规划的轨迹
         */
        virtual Trajectory plan(const PlannerInput& input) = 0;

        // ===== 生命周期管理 =====

        /**
         * @brief 初始化规划器
         * @return true if initialization successful, false otherwise
         */
        virtual bool initialize() { return true; }

        /**
         * @brief 加载配置文件
         * @param config_file 配置文件路径
         * @return true if loading successful, false otherwise
         */
        virtual bool loadConfiguration(const std::string& config_file) = 0;

        /**
         * @brief 重新加载配置（热重载）
         */
        virtual void reloadConfiguration() = 0;

        // ===== 状态查询 =====

        /**
         * @brief 检查规划器是否就绪
         * @return true if ready, false otherwise
         */
        virtual bool isReady() const { return initialized_; }

        /**
         * @brief 获取规划器模式
         * @return 规划器模式
         */
        virtual PlannerMode getMode() const = 0;

        /**
         * @brief 获取模式字符串
         * @return 模式字符串
         */
        virtual std::string getModeString() const {
            switch (getMode()) {
                case PlannerMode::AUTO: return "auto";
                case PlannerMode::HUMANOID: return "humanoid";
                default: return "unknown";
            }
        }

        // ===== 性能统计 =====

        /**
         * @brief 获取统计信息快照
         * @return 统计信息快照
         */
        virtual PlannerStatsSnapshot getStats() const = 0;

        /**
         * @brief 打印性能统计
         */
        virtual void printPerformanceStats() = 0;

        /**
         * @brief 重置统计信息
         */
        virtual void resetStats() {
            if (stats_) {
                stats_->reset();
            }
        }

        // ===== 配置更新 =====

        /**
         * @brief 更新参数配置
         * @param parameters 参数键值对
         */
        virtual void updateParameters(const std::map<std::string, double>& parameters) {
            (void)parameters; // 默认不做任何操作
        }

    protected:
        bool initialized_ = false;
        PlannerStats* stats_ = nullptr;

        /**
         * @brief 辅助函数：记录规划时间
         */
        class ScopedTimer {
        public:
            ScopedTimer(PlannerStats* stats, std::atomic<double>* avg_ptr = nullptr)
                : stats_(stats), avg_ptr_(avg_ptr),
                  start_(std::chrono::high_resolution_clock::now()) {}

            ~ScopedTimer() {
                auto end = std::chrono::high_resolution_clock::now();
                double latency = std::chrono::duration<double, std::milli>(end - start_).count();
                if (stats_) {
                    stats_->recordPlanning(latency);
                }
                if (avg_ptr_) {
                    // 简单的移动平均
                    double current = avg_ptr_->load();
                    avg_ptr_->store((current + latency) / 2.0);
                }
            }

        private:
            PlannerStats* stats_;
            std::atomic<double>* avg_ptr_;
            std::chrono::high_resolution_clock::time_point start_;
        };
    };

} // namespace aurora::planner

#endif // PLANNER_BASE_HPP