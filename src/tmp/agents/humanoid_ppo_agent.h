// humanoid_ppo_agent.h
// Humanoid PPO Agent (43-dim state, 3-dim continuous action)
#ifndef HUMANOID_PPO_AGENT_H
#define HUMANOID_PPO_AGENT_H

#include "humanoid_state.h"
#include "humanoid_action.h"
#include "onnx_inference_engine.h"

#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <random>

// ONNX Runtime headers
#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace aurora::planner {

/**
 * @brief Humanoid PPO 推理配置
 *
 * 对齐训练侧 humanoid_nav_data_training.yaml
 */
struct HumanoidPPOConfig {
    // 网络结构 (对齐训练配置)
    int state_dim = HumanoidState::STATE_DIM;   // 43
    int action_dim = HumanoidAction::ACTION_DIM; // 3
    int hidden_dim = 256;
    int num_layers = 3;             // [256, 128, 64]

    // 连续动作空间参数 (对齐训练配置)
    double action_scale = 1.0;
    double init_log_std = 0.0;      // 对齐训练 init_noise_std: 0.5 → log_std: 0.0
    double min_log_std = -20.0;
    double max_log_std = 2.0;

    // 推理参数
    int num_inference_threads = 1;
    bool enable_memory_pool = true;

    // 探索参数
    double initial_epsilon = 0.1;
    double epsilon_decay = 0.995;
    double min_epsilon = 0.01;

    HumanoidPPOConfig() = default;
};

/**
 * @brief Humanoid PPO Agent (端侧推理)
 *
 * 专为 Humanoid 场景设计:
 * - 43维状态空间 (基座速度 + 导航 + 数据价值扇区 + 障碍物 + 采集状态 + 动作历史)
 * - 3维连续动作空间 (forward, lateral, angular velocity)
 * - ONNX Runtime 推理
 */
class HumanoidPPOAgent {
public:
    struct InferenceStats {
        uint64_t total_count = 0;
        double avg_latency_ms = 0.0;
        uint64_t error_count = 0;
    };

    explicit HumanoidPPOAgent(const HumanoidPPOConfig& config = HumanoidPPOConfig());
    ~HumanoidPPOAgent();

    // 禁止拷贝
    HumanoidPPOAgent(const HumanoidPPOAgent&) = delete;
    HumanoidPPOAgent& operator=(const HumanoidPPOAgent&) = delete;

    /**
     * @brief 选择动作 (带探索噪声)
     */
    HumanoidAction selectAction(const HumanoidState& state,
                               std::vector<double>& action_raw,
                               double& log_prob);

    /**
     * @brief 选择确定性动作
     */
    HumanoidAction selectActionDeterministic(const HumanoidState& state);

    /**
     * @brief 评估状态价值
     */
    double evaluateValue(const HumanoidState& state);

    // ===== 模型管理 =====

    /**
     * @brief 加载 ONNX 模型
     */
    bool loadOnnxModel(const std::string& model_path);

    bool isModelLoaded() const { return model_loaded_; }

    // ===== 统计 =====

    InferenceStats getInferenceStats() const {
        return {total_inferences_.load(),
                total_inferences_.load() > 0
                    ? total_latency_ms_.load() / total_inferences_.load()
                    : 0.0,
                error_count_.load()};
    }

    void resetStatistics() {
        total_inferences_ = 0;
        total_latency_ms_ = 0.0;
        error_count_ = 0;
    }

    const HumanoidPPOConfig& getConfig() const { return config_; }

private:
    HumanoidPPOConfig config_;
    bool model_loaded_ = false;

#ifdef HAVE_ONNXRUNTIME
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    Ort::MemoryInfo memory_info_;

    std::vector<std::string> model_output_names_;
    bool has_log_std_output_ = false;

    /**
     * @brief ONNX 推理 (返回 action_mean, action_log_std, value)
     */
    struct HumanoidInferenceOutput {
        std::vector<double> action_mean;
        std::vector<double> action_log_std;
        double value;
    };
    HumanoidInferenceOutput runInference(const HumanoidState& state);
#endif

    // 统计
    std::atomic<uint64_t> total_inferences_{0};
    std::atomic<double> total_latency_ms_{0.0};
    std::atomic<uint64_t> error_count_{0};

    // 线程安全
    mutable std::mutex inference_mutex_;

    // 随机数
    std::mt19937 rng_;
};

} // namespace aurora::planner

#endif // HUMANOID_PPO_AGENT_H
