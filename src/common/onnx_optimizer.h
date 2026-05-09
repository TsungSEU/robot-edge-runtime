// onnx_optimizer.h - 优化的 ONNX 模型推理包装器
// 针对边缘设备部署优化，支持异步推理和批量推理

#ifndef ONNX_OPTIMIZER_H
#define ONNX_OPTIMIZER_H

#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <future>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <functional>

#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

#include "common/memory_pool.h"

namespace aurora::onnx {

// ==================== 配置选项 ====================

struct OnnxOptimizerConfig {
    // 执行选项
    int num_threads = 1;              // 推理线程数 (0=自动)
    bool enable_parallel_execution = true;  // 并行执行
    bool enable_mem_pattern = true;    // 内存模式优化
    bool enable_cpu_mem_arena = true;  // CPU 内存竞技场

    // 图优化
    int graph_optimization_level = 99; // 图优化级别 (0=禁用, 99=全部)
    bool enable_symbolic_dim_values = true;  // 符号维度值

    // 模型选项
    bool use_mmap = true;              // 使用内存映射加载模型
    bool enable_lazy_init = false;    // 延迟初始化

    // 推理选项
    bool enable_simd = true;           // 启用 SIMD 优化 (来自 performance_utils.h)
    bool enable_fast_math = true;      // 启用快速数学

    // 内存池
    size_t memory_pool_size = 5;       // 推理缓冲池大小

    // 异步推理
    bool enable_async = false;         // 启用异步推理
    size_t async_queue_size = 10;      // 异步队列大小
};

// ==================== 推理结果 ====================

struct InferenceResult {
    std::vector<float> outputs;
    double latency_ms;
    bool success;
    std::string error_message;

    InferenceResult() : latency_ms(0), success(false) {}
};

// ==================== ONNX 会话管理器 ====================

#ifdef HAVE_ONNXRUNTIME

/**
 * @brief ONNX Runtime 会话包装器
 * 提供优化的模型加载和推理功能
 */
class OnnxSession {
private:
    OnnxOptimizerConfig config_;

    // ONNX Runtime 组件
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::MemoryInfo> memory_info_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    std::unique_ptr<Ort::RunOptions> run_options_;

    // 模型信息
    std::string model_path_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<std::vector<int64_t>> input_shapes_;
    std::vector<std::vector<int64_t>> output_shapes_;

    // 内存池
    std::unique_ptr<aurora::memory::InferenceBufferPool> buffer_pool_;

    // 性能统计
    std::atomic<uint64_t> inference_count_{0};
    std::atomic<double> total_latency_ms_{0.0};
    std::atomic<double> min_latency_ms_{std::numeric_limits<double>::max()};
    std::atomic<double> max_latency_ms_{0.0};

    // 线程安全
    mutable std::mutex session_mutex_;

    // 初始化标志
    std::atomic<bool> initialized_{false};

    bool createSessionOptions();
    bool loadModel();
    void getInputOutputInfo();

public:
    explicit OnnxSession(const OnnxOptimizerConfig& config = OnnxOptimizerConfig());
    ~OnnxSession();

    // 禁止拷贝
    OnnxSession(const OnnxSession&) = delete;
    OnnxSession& operator=(const OnnxSession&) = delete;

    // 初始化
    bool initialize(const std::string& model_path);
    bool isInitialized() const { return initialized_.load(); }

    // 同步推理
    InferenceResult run(const std::vector<std::vector<float>>& inputs);
    InferenceResult run(const std::vector<float>& input);

    // 批量推理 (优化版)
    std::vector<InferenceResult> runBatch(const std::vector<std::vector<float>>& batch);

    // 模型信息
    const std::vector<std::string>& getInputNames() const { return input_names_; }
    const std::vector<std::string>& getOutputNames() const { return output_names_; }
    const std::vector<std::vector<int64_t>>& getInputShapes() const { return input_shapes_; }
    const std::vector<std::vector<int64_t>>& getOutputShapes() const { return output_shapes_; }

    // 性能统计
    uint64_t getInferenceCount() const { return inference_count_.load(); }
    double getAverageLatency() const {
        uint64_t count = inference_count_.load();
        return count > 0 ? total_latency_ms_.load() / count : 0.0;
    }
    double getMinLatency() const { return min_latency_ms_.load(); }
    double getMaxLatency() const { return max_latency_ms_.load(); }

    void resetStats() {
        inference_count_ = 0;
        total_latency_ms_ = 0.0;
        min_latency_ms_ = std::numeric_limits<double>::max();
        max_latency_ms_ = 0.0;
    }
};

// ==================== 异步推理引擎 ====================

/**
 * @brief 异步推理引擎
 * 支持并发提交推理请求
 */
class AsyncInferenceEngine {
private:
    std::shared_ptr<OnnxSession> session_;
    OnnxOptimizerConfig config_;

    // 推理队列
    struct InferenceTask {
        std::vector<float> input;
        std::promise<InferenceResult> promise;
    };

    std::queue<InferenceTask> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // 工作线程
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};

    // 批处理
    std::vector<std::vector<float>> batch_buffer_;
    size_t batch_size_;

