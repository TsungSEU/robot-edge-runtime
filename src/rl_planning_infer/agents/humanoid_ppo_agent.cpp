// humanoid_ppo_agent.cpp
#include "humanoid_ppo_agent.h"
#include "common/log/logger.h"
#include <chrono>
#include <cmath>
#include <algorithm>

namespace aurora::planner {

HumanoidPPOAgent::HumanoidPPOAgent(const HumanoidPPOConfig& config)
    : config_(config)
#ifdef HAVE_ONNXRUNTIME
    , memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
#endif
{
    std::random_device rd;
    rng_.seed(rd());

#ifdef HAVE_ONNXRUNTIME
    try {
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "HumanoidPPO");
        session_options_ = std::make_unique<Ort::SessionOptions>();
        session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (config_.num_inference_threads > 0) {
            session_options_->SetIntraOpNumThreads(config_.num_inference_threads);
        }
        session_options_->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    } catch (const Ort::Exception& e) {
        AD_ERROR("HumanoidPPOAgent", "ONNX Runtime init error: %s", e.what());
    }
#endif
}

HumanoidPPOAgent::~HumanoidPPOAgent() = default;

bool HumanoidPPOAgent::loadOnnxModel(const std::string& model_path) {
#ifdef HAVE_ONNXRUNTIME
    if (!env_) {
        AD_ERROR("HumanoidPPOAgent", "ONNX Environment not initialized");
        return false;
    }

    try {
        session_ = std::make_unique<Ort::Session>(*env_, model_path.c_str(),
                                                   *session_options_);

        // 发现模型输出名称
        model_output_names_.clear();
        has_log_std_output_ = false;

        Ort::AllocatorWithDefaultOptions allocator;
        size_t num_outputs = session_->GetOutputCount();
        AD_INFO("HumanoidPPOAgent", "ONNX Model has %zu outputs:", num_outputs);

        for (size_t i = 0; i < num_outputs; ++i) {
            auto output_name_allocated = session_->GetOutputNameAllocated(i, allocator);
            std::string name = output_name_allocated.get();
            model_output_names_.push_back(name);
            AD_INFO("HumanoidPPOAgent", "  [%zu] %s", i, name.c_str());

            if (name == "action_log_std") {
                has_log_std_output_ = true;
            }
        }

        model_loaded_ = true;
        AD_INFO("HumanoidPPOAgent", "Loaded Humanoid ONNX model: %s (state=%d, action=%d)",
                model_path.c_str(), config_.state_dim, config_.action_dim);
        return true;

    } catch (const Ort::Exception& e) {
        AD_ERROR("HumanoidPPOAgent", "Failed to load ONNX model: %s", e.what());
        error_count_++;
        model_loaded_ = false;
        return false;
    }
#else
    AD_WARN("HumanoidPPOAgent", "ONNX Runtime not available");
    return false;
#endif
}

HumanoidAction HumanoidPPOAgent::selectAction(const HumanoidState& state,
                                             std::vector<double>& action_raw,
                                             double& log_prob) {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    auto start_time = std::chrono::high_resolution_clock::now();

#ifdef HAVE_ONNXRUNTIME
    if (!session_) {
        action_raw.assign(3, 0.0);
        log_prob = 0.0;
        return HumanoidAction();
    }

    try {
        auto output = runInference(state);

        // 采样动作: action = mean + std * noise
        std::normal_distribution<double> dist(0.0, 1.0);
        action_raw.resize(config_.action_dim);
        for (int i = 0; i < config_.action_dim; ++i) {
            double std_val = std::exp(output.action_log_std[i]);
            action_raw[i] = output.action_mean[i] + std_val * dist(rng_);
        }

        // 裁剪到 [-1, 1]
        for (auto& v : action_raw) {
            v = std::clamp(v, -1.0, 1.0);
        }

        // 计算对数概率
        log_prob = 0.0;
        for (int i = 0; i < config_.action_dim; ++i) {
            double diff = action_raw[i] - output.action_mean[i];
            double var = std::exp(2.0 * output.action_log_std[i]);
            log_prob += -0.5 * (diff * diff / var + std::log(2.0 * M_PI * var));
        }

        // 更新统计
        auto end_time = std::chrono::high_resolution_clock::now();
        double latency = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        total_inferences_++;
        double current = total_latency_ms_.load();
        while (!total_latency_ms_.compare_exchange_weak(current, current + latency)) {}

        return HumanoidAction::fromNormalized(action_raw);

    } catch (const Ort::Exception& e) {
        error_count_++;
        AD_ERROR("HumanoidPPOAgent", "Inference error: %s", e.what());
        action_raw.assign(3, 0.0);
        log_prob = 0.0;
        return HumanoidAction();
    }
#else
    action_raw.assign(3, 0.0);
    log_prob = 0.0;
    return HumanoidAction();
#endif
}

