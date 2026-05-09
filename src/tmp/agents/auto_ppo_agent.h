// auto_ppo_agent.h - Auto mode PPO agent (25-dim state, 4-dim discrete actions)
#ifndef AUTO_PPO_AGENT_H
#define AUTO_PPO_AGENT_H

#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <limits>
#include <algorithm>
#include "../maps/costmap.h"
#include "../observation/state_base.h"
#include "../observation/traits/auto_state_traits.h"
#include "common/platform/platform_adapter.h"
#include "common/memory_pool.h"
#include "common/performance_utils.h"
#include "onnx_inference_engine.h"

// ONNX Runtime headers
#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

// Forward declarations for ONNX Runtime
namespace Ort {
class Env;
class Session;
struct SessionOptions;
}

namespace aurora::planner {

/**
 * @brief Auto mode PPO configuration
 *
 * For autonomous driving with:
 * - 25-dimensional observation space
 * - 4-dimensional discrete action space
 */
struct AutoPPOConfig {
    // RL parameters
    double learning_rate = 3e-4;
    double gamma = 0.999;
    double lam = 0.95;
    double clip_epsilon = 0.2;
    double entropy_coef = 0.01;
    double value_loss_coef = 0.5;
    int batch_size = 64;
    int epochs = 10;
    int max_training_steps = 15000;
    int max_episode_steps = 200;

    // Network dimensions
    int state_dim = 25;  // Auto mode: 25-dimensional state
    int action_dim = 4;  // Auto mode: 4 discrete actions

    // Optimization parameters
    bool use_quantized_model = false;
    int num_inference_threads = 1;
    bool enable_simd = true;
    bool enable_memory_pool = true;
    bool use_ppo = true;  // Enable PPO-based planning
    bool use_new_inference_engine = false;  // Use new ONNXInferenceEngine

    AutoPPOConfig() = default;
};

struct AutoTrajectory {
    std::vector<Point> states;
    std::vector<int> actions;
    std::vector<double> rewards;
    std::vector<double> log_probs;
    std::vector<double> values;
    std::vector<bool> dones;
    double total_cost = 0.0;
};

/**
 * @brief Inference performance statistics
 */
struct AutoInferenceStats {
    std::atomic<uint64_t> total_inferences;
    std::atomic<double> total_latency_ms;
    std::atomic<double> min_latency_ms;
    std::atomic<double> max_latency_ms;
    std::atomic<uint64_t> error_count;

    AutoInferenceStats()
        : total_inferences(0),
          total_latency_ms(0.0),
          min_latency_ms(std::numeric_limits<double>::max()),
          max_latency_ms(0.0),
          error_count(0) {}

    void recordInference(double latency_ms) {
        total_inferences++;
        total_latency_ms = total_latency_ms + latency_ms;

        // Update min latency
        double current_min = min_latency_ms.load();
        while (latency_ms < current_min) {
            if (min_latency_ms.compare_exchange_weak(current_min, latency_ms)) {
                break;
            }
        }

        // Update max latency
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
 * @brief Auto mode PPO Agent
 *
 * Optimized for autonomous driving with:
 * - 25-dimensional observation space (24 original + 1 reachability)
 * - 4-dimensional discrete action space
 * - Platform-specific optimizations
 * - Memory pool management
 * - SIMD acceleration
 * - Thread-safe inference
 */
class AutoPPOAgent {
private:
    AutoPPOConfig config_;
    std::unique_ptr<aurora::platform::PlatformAdapter> platform_adapter_;

    // ONNX Runtime components
#ifdef HAVE_ONNXRUNTIME
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_;
#endif

    // Unified inference engine
    std::unique_ptr<AutoInferenceEngine> new_inference_engine_;

    // Memory pool
    std::unique_ptr<aurora::memory::InferenceBufferPool> buffer_pool_;

    // Performance statistics
    AutoInferenceStats inference_stats_;

    // Network dimensions
    int state_dim_;
    int action_dim_;

    // Training statistics
    double total_reward_;
    int episode_count_;

    // Thread safety
    mutable std::mutex inference_mutex_;

    // Persistent RNG (avoid creating on every selectAction)
    static thread_local std::mt19937 rng_;

#ifdef HAVE_ONNXRUNTIME
    std::pair<Ort::Value, Ort::Value> runInference(
        const aurora::planner::State<aurora::planner::AutoStateTraits>& state
    );
#endif

    void initializeFromPlatform();
    void initializeRNG();
    void initializeNewInferenceEngine();

public:
    AutoPPOAgent(const AutoPPOConfig& config = AutoPPOConfig());
    ~AutoPPOAgent();

    // ===== Action Selection =====
    // Type-safe methods using State<AutoStateTraits>
    int selectAction(const Point& state, bool deterministic = false);
    std::vector<double> getActionProbabilities(const Point& state);
    double getValue(const Point& state);

    // New type-safe methods using State<AutoStateTraits>
    int selectAction(const aurora::planner::State<aurora::planner::AutoStateTraits>& state,
                     bool deterministic = false);
    std::vector<double> getActionProbabilities(
        const aurora::planner::State<aurora::planner::AutoStateTraits>& state);

    // Training
    void update(const std::vector<AutoTrajectory>& trajectories);

    // Model management
    bool saveWeights(const std::string& filepath);
    bool loadWeights(const std::string& filepath);
    bool loadOnnxModel(const std::string& filepath);

    // Statistics
    double getTotalReward() const { return total_reward_; }
    int getEpisodeCount() const { return episode_count_; }
    void resetStatistics();

    void setStateDim(int dim) { state_dim_ = dim; }
    int getStateDim() const { return state_dim_; }
    int getMaxEpisodeSteps() const { return config_.max_episode_steps; }
    double getGamma() const { return config_.gamma; }
    double getLambda() const { return config_.lam; }
    void updateConfigFromParameters(const std::map<std::string, double>& parameters);

    // Inference statistics
    const AutoInferenceStats& getInferenceStats() const { return inference_stats_; }
    void resetInferenceStats() { inference_stats_.reset(); }
    double getAverageInferenceLatency() const {
        return inference_stats_.getAverageLatency();
    }
    uint64_t getTotalInferences() const {
        return inference_stats_.total_inferences.load();
    }

    // Platform-specific interfaces
    aurora::platform::PlatformType getPlatform() const {
        return platform_adapter_->getPlatform();
    }
    bool useQuantizedModel() const {
        return config_.use_quantized_model || platform_adapter_->useQuantizedModel();
    }
};

// Type aliases for backward compatibility
using PPOConfig = AutoPPOConfig;
using PPOAgent = AutoPPOAgent;
using Trajectory = AutoTrajectory;
using InferenceStats = AutoInferenceStats;

} // namespace aurora::planner

#endif // AUTO_PPO_AGENT_H
