// auto_ppo_agent.cpp - Auto mode PPO agent (25-dim state, 4-dim discrete actions)
#include "auto_ppo_agent.h"
#include "../observation/state_base.h"
#include "../observation/traits/auto_state_traits.h"
#include <random>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <cstring>
#include <chrono>
#include "common/log/logger.h"

using AutoState = aurora::planner::State<aurora::planner::AutoStateTraits>;

#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace aurora::planner {

// Thread-local RNG
thread_local std::mt19937 AutoPPOAgent::rng_;

// Static init flag
static std::once_flag rng_init_flag;

void AutoPPOAgent::initializeRNG() {
    std::call_once(rng_init_flag, []() {
        std::random_device rd;
        std::seed_seq seq{
            static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()),
            rd(), rd(), rd(), rd(), rd()
        };
        rng_.seed(seq);
    });
}

AutoPPOAgent::AutoPPOAgent(const AutoPPOConfig& config)
    : config_(config),
      state_dim_(25),  // Auto mode: 25-dimensional state
      action_dim_(4),  // Auto mode: 4 discrete actions
      total_reward_(0.0),
      episode_count_(0),

#ifdef HAVE_ONNXRUNTIME
    memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
#endif
{
    // Initialize RNG
    initializeRNG();

    // Initialize platform adapter
    platform_adapter_ = std::make_unique<aurora::platform::PlatformAdapter>();

    // Optimize based on platform
    initializeFromPlatform();

    // Initialize new inference engine (if enabled)
    if (config_.use_new_inference_engine) {
        initializeNewInferenceEngine();
    }

    // Initialize memory pool
    if (config_.enable_memory_pool) {
        buffer_pool_ = std::make_unique<aurora::memory::InferenceBufferPool>(
            state_dim_, action_dim_, config_.batch_size / 10);
    }

    // Initialize ONNX Runtime environment
#ifdef HAVE_ONNXRUNTIME
    if (!config_.use_new_inference_engine) {
        try {
            env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "AutoPPOAgent");
            AD_INFO(AutoPPOAgent, "ONNX Runtime environment initialized successfully");
        } catch (const std::exception& e) {
            AD_ERROR(AutoPPOAgent, "Failed to initialize ONNX Runtime environment: %s", e.what());
            env_ = nullptr;
        }
        session_ = nullptr;
    }
#endif

    AD_INFO(AutoPPOAgent, "Auto PPO Agent initialized with state_dim=%d, action_dim=%d, threads=%d, new_engine=%s",
            state_dim_, action_dim_, config_.num_inference_threads,
            config_.use_new_inference_engine ? "Yes" : "No");
}

AutoPPOAgent::~AutoPPOAgent() = default;

void AutoPPOAgent::initializeFromPlatform() {
    // Optimize based on platform
    config_.use_quantized_model = config_.use_quantized_model || platform_adapter_->useQuantizedModel();

    if (config_.num_inference_threads == 1) {
        config_.num_inference_threads = platform_adapter_->getOptimalThreadCount();
        // Auto mode typically has more resources
        if (config_.num_inference_threads > 8) {
            config_.num_inference_threads = 8;
        }
    }

    // Check SIMD support
    if (!platform_adapter_->hasSIMDSupport()) {
        AD_WARN(AutoPPOAgent, "SIMD not supported on this platform, using standard implementation");
        config_.enable_simd = false;
    }

    AD_DEBUG(AutoPPOAgent, "Platform: %s, Cores: %d, Memory: %lu MB, SIMD: %s, Quantized: %s",
            platform_adapter_->getPlatformName().c_str(),
            platform_adapter_->getCpuCores(),
            platform_adapter_->getTotalMemoryMB(),
            platform_adapter_->hasSIMDSupport() ? "Yes" : "No",
            config_.use_quantized_model ? "Yes" : "No");
}

void AutoPPOAgent::initializeNewInferenceEngine() {
    ONNXInferenceConfig engine_config;
    engine_config.num_threads = config_.num_inference_threads;
    engine_config.enable_memory_pool = config_.enable_memory_pool;
    engine_config.enable_simd = config_.enable_simd;
    engine_config.sequential_execution = true;

    new_inference_engine_ = std::make_unique<AutoInferenceEngine>(engine_config);
    AD_INFO(AutoPPOAgent, "New ONNXInferenceEngine initialized");
}

