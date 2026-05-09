// onnx_inference_engine.h
#ifndef ONNX_INFERENCE_ENGINE_H
#define ONNX_INFERENCE_ENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <functional>
#include <chrono>
#include <limits>

#include "common/memory_pool.h"
#include "common/log/logger.h"

// ONNX Runtime headers
#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace aurora::planner {

/**
 * @brief ONNX推理引擎配置
 */
struct ONNXInferenceConfig {
    std::string model_path;
    int num_threads = 1;
    bool enable_memory_pool = true;
    bool enable_simd = true;
    int initial_log_std = -0.5;  // For continuous action spaces

    // 图优化级别
    int graph_optimization_level = 3;  // ORT_ENABLE_ALL

    // 执行模式
    bool sequential_execution = true;

    // Arena内存配置 (ARM优化)
    size_t arena_max_memory = 1024 * 1024 * 1024;  // 1GB

    ONNXInferenceConfig() = default;
};

/**
 * @brief 推理结果 - 通用版本
 */
struct InferenceResult {
    std::vector<float> policy_output;  // 策略输出
    float value_output = 0.0f;         // 价值输出
    double inference_time_ms = 0.0;    // 推理耗时
    bool success = true;               // 是否成功

    InferenceResult() = default;
};

/**
 * @brief 连续动作推理结果
 */
struct ContinuousInferenceResult {
    std::vector<double> action_mean;     // 动作均值
    std::vector<double> action_log_std;  // 动作log标准差
    double value = 0.0;                  // 状态价值
    double inference_time_ms = 0.0;      // 推理耗时
    bool success = true;                 // 是否成功

    ContinuousInferenceResult() = default;

    explicit ContinuousInferenceResult(int action_dim)
        : action_mean(action_dim), action_log_std(action_dim) {
        action_log_std.assign(action_dim, -0.5);  // 默认std ~ 0.6
    }
};

/**
 * @brief 推理性能统计
 */
struct ONNXInferenceStats {
    std::atomic<uint64_t> total_inferences{0};
    std::atomic<double> total_latency_ms{0.0};
    std::atomic<double> min_latency_ms{std::numeric_limits<double>::max()};
    std::atomic<double> max_latency_ms{0.0};
    std::atomic<uint64_t> error_count{0};

    ONNXInferenceStats() = default;

    void recordInference(double latency_ms) {
        total_inferences++;

        // 更新总延迟
        double current_total = total_latency_ms.load();
        while (!total_latency_ms.compare_exchange_weak(current_total, current_total + latency_ms)) {
            // CAS loop
        }

        // 更新最小延迟
        double current_min = min_latency_ms.load();
        while (latency_ms < current_min) {
            if (min_latency_ms.compare_exchange_weak(current_min, latency_ms)) {
                break;
            }
        }

        // 更新最大延迟
        double current_max = max_latency_ms.load();
        while (latency_ms > current_max) {
            if (max_latency_ms.compare_exchange_weak(current_max, latency_ms)) {
                break;
            }
        }
    }

    void recordError() {
        error_count++;
    }

    double getAverageLatency() const {
        uint64_t count = total_inferences.load();
        return count > 0 ? total_latency_ms.load() / count : 0.0;
    }

    void reset() {
        total_inferences = 0;
        total_latency_ms = 0.0;
        min_latency_ms = std::numeric_limits<double>::max();
        max_latency_ms = 0.0;
        error_count = 0;
    }
};

/**
 * @brief ONNX推理引擎 - 模板化版本
 *
 * 支持离散和连续动作空间的ONNX模型推理
 * - 离散动作: 输出动作概率分布
 * - 连续动作: 输出动作均值和log_std
 *
 * @tparam StateDim  状态空间维度
 * @tparam ActionDim 动作空间维度
 * @tparam IsContinuous 是否为连续动作空间
 */
template<int StateDim, int ActionDim, bool IsContinuous = false>
class ONNXInferenceEngine {
public:
    static constexpr int STATE_DIM = StateDim;
    static constexpr int ACTION_DIM = ActionDim;
    static constexpr bool IS_CONTINUOUS = IsContinuous;

    using ResultType = std::conditional_t<IsContinuous,
                                          ContinuousInferenceResult,
                                          InferenceResult>;

