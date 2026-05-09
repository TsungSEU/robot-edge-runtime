// planner_config_manager.cpp
#include "planner_config_manager.h"
#include <fstream>
#include <sstream>
#include "common/log/logger.h"

#ifdef HAVE_YAMLCPP
#include <yaml-cpp/yaml.h>
#endif

namespace aurora::planner {

PlannerConfigManager& PlannerConfigManager::getInstance() {
    static PlannerConfigManager instance;
    return instance;
}

bool PlannerConfigManager::loadFromYaml(const std::string& config_file,
                                        PlannerConfigData& output_config) {
#ifdef HAVE_YAMLCPP
    try {
        YAML::Node config = YAML::LoadFile(config_file);

        // 解析模式
        std::string mode_str = config["mode"] ? config["mode"].as<std::string>() : "humanoid";
        if (mode_str == "auto") {
            output_config.mode = PlannerMode::AUTO;
        } else if (mode_str == "humanoid") {
            output_config.mode = PlannerMode::HUMANOID;
            output_config.mode = PlannerMode::HUMANOID;
        }

        // 解析模型路径
        if (config["model_path"]) {
            output_config.model_path = config["model_path"].as<std::string>();
        }

        // 解析PPO配置
        if (config["ppo_config"]) {
            if (output_config.mode == PlannerMode::AUTO) {
                parseAutoAutoPPOConfig(config["ppo_config"], output_config.auto_ppo_config);
            } else {
                parseHumanoidAutoPPOConfig(config["ppo_config"], output_config.humanoid_ppo_config);
            }
        }

        // 解析奖励配置
        if (config["reward_config"]) {
            parseRewardConfig(config["reward_config"], output_config.reward_config);
        }

        // 解析数据价值配置
        if (config["value_config"]) {
            parseValueConfig(config["value_config"], output_config.value_config);
        }

        // 解析成本地图配置
        if (config["costmap_config"]) {
            parseCostMapConfig(config["costmap_config"], output_config.costmap_config);
        }

        // 解析规划器参数
        if (config["planner_parameters"]) {
            parsePlannerParameters(config["planner_parameters"], output_config.planner_parameters);
        }

        // 解析observation_space
        if (config["observation_space"]) {
            int obs_dim = config["observation_space"].as<int>();
            if (output_config.mode == PlannerMode::AUTO) {
                // Auto模式可能使用不同的维度
                output_config.auto_ppo_config.state_dim = obs_dim;
            } else {
                output_config.humanoid_ppo_config.state_dim = obs_dim;
            }
        }

        AD_INFO("PlannerConfigManager", "Loaded config from %s (mode: %s)",
                config_file.c_str(), mode_str.c_str());

        return true;

    } catch (const YAML::Exception& e) {
        AD_ERROR("PlannerConfigManager", "Failed to parse YAML: %s", e.what());
        return false;
    }
#else
    (void)config_file;
    (void)output_config;
    AD_WARN("PlannerConfigManager", "YAML-cpp not available, cannot load config");
    return false;
#endif
}

bool PlannerConfigManager::saveToYaml(const std::string& config_file,
                                      const PlannerConfigData& config) {
#ifdef HAVE_YAMLCPP
    try {
        YAML::Node root;

        // 模式
        std::string mode_str = getModeString(static_cast<PlannerBase*>(nullptr));  // 获取模式字符串
        if (config.mode == PlannerMode::AUTO) mode_str = "auto";
        else if (config.mode == PlannerMode::HUMANOID) mode_str = "humanoid";
        else if (config.mode == PlannerMode::HUMANOID) mode_str = "rule";
        root["mode"] = mode_str;

        // 模型路径
        if (!config.model_path.empty()) {
            root["model_path"] = config.model_path;
        }

        // 成本地图配置
        root["costmap_config"]["sparse_threshold"] = config.costmap_config.sparse_threshold;
        root["costmap_config"]["exploration_bonus"] = config.costmap_config.exploration_bonus;
        root["costmap_config"]["redundancy_penalty"] = config.costmap_config.redundancy_penalty;

        // 观测空间
        if (config.mode == PlannerMode::HUMANOID) {
            root["observation_space"] = config.humanoid_ppo_config.state_dim;
        } else {
            root["observation_space"] = config.auto_ppo_config.state_dim;
        }

        // 写入文件
        std::ofstream fout(config_file);
        fout << root;
        AD_INFO("PlannerConfigManager", "Saved config to %s", config_file.c_str());

        return true;

    } catch (const YAML::Exception& e) {
        AD_ERROR("PlannerConfigManager", "Failed to save YAML: %s", e.what());
        return false;
    }
#else
    (void)config_file;
    (void)config;
    AD_WARN("PlannerConfigManager", "YAML-cpp not available, cannot save config");
    return false;
#endif
}

void PlannerConfigManager::updateFromMap(const std::map<std::string, double>& parameters,
                                        PlannerConfigData& config) {
    for (const auto& [key, value] : parameters) {
        // Auto模式PPO配置
        if (key == "auto_ppo_learning_rate" || key == "ppo_config_learning_rate") {
            config.auto_ppo_config.learning_rate = value;
        } else if (key == "auto_ppo_gamma" || key == "ppo_config_gamma") {
            config.auto_ppo_config.gamma = value;
        } else if (key == "auto_ppo_gae_lambda" || key == "ppo_config_gae_lambda") {
            config.auto_ppo_config.lam = value;
        } else if (key == "auto_ppo_clip_epsilon" || key == "ppo_config_clip_epsilon") {
            config.auto_ppo_config.clip_epsilon = value;
        } else if (key == "auto_ppo_entropy_coef" || key == "ppo_config_entropy_coef") {
            config.auto_ppo_config.entropy_coef = value;
        } else if (key == "auto_ppo_value_loss_coef" || key == "ppo_config_value_loss_coef") {
            config.auto_ppo_config.value_loss_coef = value;
        }

        // Humanoid模式PPO配置
        else if (key == "humanoid_inference_action_scale") {
            config.humanoid_ppo_config.action_scale = value;
        } else if (key == "humanoid_inference_init_log_std") {
            config.humanoid_ppo_config.init_log_std = value;
        } else if (key == "humanoid_inference_min_log_std") {
            config.humanoid_ppo_config.min_log_std = value;
        } else if (key == "humanoid_inference_max_log_std") {
            config.humanoid_ppo_config.max_log_std = value;
        }

        // 成本地图配置
        else if (key == "sparse_threshold") {
            config.costmap_config.sparse_threshold = value;
        } else if (key == "exploration_bonus") {
            config.costmap_config.exploration_bonus = value;
        } else if (key == "redundancy_penalty") {
            config.costmap_config.redundancy_penalty = value;
        }

        // 通用参数
        config.planner_parameters[key] = value;
    }
}

PlannerConfigData PlannerConfigManager::getDefaultConfig(PlannerMode mode) {
    PlannerConfigData config;
    config.mode = mode;

    switch (mode) {
        case PlannerMode::AUTO:
            config.auto_ppo_config = AutoPPOConfig();
            config.auto_ppo_config.state_dim = 25;
            config.auto_ppo_config.action_dim = 4;
            config.model_path = "models/auto_ppo.onnx";
            break;

        case PlannerMode::HUMANOID:
            config.humanoid_ppo_config = HumanoidPPOConfig();
            config.humanoid_ppo_config.state_dim = 43;
            config.humanoid_ppo_config.action_dim = 3;
            config.model_path = "models/humanoid_ppo.onnx";
            break;
    }

    // 默认成本地图配置
    config.costmap_config.sparse_threshold = 0.15;
    config.costmap_config.exploration_bonus = 10.0;
    config.costmap_config.redundancy_penalty = 5.0;

    return config;
}

size_t PlannerConfigManager::registerCallback(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
    return callbacks_.size() - 1;
}

void PlannerConfigManager::unregisterCallback(size_t callback_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (callback_id < callbacks_.size()) {
        callbacks_[callback_id] = nullptr;
    }
}

void PlannerConfigManager::notifyCallbacks(const PlannerConfigData& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& callback : callbacks_) {
        if (callback) {
            try {
                callback(config);
            } catch (const std::exception& e) {
                AD_ERROR("PlannerConfigManager", "Config change callback error: %s", e.what());
            }
        }
    }
}