int AutoPPOAgent::selectAction(const Point& state, bool deterministic) {
    // Use new type-safe State<AutoStateTraits>
    AutoState state_vec;
    state_vec.setFeature(AutoStateTraits::NORM_X, state.x);
    state_vec.setFeature(AutoStateTraits::NORM_Y, state.y);

    auto probs = getActionProbabilities(state);
    if (deterministic) {
        return std::distance(probs.begin(), std::max_element(probs.begin(), probs.end()));
    } else {
        std::discrete_distribution<> dis(probs.begin(), probs.end());
        return dis(rng_);
    }
}

// ===== Type-Safe Action Selection =====

int AutoPPOAgent::selectAction(
    const aurora::planner::State<aurora::planner::AutoStateTraits>& new_state,
    bool deterministic
) {
    std::lock_guard<std::mutex> lock(inference_mutex_);

#ifdef HAVE_ONNXRUNTIME
    if (session_) {
        auto tensors = runInference(new_state);
        float* logits_data = tensors.first.GetTensorMutableData<float>();

        std::vector<float> probs(action_dim_);
        if (config_.enable_simd) {
            aurora::performance::softmaxSIMD(logits_data, probs.data(), action_dim_);
        } else {
            // CPU fallback: compute max, exp, and normalize
            float max_logit = logits_data[0];
            for (int i = 1; i < action_dim_; i++) {
                if (logits_data[i] > max_logit) {
                    max_logit = logits_data[i];
                }
            }

            float sum_exp = 0.0f;
            for (int i = 0; i < action_dim_; i++) {
                probs[i] = std::exp(logits_data[i] - max_logit);
                sum_exp += probs[i];
            }

            for (int i = 0; i < action_dim_; i++) {
                probs[i] /= sum_exp;
            }
        }

        int action;
        if (deterministic) {
            action = std::distance(probs.begin(),
                                  std::max_element(probs.begin(), probs.end()));
        } else {
            std::discrete_distribution<int> dist(probs.begin(), probs.end());
            action = dist(rng_);
        }

        return action;
    }
#endif

    // Fallback: random action
    std::uniform_int_distribution<int> dist(0, action_dim_ - 1);
    return dist(rng_);
}

std::vector<double> AutoPPOAgent::getActionProbabilities(
    const aurora::planner::State<aurora::planner::AutoStateTraits>& new_state
) {
    std::lock_guard<std::mutex> lock(inference_mutex_);

#ifdef HAVE_ONNXRUNTIME
    if (session_) {
        try {
            auto tensors = runInference(new_state);
            float* logits_data = tensors.first.GetTensorMutableData<float>();

            std::vector<double> probs(action_dim_);

            if (config_.enable_simd) {
                std::vector<float> logits_float(logits_data, logits_data + action_dim_);
                std::vector<float> probs_float(action_dim_);
                aurora::performance::softmaxSIMD(logits_float.data(), probs_float.data(), action_dim_);

                for (int i = 0; i < action_dim_; i++) {
                    probs[i] = static_cast<double>(probs_float[i]);
                }
            } else {
                float max_logit = logits_data[0];
                for (int i = 1; i < action_dim_; i++) {
                    if (logits_data[i] > max_logit) {
                        max_logit = logits_data[i];
                    }
                }

                double sum_exp = 0.0;
                for (int i = 0; i < action_dim_; i++) {
                    probs[i] = std::exp(static_cast<double>(logits_data[i]) - max_logit);
                    sum_exp += probs[i];
                }

                for (int i = 0; i < action_dim_; i++) {
                    probs[i] /= sum_exp;
                }
            }

            return probs;
        } catch (const Ort::Exception& e) {
            AD_ERROR(AutoPPOAgent, "ONNX inference failed: %s", e.what());
            return std::vector<double>(action_dim_, 1.0 / action_dim_);
        }
    }
#endif

    // Fallback: uniform distribution
    return std::vector<double>(action_dim_, 1.0 / action_dim_);
}

// ===== END: Type-Safe Action Selection =====

std::vector<double> AutoPPOAgent::getActionProbabilities(const Point& state) {
    // Use new type-safe State<AutoStateTraits>
    AutoState state_vec;
    state_vec.setFeature(AutoStateTraits::NORM_X, state.x);
    state_vec.setFeature(AutoStateTraits::NORM_Y, state.y);

    return getActionProbabilities(state_vec);
}