HumanoidAction HumanoidPPOAgent::selectActionDeterministic(const HumanoidState& state) {
    std::lock_guard<std::mutex> lock(inference_mutex_);

#ifdef HAVE_ONNXRUNTIME
    if (!session_) return HumanoidAction();

    try {
        auto output = runInference(state);
        std::vector<double> action_vec(output.action_mean.begin(),
                                       output.action_mean.end());
        return HumanoidAction::fromNormalized(action_vec);
    } catch (const Ort::Exception& e) {
        error_count_++;
        return HumanoidAction();
    }
#else
    return HumanoidAction();
#endif
}

double HumanoidPPOAgent::evaluateValue(const HumanoidState& state) {
    std::lock_guard<std::mutex> lock(inference_mutex_);

#ifdef HAVE_ONNXRUNTIME
    if (!session_) return 0.0;

    try {
        auto output = runInference(state);
        return output.value;
    } catch (const Ort::Exception& e) {
        error_count_++;
        return 0.0;
    }
#else
    return 0.0;
#endif
}

#ifdef HAVE_ONNXRUNTIME
HumanoidPPOAgent::HumanoidInferenceOutput HumanoidPPOAgent::runInference(const HumanoidState& state) {
    const auto& features = state.getFeatures();

    // 准备输入 tensor
    std::vector<float> input_data(config_.state_dim);
    size_t copy_size = std::min(features.size(), static_cast<size_t>(config_.state_dim));
    for (size_t i = 0; i < copy_size; ++i) {
        input_data[i] = static_cast<float>(features[i]);
    }
    for (size_t i = copy_size; i < static_cast<size_t>(config_.state_dim); ++i) {
        input_data[i] = 0.0f;
    }

    std::vector<int64_t> input_shape = {1, config_.state_dim};
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info_,
        input_data.data(),
        config_.state_dim,
        input_shape.data(),
        input_shape.size());

    const char* input_names[] = {"state"};

    std::vector<const char*> output_name_ptrs;
    for (const auto& name : model_output_names_) {
        output_name_ptrs.push_back(name.c_str());
    }

    auto outputs = session_->Run(
        Ort::RunOptions{nullptr},
        input_names,
        &input_tensor,
        1,
        output_name_ptrs.data(),
        output_name_ptrs.size());

    // 解析输出
    HumanoidInferenceOutput result;
    result.action_mean.assign(config_.action_dim, 0.0);
    result.action_log_std.assign(config_.action_dim, config_.init_log_std);
    result.value = 0.0;

    for (size_t i = 0; i < model_output_names_.size(); ++i) {
        const std::string& name = model_output_names_[i];
        auto* data = outputs[i].GetTensorData<float>();

        if (name == "action_mean") {
            for (int j = 0; j < config_.action_dim; ++j) {
                result.action_mean[j] = static_cast<double>(data[j]);
            }
        } else if (name == "action_log_std" && has_log_std_output_) {
            for (int j = 0; j < config_.action_dim; ++j) {
                result.action_log_std[j] = static_cast<double>(data[j]);
            }
        } else if (name == "value") {
            result.value = static_cast<double>(*data);
        }
    }

    return result;
}
#endif

} // namespace aurora::planner