std::string PlannerConfigManager::getConfigPath(PlannerMode mode, const std::string& base_path) {
    std::string filename;
    switch (mode) {
        case PlannerMode::AUTO: filename = "auto_planner_weights.yaml"; break;
        case PlannerMode::HUMANOID: filename = "planner_weights.yaml"; break;
    }
    return base_path.empty() ? filename : base_path + "/" + filename;
}

std::string PlannerConfigManager::getModelPath(PlannerMode mode, const std::string& base_path) {
    std::string filename;
    switch (mode) {
        case PlannerMode::AUTO: filename = "auto_ppo.onnx"; break;
        case PlannerMode::HUMANOID: filename = "humanoid_ppo.onnx"; break;
    }
    return base_path.empty() ? filename : base_path + "/" + filename;
}

#ifdef HAVE_YAMLCPP

void PlannerConfigManager::parseAutoAutoPPOConfig(const YAML::Node& node, AutoPPOConfig& config) {
    if (node["learning_rate"]) config.learning_rate = node["learning_rate"].as<double>();
    if (node["gamma"]) config.gamma = node["gamma"].as<double>();
    if (node["gae_lambda"] || node["lam"]) config.lam = (node["gae_lambda"] ? node["gae_lambda"].as<double>() : node["lam"].as<double>());
    if (node["clip_epsilon"]) config.clip_epsilon = node["clip_epsilon"].as<double>();
    if (node["entropy_coef"]) config.entropy_coef = node["entropy_coef"].as<double>();
    if (node["value_loss_coef"]) config.value_loss_coef = node["value_loss_coef"].as<double>();
    if (node["batch_size"]) config.batch_size = node["batch_size"].as<int>();
    if (node["epochs"]) config.epochs = node["epochs"].as<int>();
    if (node["max_training_steps"]) config.max_training_steps = node["max_training_steps"].as<int>();
    if (node["max_episode_steps"]) config.max_episode_steps = node["max_episode_steps"].as<int>();

    // 优化参数
    if (node["use_quantized_model"]) config.use_quantized_model = node["use_quantized_model"].as<bool>();
    if (node["num_inference_threads"]) config.num_inference_threads = node["num_inference_threads"].as<int>();
    if (node["enable_simd"]) config.enable_simd = node["enable_simd"].as<bool>();
    if (node["enable_memory_pool"]) config.enable_memory_pool = node["enable_memory_pool"].as<bool>();
}