#ifdef HAVE_ONNXRUNTIME
std::pair<Ort::Value, Ort::Value> AutoPPOAgent::runInference(
    const aurora::planner::State<aurora::planner::AutoStateTraits>& state
) {
    if (!session_) {
        throw std::runtime_error("ONNX session is not initialized");
    }

    // Use RAII for buffer management
    std::vector<float> fallback_buffer(state_dim_);
    float* input_data = fallback_buffer.data();

    if (buffer_pool_) {
        aurora::memory::ScopedInferenceBuffer scoped_buffer(*buffer_pool_);
        input_data = scoped_buffer.state();
    }

    // Prepare input data from State<AutoStateTraits>
    size_t state_size = state.size();
    size_t copy_size = std::min(state_size, static_cast<size_t>(state_dim_));

    for (size_t i = 0; i < copy_size; ++i) {
        input_data[i] = static_cast<float>(state[i]);
    }
    for (size_t i = copy_size; i < static_cast<size_t>(state_dim_); ++i) {
        input_data[i] = 0.0f;
    }

    std::vector<int64_t> input_shape = {1, static_cast<int64_t>(state_dim_)};

    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info_, input_data, static_cast<size_t>(state_dim_),
        input_shape.data(), input_shape.size());

    std::vector<const char*> input_names = {"input"};
    std::vector<const char*> output_names = {"output_policy", "output_value"};

    auto start = std::chrono::high_resolution_clock::now();
    auto output_tensors = session_->Run(
        Ort::RunOptions{nullptr},
        input_names.data(),
        &input_tensor,
        1,
        output_names.data(),
        2);
    auto end = std::chrono::high_resolution_clock::now();

    auto latency = std::chrono::duration<double, std::milli>(end - start).count();
    inference_stats_.recordInference(latency);

    if (output_tensors.size() != 2) {
        throw std::runtime_error("Unexpected number of output tensors");
    }

    Ort::Value policy_tensor = std::move(output_tensors[0]);
    Ort::Value value_tensor = std::move(output_tensors[1]);

    auto policy_info = policy_tensor.GetTensorTypeAndShapeInfo();
    if (policy_info.GetElementCount() != static_cast<size_t>(action_dim_)) {
        AD_WARN(AutoPPOAgent, "Policy tensor size mismatch. Expected: %d, Got: %ld",
                action_dim_, policy_info.GetElementCount());
    }

    return std::make_pair(std::move(policy_tensor), std::move(value_tensor));
}
#endif

double AutoPPOAgent::getValue(const Point& state) {
    // Use new type-safe State<AutoStateTraits>
    AutoState state_vec;
    state_vec.setFeature(AutoStateTraits::NORM_X, state.x);
    state_vec.setFeature(AutoStateTraits::NORM_Y, state.y);

    std::lock_guard<std::mutex> lock(inference_mutex_);

#ifdef HAVE_ONNXRUNTIME
    if (session_) {
        try {
            auto tensors = runInference(state_vec);
            float* value_data = tensors.second.GetTensorMutableData<float>();
            return static_cast<double>(value_data[0]);
        } catch (const Ort::Exception& e) {
            AD_ERROR(PLANNER, "ONNX model inference error: %s", e.what());
            inference_stats_.recordError();
        }
    }
#endif

    return 0.0;
}

void AutoPPOAgent::update(const std::vector<AutoTrajectory>& trajectories) {
    AD_WARN(PLANNER, "Updating AutoPPOAgent with %lu trajectories", trajectories.size());

    for (const auto& traj : trajectories) {
        episode_count_++;
        for (double reward : traj.rewards) {
            total_reward_ += reward;
        }
    }

    AD_WARN(AutoPPOAgent, "AutoPPOAgent updated. Total episodes: %d, Total reward: %.2f",
            episode_count_, total_reward_);
}

bool AutoPPOAgent::saveWeights(const std::string& filepath) {
    AD_WARN(AutoPPOAgent, "Saving weights not implemented for ONNX Runtime");
    return false;
}

bool AutoPPOAgent::loadWeights(const std::string& filepath) {
    return loadOnnxModel(filepath);
}