    ONNXInferenceEngine(const ONNXInferenceConfig& config);
    ~ONNXInferenceEngine();

    // 禁止拷贝
    ONNXInferenceEngine(const ONNXInferenceEngine&) = delete;
    ONNXInferenceEngine& operator=(const ONNXInferenceEngine&) = delete;

    /**
     * @brief 加载ONNX模型
     * @param model_path 模型文件路径
     * @return 是否成功
     */
    bool loadModel(const std::string& model_path);

    /**
     * @brief 运行推理
     * @param state 状态向量
     * @return 推理结果
     */
    ResultType run(const std::vector<double>& state);

    /**
     * @brief 运行推理（数组版本）
     * @param state 状态数组
     * @param state_dim 状态维度
     * @return 推理结果
     */
    ResultType run(const double* state, int state_dim);

    /**
     * @brief 获取推理统计
     */
    const ONNXInferenceStats& getStats() const { return stats_; }

    /**
     * @brief 重置统计信息
     */
    void resetStats() { stats_.reset(); }

    /**
     * @brief 检查模型是否已加载
     */
    bool isModelLoaded() const { return model_loaded_; }

    /**
     * @brief 获取状态维度
     */
    int getStateDim() const { return StateDim; }

    /**
     * @brief 获取动作维度
     */
    int getActionDim() const { return ActionDim; }

    /**
     * @brief 设置配置
     */
    void setConfig(const ONNXInferenceConfig& config) { config_ = config; }

    /**
     * @brief 获取配置
     */
    const ONNXInferenceConfig& getConfig() const { return config_; }

private:
#ifdef HAVE_ONNXRUNTIME
    /**
     * @brief 运行ONNX推理（内部实现）
     */
    std::vector<Ort::Value> runInferenceInternal(const float* input_data);

    /**
     * @brief 创建会话选项
     */
    Ort::SessionOptions createSessionOptions();

    /**
     * @brief 获取模型输出名称
     */
    void discoverOutputNames();
#endif

    ONNXInferenceConfig config_;
    std::unique_ptr<aurora::memory::InferenceBufferPool> buffer_pool_;

    // 统计信息
    ONNXInferenceStats stats_;

    // 线程安全
    mutable std::mutex inference_mutex_;

#ifdef HAVE_ONNXRUNTIME
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_;

    // 模型输出名称
    std::vector<std::string> model_output_names_;
    bool has_log_std_output_ = false;
#endif

    bool model_loaded_ = false;
};

// ===== 模板实现 =====

#ifdef HAVE_ONNXRUNTIME

template<int StateDim, int ActionDim, bool IsContinuous>
ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::ONNXInferenceEngine(
    const ONNXInferenceConfig& config)
    : config_(config)
    , memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    , model_loaded_(false) {

    // 初始化内存池
    if (config_.enable_memory_pool) {
        buffer_pool_ = std::make_unique<aurora::memory::InferenceBufferPool>(
            StateDim, ActionDim, 10);
    }

    try {
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ONNXInferenceEngine");
        AD_INFO("ONNXInferenceEngine", "ONNX Runtime environment initialized (StateDim=%d, ActionDim=%d)",
                StateDim, ActionDim);
    } catch (const Ort::Exception& e) {
        AD_ERROR("ONNXInferenceEngine", "Failed to initialize ONNX Runtime: %s", e.what());
    }
}

template<int StateDim, int ActionDim, bool IsContinuous>
ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::~ONNXInferenceEngine() {
    // 清理资源
}

template<int StateDim, int ActionDim, bool IsContinuous>
bool ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::loadModel(
    const std::string& model_path) {

    if (!env_) {
        AD_ERROR("ONNXInferenceEngine", "ONNX Environment not initialized");
        return false;
    }

    try {
        auto session_options = createSessionOptions();
        session_ = std::make_unique<Ort::Session>(*env_, model_path.c_str(), session_options);

        // 发现输出名称
        discoverOutputNames();

        model_loaded_ = true;
        AD_INFO("ONNXInferenceEngine", "Model loaded from %s", model_path.c_str());
        return true;

    } catch (const Ort::Exception& e) {
        AD_ERROR("ONNXInferenceEngine", "Failed to load model: %s", e.what());
        stats_.recordError();
        model_loaded_ = false;
        return false;
    }
}