void PlannerConfigManager::parseHumanoidAutoPPOConfig(const YAML::Node& node, ContinuousAutoPPOConfig& config) {
    if (node["learning_rate"]) config.learning_rate = node["learning_rate"].as<double>();
    if (node["gamma"]) config.gamma = node["gamma"].as<double>();
    if (node["gae_lambda"] || node["lam"]) config.lam = (node["gae_lambda"] ? node["gae_lambda"].as<double>() : node["lam"].as<double>());
    if (node["clip_epsilon"]) config.clip_epsilon = node["clip_epsilon"].as<double>();
    if (node["entropy_coef"]) config.entropy_coef = node["entropy_coef"].as<double>();
    if (node["value_loss_coef"]) config.value_loss_coef = node["value_loss_coef"].as<double>();
    if (node["max_grad_norm"]) config.max_grad_norm = node["max_grad_norm"].as<double>();
    if (node["batch_size"]) config.batch_size = node["batch_size"].as<int>();
    if (node["epochs"]) config.epochs = node["epochs"].as<int>();
    if (node["max_training_steps"]) config.max_training_steps = node["max_training_steps"].as<int>();
    if (node["max_episode_steps"]) config.max_episode_steps = node["max_episode_steps"].as<int>();

    // 连续动作空间参数
    if (node["action_scale"]) config.action_scale = node["action_scale"].as<double>();
    if (node["action_bias"]) config.action_bias = node["action_bias"].as<double>();
    if (node["init_log_std"]) config.init_log_std = node["init_log_std"].as<double>();
    if (node["min_log_std"]) config.min_log_std = node["min_log_std"].as<double>();
    if (node["max_log_std"]) config.max_log_std = node["max_log_std"].as<double>();

    // 网络结构
    if (node["state_dim"]) config.state_dim = node["state_dim"].as<int>();
    if (node["action_dim"]) config.action_dim = node["action_dim"].as<int>();
    if (node["hidden_dim"]) config.hidden_dim = node["hidden_dim"].as<int>();
    if (node["num_layers"]) config.num_layers = node["num_layers"].as<int>();

    // 优化参数
    if (node["use_quantized_model"]) config.use_quantized_model = node["use_quantized_model"].as<bool>();
    if (node["num_inference_threads"]) config.num_inference_threads = node["num_inference_threads"].as<int>();
    if (node["enable_memory_pool"]) config.enable_memory_pool = node["enable_memory_pool"].as<bool>();

    // 探索参数
    if (node["initial_epsilon"]) config.initial_epsilon = node["initial_epsilon"].as<double>();
    if (node["epsilon_decay"]) config.epsilon_decay = node["epsilon_decay"].as<double>();
    if (node["min_epsilon"]) config.min_epsilon = node["min_epsilon"].as<double>();
}