bool AutoPPOAgent::loadOnnxModel(const std::string& filepath) {
    if (config_.use_new_inference_engine && new_inference_engine_) {
        bool success = new_inference_engine_->loadModel(filepath);
        if (success) {
            AD_INFO(AutoPPOAgent, "ONNX model loaded successfully using new engine from %s",
                    filepath.c_str());
        } else {
            AD_ERROR(AutoPPOAgent, "Failed to load ONNX model using new engine from %s",
                    filepath.c_str());
        }
        return success;
    }

#ifdef HAVE_ONNXRUNTIME
    if (!env_) {
        AD_ERROR(AutoPPOAgent, "ONNX Environment invalid");
        return false;
    }

    try {
        session_.reset();

        Ort::SessionOptions session_options;

        session_options.SetIntraOpNumThreads(config_.num_inference_threads);
        session_options.SetInterOpNumThreads(1);

        session_options.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef __aarch64__
        session_options.AddConfigEntry("session.intra_op_num_threads",
                                     std::to_string(config_.num_inference_threads).c_str());
        session_options.AddConfigEntry("session.inter_op_num_threads", "1");
        session_options.AddConfigEntry("session.enable_mem_reuse", "1");
        session_options.AddConfigEntry("session.enable_mem_pattern", "1");
        session_options.AddConfigEntry("session.arena_max_memory", "1073741824"); // 1GB
#endif

        session_ = std::make_unique<Ort::Session>(*env_, filepath.c_str(), session_options);

        AD_INFO(AutoPPOAgent, "ONNX model loaded successfully from %s (threads=%d)",
                filepath.c_str(), config_.num_inference_threads);
        return true;
    } catch (const Ort::Exception& e) {
        AD_ERROR(AutoPPOAgent, "Failed to load ONNX model from %s: %s",
                filepath.c_str(), e.what());
        session_ = nullptr;
        return false;
    }
#else
    AD_WARN(AutoPPOAgent, "ONNX Runtime not enabled, cannot load model from %s", filepath.c_str());
    return false;
#endif
}

void AutoPPOAgent::resetStatistics() {
    total_reward_ = 0.0;
    episode_count_ = 0;
}

void AutoPPOAgent::updateConfigFromParameters(const std::map<std::string, double>& parameters) {
    auto findParam = [&](const std::vector<std::string>& keys) -> std::map<std::string, double>::const_iterator {
        for (const auto& key : keys) {
            auto it = parameters.find(key);
            if (it != parameters.end()) {
                return it;
            }
        }
        return parameters.end();
    };

    // Update PPO config (with new key format, backward compatible)
    auto it = findParam({"auto_ppo_learning_rate", "ppo_config_learning_rate"});
    if (it != parameters.end()) config_.learning_rate = it->second;

    it = findParam({"auto_ppo_gamma", "ppo_config_gamma"});
    if (it != parameters.end()) config_.gamma = it->second;

    it = findParam({"auto_ppo_gae_lambda", "ppo_config_gae_lambda"});
    if (it != parameters.end()) config_.lam = it->second;

    it = findParam({"auto_ppo_clip_epsilon", "ppo_config_clip_epsilon"});
    if (it != parameters.end()) config_.clip_epsilon = it->second;

    it = findParam({"auto_ppo_entropy_coef", "ppo_config_entropy_coef"});
    if (it != parameters.end()) config_.entropy_coef = it->second;

    it = findParam({"auto_ppo_value_loss_coef", "ppo_config_value_loss_coef"});
    if (it != parameters.end()) config_.value_loss_coef = it->second;

    it = findParam({"auto_ppo_batch_size", "ppo_config_batch_size"});
    if (it != parameters.end()) config_.batch_size = static_cast<int>(it->second);

    it = findParam({"auto_ppo_epochs", "ppo_config_epochs"});
    if (it != parameters.end()) config_.epochs = static_cast<int>(it->second);

    it = findParam({"auto_ppo_max_training_steps", "ppo_config_max_training_steps"});
    if (it != parameters.end()) config_.max_training_steps = static_cast<int>(it->second);

    it = findParam({"auto_ppo_max_episode_steps", "ppo_config_max_episode_steps"});
    if (it != parameters.end()) config_.max_episode_steps = static_cast<int>(it->second);

    // Update optimization parameters
    it = findParam({"auto_nav_planner_use_quantized_model", "nav_planner_use_quantized_model"});
    if (it != parameters.end()) config_.use_quantized_model = (it->second > 0.5);

    it = findParam({"auto_nav_planner_enable_simd", "nav_planner_enable_simd"});
    if (it != parameters.end()) config_.enable_simd = (it->second > 0.5);

    it = findParam({"auto_nav_planner_enable_memory_pool", "nav_planner_enable_memory_pool"});
    if (it != parameters.end()) config_.enable_memory_pool = (it->second > 0.5);

    it = findParam({"auto_nav_planner_inference_threads", "nav_planner_inference_threads"});
    if (it != parameters.end()) config_.num_inference_threads = static_cast<int>(it->second);

    it = findParam({"auto_nav_planner_use_ppo", "nav_planner_use_ppo"});
    if (it != parameters.end()) config_.use_ppo = (it->second > 0.5);
}

} // namespace aurora::planner