    void workerLoop();
    std::vector<InferenceResult> processBatch();

public:
    AsyncInferenceEngine(std::shared_ptr<OnnxSession> session,
                        const OnnxOptimizerConfig& config = OnnxOptimizerConfig());
    ~AsyncInferenceEngine();

    // 禁止拷贝
    AsyncInferenceEngine(const AsyncInferenceEngine&) = delete;
    AsyncInferenceEngine& operator=(const AsyncInferenceEngine&) = delete;

    // 启动/停止
    bool start();
    void stop();

    // 异步推理
    std::future<InferenceResult> inferAsync(const std::vector<float>& input);

    // 同步推理 (带超时)
    InferenceResult infer(const std::vector<float>& input,
                         int timeout_ms = 1000);
};

#endif // HAVE_ONNXRUNTIME

// ==================== 模型缓存管理器 ====================

/**
 * @brief ONNX 模型缓存管理器
 * 支持模型热加载和版本管理
 */
class OnnxModelCache {
private:
    struct ModelEntry {
        std::string path;
        std::string version;
        uint64_t file_size;
        std::chrono::system_clock::time_point load_time;
        std::shared_ptr<void> session;  // OnnxSession*
    };

    std::unordered_map<std::string, ModelEntry> cache_;
    mutable std::shared_mutex cache_mutex_;  // 读写锁
    size_t max_cache_size_;

    // 单例
    OnnxModelCache() : max_cache_size_(3) {}
    ~OnnxModelCache() = default;

public:
    static OnnxModelCache& getInstance() {
        static OnnxModelCache instance;
        return instance;
    }

    // 禁止拷贝
    OnnxModelCache(const OnnxModelCache&) = delete;
    OnnxModelCache& operator=(const OnnxModelCache&) = delete;

    // 获取或加载模型
    std::shared_ptr<OnnxSession> getModel(const std::string& model_path,
                                          const OnnxOptimizerConfig& config);

    // 预加载模型
    bool preloadModel(const std::string& model_path,
                     const OnnxOptimizerConfig& config);

    // 卸载模型
    void unloadModel(const std::string& model_path);

    // 清空缓存
    void clear();

    // 获取缓存信息
    size_t size() const { return cache_.size(); }
    std::vector<std::string> getCachedModels() const;
};

// ==================== 推理统计收集器 ====================

/**
 * @brief 推理性能统计收集器
 * 提供详细的性能指标收集和报告
 */
class InferenceMetricsCollector {
private:
    struct Metrics {
        uint64_t total_inferences = 0;
        uint64_t failed_inferences = 0;
        double total_latency_ms = 0.0;
        double min_latency_ms = std::numeric_limits<double>::max();
        double max_latency_ms = 0.0;
        double p50_latency_ms = 0.0;
        double p95_latency_ms = 0.0;
        double p99_latency_ms = 0.0;

        std::vector<double> latency_samples;  // 保留最近的样本用于百分位计算
        static constexpr size_t MAX_SAMPLES = 10000;
    };

    Metrics metrics_;
    std::mutex metrics_mutex_;
    std::atomic<bool> enabled_{true};

    // 滑动窗口统计
    struct WindowMetrics {
        std::deque<double> recent_latencies;
        static constexpr size_t WINDOW_SIZE = 1000;
    };
    WindowMetrics window_metrics_;
    std::mutex window_mutex_;

public:
    void recordInference(double latency_ms, bool success);
    void recordSuccess(double latency_ms) { recordInference(latency_ms, true); }
    void recordFailure(double latency_ms) { recordInference(latency_ms, false); }

    void reset();
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }

    // 获取统计信息
    uint64_t getTotalInferences() const;
    uint64_t getFailedInferences() const;
    double getAverageLatency() const;
    double getMinLatency() const;
    double getMaxLatency() const;
    double getP50Latency() const;
    double getP95Latency() const;
    double getP99Latency() const;
    double getThroughput() const;  // 推理/秒

    double getFailureRate() const;

    // 导出报告
    std::string generateReport() const;
    std::map<std::string, double> getMetricsMap() const;
};

// ==================== 批处理推理优化器 ====================

#ifdef HAVE_ONNXRUNTIME

/**
 * @brief 批处理推理优化器
 * 将多个推理请求合并为一个批次处理
 */
class BatchInferenceOptimizer {
private:
    std::shared_ptr<OnnxSession> session_;
    size_t max_batch_size_;
    double max_wait_time_ms_;  // 最大等待时间

    struct BatchEntry {
        std::vector<float> input;
        std::function<void(const InferenceResult&)> callback;
        std::chrono::steady_clock::time_point submit_time;
    };

    std::vector<BatchEntry> batch_buffer_;
    std::mutex batch_mutex_;

    std::thread processing_thread_;
    std::atomic<bool> running_{false};

    void processBatch();

public:
    BatchInferenceOptimizer(std::shared_ptr<OnnxSession> session,
                           size_t max_batch_size = 8,
                           double max_wait_time_ms = 5.0);
    ~BatchInferenceOptimizer();

    void start();
    void stop();

    // 添加推理请求到批次
    void submit(const std::vector<float>& input,
              std::function<void(const InferenceResult&)> callback);
};

#endif // HAVE_ONNXRUNTIME

} // namespace aurora::onnx

#endif // ONNX_OPTIMIZER_H
