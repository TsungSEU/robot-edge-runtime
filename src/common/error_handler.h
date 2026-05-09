// error_handler.h — 集中式错误处理器
// 统一处理日志 + 指标 + 决策（重试/降级/停止）
//
// 核心功能：
// 1. 指数退避重试 — 网络操作（AWS/MQTT）
// 2. 降级策略 — ONNX 推理失败降级到规则策略
// 3. 磁盘异常处理 — 磁盘满时跳过写入而非崩溃
// 4. 配置校验 — 数值范围检查，NaN/infinity 拒绝

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "log/logger.h"
#include <functional>
#include <string>
#include <chrono>
#include <cmath>
#include <limits>
#include <algorithm>
#include <thread>
#include <filesystem>

namespace aurora::common {

// ===== 错误分类 =====

enum class ErrorCategory : uint8_t {
    NETWORK,        // 网络操作（AWS/MQTT）
    INFERENCE,      // ONNX 推理
    DISK,           // 磁盘 I/O
    CONFIG,         // 配置校验
    GENERAL         // 通用
};

// ===== 错误处理策略 =====

enum class ErrorAction : uint8_t {
    RETRY,          // 重试（带退避）
    DEGRADE,        // 降级到备用方案
    SKIP,           // 跳过当前操作
    ABORT           // 中止当前操作
};

// ===== 重试策略 =====

struct RetryPolicy {
    int max_retries = 3;
    double initial_interval_sec = 1.0;
    double max_interval_sec = 60.0;
    double multiplier = 2.0;       // 指数退避倍数
    bool jitter = true;            // 添加随机抖动避免惊群
};

// ===== 降级策略 =====

struct DegradePolicy {
    bool enable_fallback = true;   // 启用降级备用方案
    int failures_before_degrade = 3; // 连续失败N次后降级
    double recovery_timeout_sec = 300.0; // 降级恢复超时（秒）
};

// ===== 集中式错误处理器 =====

class ErrorHandler {
public:
    static ErrorHandler& instance() {
        static ErrorHandler inst;
        return inst;
    }

    // 带指数退避的重试执行
    // 用法: auto result = ErrorHandler::instance().retry("upload", policy, [&]{ return doUpload(); });
    template<typename F>
    auto retry(const char* operation, const RetryPolicy& policy, F&& func)
        -> decltype(func())
    {
        using ReturnType = decltype(func());
        double interval = policy.initial_interval_sec;

        for (int attempt = 1; attempt <= policy.max_retries + 1; ++attempt) {
            try {
                return func();
            } catch (const std::exception& e) {
                if (attempt <= policy.max_retries) {
                    AD_WARN(ErrorHandler, "[%s] Attempt %d/%d failed: %s, retrying in %.1fs",
                            operation, attempt, policy.max_retries + 1, e.what(), interval);
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(static_cast<int>(interval * 1000)));
                    interval = std::min(interval * policy.multiplier, policy.max_interval_sec);
                } else {
                    AD_ERROR(ErrorHandler, "[%s] All %d attempts exhausted: %s",
                             operation, policy.max_retries + 1, e.what());
                    throw;
                }
            }
        }
        // Should not reach here, but satisfy compiler
        throw std::runtime_error(std::string(operation) + ": retry loop exited unexpectedly");
    }

    // 带降级的执行 — 失败时调用 fallback
    template<typename F, typename G>
    auto withFallback(const char* operation, const DegradePolicy& policy,
                      F&& primary, G&& fallback)
        -> decltype(primary())
    {
        using ReturnType = decltype(primary());
        consecutive_failures_++;

        try {
            auto result = primary();
            consecutive_failures_ = 0;
            return result;
        } catch (const std::exception& e) {
            AD_ERROR(ErrorHandler, "[%s] Primary failed: %s (consecutive failures: %d)",
                     operation, e.what(), consecutive_failures_);

            if (policy.enable_fallback && consecutive_failures_ >= policy.failures_before_degrade) {
                AD_WARN(ErrorHandler, "[%s] Degrading to fallback after %d failures",
                        operation, consecutive_failures_);
                try {
                    return fallback();
                } catch (const std::exception& fe) {
                    AD_ERROR(ErrorHandler, "[%s] Fallback also failed: %s", operation, fe.what());
                    throw;
                }
            }
            throw;
        }
    }

    // 磁盘安全写入 — 磁盘满时跳过而非崩溃
    template<typename F>
    bool diskSafeWrite(const char* operation, F&& write_func) {
        try {
            return write_func();
        } catch (const std::filesystem::filesystem_error& e) {
            AD_ERROR(ErrorHandler, "[%s] Disk I/O error: %s", operation, e.what());
            return false;  // 跳过，不崩溃
        } catch (const std::exception& e) {
            AD_ERROR(ErrorHandler, "[%s] Write error: %s", operation, e.what());
            return false;
        }
    }

    // ===== 配置校验工具 =====

    // 校验数值范围
    static bool validateRange(const char* name, double value,
                              double min_val, double max_val,
                              double default_val = 0.0) {
        if (std::isnan(value) || std::isinf(value)) {
            AD_WARN(ErrorHandler, "[Config] %s has invalid value (NaN/Inf), using default: %.4f",
                    name, default_val);
            return false;
        }
        if (value < min_val || value > max_val) {
            AD_WARN(ErrorHandler, "[Config] %s=%.4f out of range [%.4f, %.4f], using default: %.4f",
                    name, value, min_val, max_val, default_val);
            return false;
        }
        return true;
    }

    // 校验字符串非空
    static bool validateNotEmpty(const char* name, const std::string& value,
                                  const std::string& default_val = "") {
        if (value.empty()) {
            AD_WARN(ErrorHandler, "[Config] %s is empty, using default: %s",
                    name, default_val.c_str());
            return false;
        }
        return true;
    }

    // 重置失败计数
    void resetFailures() { consecutive_failures_ = 0; }

private:
    ErrorHandler() = default;
    int consecutive_failures_ = 0;
};

// ===== 预定义策略 =====

// AWS/MQTT 网络操作重试策略
inline RetryPolicy networkRetryPolicy() {
    return {.max_retries = 3, .initial_interval_sec = 1.0,
            .max_interval_sec = 60.0, .multiplier = 2.0, .jitter = true};
}

// ONNX 推理降级策略
inline DegradePolicy inferenceDegradePolicy() {
    return {.enable_fallback = true, .failures_before_degrade = 3,
            .recovery_timeout_sec = 300.0};
}

} // namespace aurora::common

#endif // ERROR_HANDLER_H
