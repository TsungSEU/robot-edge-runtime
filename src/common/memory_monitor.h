// memory_monitor.h
#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unistd.h>

#include "common/log/logger.h"

namespace aurora::monitor {

/**
 * @brief 内存监控器
 *
 * 跟踪各组件的内存使用情况，强制执行内存限制
 * 防止内存泄漏和过度使用
 */
class MemoryMonitor {
private:
    size_t max_memory_mb_;        // 最大内存限制（MB）
    std::atomic<size_t> current_usage_mb_;  // 当前使用量（MB）
    std::map<std::string, size_t> component_usage_;  // 各组件使用量
    mutable std::mutex usage_mutex_;

    // 统计信息
    std::atomic<size_t> total_allocations_;
    std::atomic<size_t> total_deallocations_;
    std::atomic<size_t> allocation_failures_;

public:
    /**
     * @brief 构造函数
     *
     * @param max_memory_mb 最大内存限制（MB），默认为1024MB
     */
    explicit MemoryMonitor(size_t max_memory_mb = 1024)
        : max_memory_mb_(max_memory_mb),
          current_usage_mb_(0),
          total_allocations_(0),
          total_deallocations_(0),
          allocation_failures_(0) {}

    /**
     * @brief 尝试分配内存
     *
     * @param component 组件名称
     * @param size_mb 要分配的内存大小（MB）
     * @return true if allocation succeeded, false otherwise
     */
    bool allocate(const std::string& component, size_t size_mb) {
        std::lock_guard<std::mutex> lock(usage_mutex_);

        // 检查是否超过限制
        if (current_usage_mb_.load() + size_mb > max_memory_mb_) {
            AD_WARN(MONITOR, "Memory allocation rejected: %s requesting %lu MB (current: %lu MB, max: %lu MB)",
                   component.c_str(), size_mb, current_usage_mb_.load(), max_memory_mb_);
            allocation_failures_++;
            return false;
        }

        // 更新使用量
        current_usage_mb_ += size_mb;
        component_usage_[component] += size_mb;
        total_allocations_++;
        AD_DEBUG(MONITOR, "Memory allocated: %s +%lu MB, total: %lu MB",
                component.c_str(), size_mb, current_usage_mb_.load());

        return true;
    }

    /**
     * @brief 释放内存
     *
     * @param component 组件名称
     * @param size_mb 要释放的内存大小（MB）
     */
    void deallocate(const std::string& component, size_t size_mb) {
        std::lock_guard<std::mutex> lock(usage_mutex_);

        // 更新使用量（不小于0）
        size_t actual_dealloc = std::min(size_mb, component_usage_[component]);
        current_usage_mb_ -= actual_dealloc;
        component_usage_[component] -= actual_dealloc;
        total_deallocations_++;

        AD_DEBUG(MONITOR, "Memory deallocated: %s -%lu MB, total: %lu MB",
                component.c_str(), actual_dealloc, current_usage_mb_.load());
    }

    /**
     * @brief 设置最大内存限制
     */
    void setMaxMemory(size_t max_memory_mb) {
        max_memory_mb_ = max_memory_mb;
        AD_INFO(MONITOR, "Max memory limit set to %lu MB", max_memory_mb);
    }

    /**
     * @brief 获取当前内存使用量（MB）
     */
    size_t getCurrentUsageMB() const {
        return current_usage_mb_.load();
    }

    /**
     * @brief 获取可用内存（MB）
     */
    size_t getAvailableMB() const {
        return std::max(max_memory_mb_ - current_usage_mb_.load(), static_cast<size_t>(0));
    }

    /**
     * @brief 获取内存使用率（0-1）
     */
    double getUsageRatio() const {
        return static_cast<double>(getCurrentUsageMB()) / max_memory_mb_;
    }

    /**
     * @brief 获取最大内存限制（MB）
     */
    size_t getMaxMemoryMB() const {
        return max_memory_mb_;
    }

    /**
     * @brief 获取指定组件的内存使用量（MB）
     */
    size_t getComponentUsage(const std::string& component) const {
        std::lock_guard<std::mutex> lock(usage_mutex_);
        auto it = component_usage_.find(component);
        return (it != component_usage_.end()) ? it->second : 0;
    }

    /**
     * @brief 获取所有组件的内存使用情况
     */
    std::map<std::string, size_t> getAllComponentUsage() const {
        std::lock_guard<std::mutex> lock(usage_mutex_);
        return component_usage_;
    }

    /**
     * @brief 打印内存使用报告
     */
    void printUsageReport() const {
        std::lock_guard<std::mutex> lock(usage_mutex_);

        AD_INFO(MONITOR, "=== Memory Usage Report ===");
        AD_INFO(MONITOR, "Total: %lu / %lu MB (%.1f%%)",
               getCurrentUsageMB(), getMaxMemoryMB(), getUsageRatio() * 100.0);

        // 按使用量排序
        std::vector<std::pair<std::string, size_t>> sorted_usage;
        for (const auto& [component, usage] : component_usage_) {
            sorted_usage.emplace_back(component, usage);
        }
        std::sort(sorted_usage.begin(), sorted_usage.end(),
                 [](const auto& a, const auto& b) {
                     return a.second > b.second;
                 });

        // 打印各组件使用量
        for (const auto& [component, usage] : sorted_usage) {
            if (usage > 0) {
                double percentage = (usage * 100.0) / getMaxMemoryMB();
                AD_INFO(MONITOR, "  %-20s: %lu MB (%.1f%%)",
                       component.c_str(), usage, percentage);
            }
        }

        AD_INFO(MONITOR, "Statistics:");
        AD_INFO(MONITOR, "  Total allocations: %lu", total_allocations_.load());
        AD_INFO(MONITOR, "  Total deallocations: %lu", total_deallocations_.load());
        AD_INFO(MONITOR, "  Allocation failures: %lu", allocation_failures_.load());
        AD_INFO(MONITOR, "========================");
    }

    /**
     * @brief 重置统计数据
     */
    void reset() {
        total_allocations_.store(0);
        total_deallocations_.store(0);
        allocation_failures_.store(0);
    }

    /**
     * @brief 获取系统总内存（MB）
     */
    static size_t getSystemTotalMemoryMB() {
        std::ifstream meminfo("/proc/meminfo");
        std::string key;
        size_t value;

        while (meminfo >> key >> value) {
            if (key == "MemTotal:") {
                return value / 1024;  // KB -> MB
            }
        }

        return 0;
    }

    /**
     * @brief 获取当前进程内存使用量（MB）
     */
    static size_t getProcessMemoryMB() {
        std::ifstream statm("/proc/self/statm");
        size_t pages;
        statm >> pages;  // 第一个字段是总页数
        return (pages * getpagesize()) / (1024 * 1024);  // 页 -> MB
    }

    /**
     * @brief 检查内存使用是否过高
     */
    bool isHighUsage() const {
        return getUsageRatio() > 0.8;
    }

    /**
     * @brief 检查内存使用是否危险
     */
    bool isCriticalUsage() const {
        return getUsageRatio() > 0.9;
    }

    /**
     * @brief 获取分配成功率
     */
    double getAllocationSuccessRate() const {
        size_t total = total_allocations_.load() + allocation_failures_.load();
        if (total == 0) return 1.0;
        return static_cast<double>(total_allocations_.load()) / total;
    }
};

} // namespace aurora::monitor

#endif // MEMORY_MONITOR_H