template<int StateDim, int ActionDim, bool IsContinuous>
Ort::SessionOptions ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::createSessionOptions() {
    Ort::SessionOptions options;

    // 设置线程数
    options.SetIntraOpNumThreads(config_.num_threads);
    options.SetInterOpNumThreads(1);

    // 设置图优化级别
    auto opt_level = static_cast<GraphOptimizationLevel>(config_.graph_optimization_level);
    options.SetGraphOptimizationLevel(opt_level);

    // 设置执行模式
    if (config_.sequential_execution) {
        options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    }

    // ARM设备特定优化
#ifdef __aarch64__
    options.AddConfigEntry("session.intra_op_num_threads",
                          std::to_string(config_.num_threads).c_str());
    options.AddConfigEntry("session.inter_op_num_threads", "1");
    options.AddConfigEntry("session.enable_mem_reuse", "1");
    options.AddConfigEntry("session.enable_mem_pattern", "1");
    options.AddConfigEntry("session.arena_max_memory",
                          std::to_string(config_.arena_max_memory).c_str());
#endif

    return options;
}

template<int StateDim, int ActionDim, bool IsContinuous>
void ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::discoverOutputNames() {
    model_output_names_.clear();
    has_log_std_output_ = false;

    Ort::AllocatorWithDefaultOptions allocator;
    size_t num_outputs = session_->GetOutputCount();

    for (size_t i = 0; i < num_outputs; ++i) {
        auto output_name_allocated = session_->GetOutputNameAllocated(i, allocator);
        std::string name = output_name_allocated.get();
        model_output_names_.push_back(name);

        AD_DEBUG("ONNXInferenceEngine", "Output [%zu]: %s", i, name.c_str());

        if (name == "action_log_std") {
            has_log_std_output_ = true;
        }
    }

    AD_INFO("ONNXInferenceEngine", "Model has %zu outputs, action_log_std: %s",
            num_outputs, has_log_std_output_ ? "Yes" : "No");
}

template<int StateDim, int ActionDim, bool IsContinuous>
std::vector<Ort::Value> ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::runInferenceInternal(
    const float* input_data) {

    if (!session_) {
        throw std::runtime_error("ONNX session is not initialized");
    }

    // 准备输入tensor
    std::vector<int64_t> input_shape = {1, StateDim};
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info_,
        const_cast<float*>(input_data),
        StateDim,
        input_shape.data(),
        input_shape.size());

    // 准备输出名称
    std::vector<const char*> output_name_ptrs;
    for (const auto& name : model_output_names_) {
        output_name_ptrs.push_back(name.c_str());
    }

    // 输入名称（根据模型可能不同）
    const char* input_names[] = {"input", "state"};

    // 执行推理
    return session_->Run(
        Ort::RunOptions{nullptr},
        input_names,
        &input_tensor,
        1,
        output_name_ptrs.data(),
        output_name_ptrs.size());
}

template<int StateDim, int ActionDim, bool IsContinuous>
typename ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::ResultType
ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::run(const std::vector<double>& state) {
    return run(state.data(), state.size());
}