void PlannerConfigManager::parseRewardConfig(const YAML::Node& node, HumanoidRewardConfig& config) {
    if (node["goal_reward"]) config.goal_reward = node["goal_reward"].as<double>();
    if (node["fall_penalty"]) config.fall_penalty = node["fall_penalty"].as<double>();
    if (node["step_penalty"]) config.step_penalty = node["step_penalty"].as<double>();
    if (node["collision_penalty"]) config.collision_penalty = node["collision_penalty"].as<double>();
    if (node["energy_penalty"]) config.energy_penalty = node["energy_penalty"].as<double>();
    if (node["progress_reward"]) config.progress_reward = node["progress_reward"].as<double>();
    if (node["time_penalty"]) config.time_penalty = node["time_penalty"].as<double>();
    if (node["stability_bonus"]) config.stability_bonus = node["stability_bonus"].as<double>();
}

void PlannerConfigManager::parseValueConfig(const YAML::Node& node, DataValueConfig& config) {
    if (node["sparsity_weight"]) config.sparsity_weight = node["sparsity_weight"].as<double>();
    if (node["novelty_weight"]) config.novelty_weight = node["novelty_weight"].as<double>();
    if (node["diversity_weight"]) config.diversity_weight = node["diversity_weight"].as<double>();
    if (node["scene_importance_weight"]) config.scene_importance_weight = node["scene_importance_weight"].as<double>();
}

void PlannerConfigManager::parseCostMapConfig(const YAML::Node& node, PlannerConfigData::CostMapConfig& config) {
    if (node["sparse_threshold"]) config.sparse_threshold = node["sparse_threshold"].as<double>();
    if (node["exploration_bonus"]) config.exploration_bonus = node["exploration_bonus"].as<double>();
    if (node["redundancy_penalty"]) config.redundancy_penalty = node["redundancy_penalty"].as<double>();
    if (node["grid_size"]) config.grid_size = node["grid_size"].as<int>();
    if (node["resolution"]) config.resolution = node["resolution"].as<double>();
}

void PlannerConfigManager::parsePlannerParameters(const YAML::Node& node, std::map<std::string, double>& params) {
    for (const auto& kv : node) {
        std::string key = kv.first.as<std::string>();
        double value = kv.second.as<double>();
        params[key] = value;
    }
}

#endif // HAVE_YAMLCPP

} // namespace aurora::planner
