// planner_config_manager.h
#ifndef PLANNER_CONFIG_MANAGER_H
#define PLANNER_CONFIG_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

#include "../core/planner_base.hpp"
#include "../agents/auto_ppo_agent.h"
#include "../agents/humanoid_ppo_agent.h"
#include "../agents/humanoid_reward.h"
#include "../maps/humanoid_data_value.h"

// YAMLcpp
#ifdef HAVE_YAMLCPP
#include <yaml-cpp/yaml.h>
#endif

namespace aurora::planner {

/**
 * @brief 规划器配置数据
 */
struct PlannerConfigData {
    // 通用配置
    PlannerMode mode = PlannerMode::HUMANOID;
    std::string model_path;
    std::string config_file;

    // PPO配置 (Auto模式)
    AutoPPOConfig auto_ppo_config;

    // Humanoid PPO配置 (43-dim state, 3-dim action)
    HumanoidPPOConfig humanoid_ppo_config;

    // 奖励配置
    HumanoidRewardConfig reward_config;

    // 数据价值配置
    DataValueConfig value_config;

    // 成本地图配置
    struct CostMapConfig {
        double sparse_threshold = 0.15;
        double exploration_bonus = 10.0;
        double redundancy_penalty = 5.0;
        int grid_size = 100;
        double resolution = 1.0;
    } costmap_config;

    // 规划器参数
    std::map<std::string, double> planner_parameters;

    PlannerConfigData() = default;
};

/**
 * @brief 规划器配置管理器
 *
 * 统一管理所有规划器模式的配置加载、热重载和参数更新
 */
class PlannerConfigManager {
public:
    /**
     * @brief 配置变更回调函数类型
     */
    using ConfigChangeCallback = std::function<void(const PlannerConfigData&)>;

    /**
     * @brief 获取单例实例
     */
    static PlannerConfigManager& getInstance();

    // 禁止拷贝和移动
    PlannerConfigManager(const PlannerConfigManager&) = delete;
    PlannerConfigManager& operator=(const PlannerConfigManager&) = delete;
    PlannerConfigManager(PlannerConfigManager&&) = delete;
    PlannerConfigManager& operator=(PlannerConfigManager&&) = delete;

    /**
     * @brief 从YAML文件加载配置
     * @param config_file 配置文件路径
     * @param output_config 输出的配置数据
     * @return 是否成功
     */
    bool loadFromYaml(const std::string& config_file, PlannerConfigData& output_config);

    /**
     * @brief 保存配置到YAML文件
     * @param config_file 配置文件路径
     * @param config 配置数据
     * @return 是否成功
     */
    bool saveToYaml(const std::string& config_file, const PlannerConfigData& config);

    /**
     * @brief 从map更新配置参数
     * @param parameters 参数键值对
     * @param config 要更新的配置
     */
    void updateFromMap(const std::map<std::string, double>& parameters,
                      PlannerConfigData& config);

    /**
     * @brief 获取指定模式的默认配置
     * @param mode 规划器模式
     * @return 默认配置
     */
    PlannerConfigData getDefaultConfig(PlannerMode mode);

    /**
     * @brief 注册配置变更回调
     * @param callback 回调函数
     * @return 回调ID
     */
    size_t registerCallback(ConfigChangeCallback callback);

    /**
     * @brief 注销配置变更回调
     * @param callback_id 回调ID
     */
    void unregisterCallback(size_t callback_id);

    /**
     * @brief 触发配置变更回调
     * @param config 新的配置
     */
    void notifyCallbacks(const PlannerConfigData& config);

    /**
     * @brief 获取当前配置
     */
    const PlannerConfigData& getCurrentConfig() const { return current_config_; }

    /**
     * @brief 设置当前配置
     */
    void setCurrentConfig(const PlannerConfigData& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_config_ = config;
        notifyCallbacks(config);
    }

    /**
     * @brief 根据模式创建配置文件路径
     * @param mode 规划器模式
     * @param base_path 基础路径
     * @return 配置文件路径
     */
    static std::string getConfigPath(PlannerMode mode, const std::string& base_path);

    /**
     * @brief 根据模式创建模型文件路径
     * @param mode 规划器模式
     * @param base_path 基础路径
     * @return 模型文件路径
     */
    static std::string getModelPath(PlannerMode mode, const std::string& base_path);

private:
    PlannerConfigManager() = default;
    ~PlannerConfigManager() = default;

#ifdef HAVE_YAMLCPP
    /**
     * @brief 解析Auto模式PPO配置
     */
    void parseAutoPPOConfig(const YAML::Node& node, AutoPPOConfig& config);

    /**
     * @brief 解析Humanoid模式PPO配置
     */
    void parseHumanoidPPOConfig(const YAML::Node& node, HumanoidPPOConfig& config);

    /**
     * @brief 解析奖励配置
     */
    void parseRewardConfig(const YAML::Node& node, HumanoidRewardConfig& config);

    /**
     * @brief 解析数据价值配置
     */
    void parseValueConfig(const YAML::Node& node, DataValueConfig& config);

    /**
     * @brief 解析成本地图配置
     */
    void parseCostMapConfig(const YAML::Node& node, PlannerConfigData::CostMapConfig& config);

    /**
     * @brief 解析通用规划器参数
     */
    void parsePlannerParameters(const YAML::Node& node, std::map<std::string, double>& params);
#endif

    mutable std::mutex mutex_;
    PlannerConfigData current_config_;
    std::vector<ConfigChangeCallback> callbacks_;
    size_t next_callback_id_ = 0;
};

} // namespace aurora::planner

#endif // PLANNER_CONFIG_MANAGER_H