template<int StateDim, int ActionDim, bool IsContinuous>
typename ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::ResultType
ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::run(const double* state, int state_dim) {
    std::lock_guard<std::mutex> lock(inference_mutex_);

    auto start = std::chrono::high_resolution_clock::now();
    ResultType result;

#ifdef HAVE_ONNXRUNTIME
    if (!session_) {
        result.success = false;
        stats_.recordError();
        return result;
    }

    try {
        // 准备输入数据
        std::vector<float> input_buffer(StateDim);
        size_t copy_size = std::min(static_cast<size_t>(state_dim), static_cast<size_t>(StateDim));
        for (size_t i = 0; i < copy_size; ++i) {
            input_buffer[i] = static_cast<float>(state[i]);
        }
        for (size_t i = copy_size; i < static_cast<size_t>(StateDim); ++i) {
            input_buffer[i] = 0.0f;
        }

        // 运行推理
        auto outputs = runInferenceInternal(input_buffer.data());

        // 解析输出
        if constexpr (IsContinuous) {
            // 连续动作空间
            result.action_mean.resize(ActionDim);
            result.action_log_std.resize(ActionDim);

            for (size_t i = 0; i < model_output_names_.size(); ++i) {
                const std::string& name = model_output_names_[i];
                Ort::Value& output = outputs[i];

                // 使用GetTensorRawData避免模板参数问题
                void* raw_data = output.GetTensorRawData();
                const float* data = static_cast<const float*>(raw_data);

                if (name == "action_mean") {
                    for (int j = 0; j < ActionDim; ++j) {
                        result.action_mean[j] = static_cast<double>(data[j]);
                    }
                } else if (name == "action_log_std" && has_log_std_output_) {
                    for (int j = 0; j < ActionDim; ++j) {
                        result.action_log_std[j] = static_cast<double>(data[j]);
                    }
                } else if (name == "value") {
                    result.value = static_cast<double>(*data);
                }
            }

            // 如果模型没有输出log_std，使用配置值
            if (!has_log_std_output_) {
                for (int j = 0; j < ActionDim; ++j) {
                    result.action_log_std[j] = config_.initial_log_std;
                }
            }
        } else {
            // 离散动作空间
            result.policy_output.resize(ActionDim);

            for (size_t i = 0; i < model_output_names_.size(); ++i) {
                const std::string& name = model_output_names_[i];
                Ort::Value& output = outputs[i];
                auto shape = output.GetTensorTypeAndShapeInfo().GetShape();

                void* raw_data = output.GetTensorRawData();
                const float* data = static_cast<const float*>(raw_data);

                if (name == "output_policy" || name == "policy") {
                    size_t num_actions = shape.size() > 0 ? shape[shape.size() - 1] : ActionDim;
                    for (size_t j = 0; j < std::min(num_actions, static_cast<size_t>(ActionDim)); ++j) {
                        result.policy_output[j] = static_cast<float>(data[j]);
                    }
                } else if (name == "output_value" || name == "value") {
                    result.value_output = data[0];
                }
            }
        }

        result.success = true;

    } catch (const Ort::Exception& e) {
        AD_ERROR("ONNXInferenceEngine", "Inference error: %s", e.what());
        stats_.recordError();
        result.success = false;
    }
#else
    (void)state;
    (void)state_dim;
    result.success = false;
    stats_.recordError();
#endif

    auto end = std::chrono::high_resolution_clock::now();
    result.inference_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    stats_.recordInference(result.inference_time_ms);

    return result;
}

#else // !HAVE_ONNXRUNTIME

// 没有ONNX Runtime时的存根实现
template<int StateDim, int ActionDim, bool IsContinuous>
ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::ONNXInferenceEngine(
    const ONNXInferenceConfig& config)
    : config_(config), model_loaded_(false) {
    AD_WARN("ONNXInferenceEngine", "ONNX Runtime not available");
}

template<int StateDim, int ActionDim, bool IsContinuous>
ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::~ONNXInferenceEngine() = default;

template<int StateDim, int ActionDim, bool IsContinuous>
bool ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::loadModel(
    const std::string& model_path) {
    (void)model_path;
    AD_WARN("ONNXInferenceEngine", "ONNX Runtime not available, cannot load model");
    return false;
}

template<int StateDim, int ActionDim, bool IsContinuous>
typename ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::ResultType
ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::run(const std::vector<double>& state) {
    (void)state;
    ResultType result;
    result.success = false;
    stats_.recordError();
    return result;
}

template<int StateDim, int ActionDim, bool IsContinuous>
typename ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::ResultType
ONNXInferenceEngine<StateDim, ActionDim, IsContinuous>::run(const double* state, int state_dim) {
    (void)state;
    (void)state_dim;
    ResultType result;
    result.success = false;
    stats_.recordError();
    return result;
}

#endif // HAVE_ONNXRUNTIME

// ===== 类型别名 =====

// Auto模式: 25维状态, 4维离散动作
using AutoInferenceEngine = ONNXInferenceEngine<25, 4, false>;

// Humanoid模式: 43维状态, 3维连续动作 (速度命令)
using HumanoidInferenceEngine = ONNXInferenceEngine<43, 3, true>;

} // namespace aurora::planner

#endif // ONNX_INFERENCE_ENGINE_H
