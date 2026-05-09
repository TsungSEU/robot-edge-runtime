// planner_factory.h
#ifndef PLANNER_FACTORY_H
#define PLANNER_FACTORY_H

#include <memory>
#include <string>
#include <map>
#include <functional>

#include "i_planner.h"
#include "../config/planner_config_manager.h"

// Forward declarations
namespace aurora::planner {
    class AutoPlanner;
    class HumanoidPlanner;
}

namespace aurora::planner {

/**
 * @brief 规划器创建配置
 */
struct PlannerCreateConfig {
    PlannerMode mode = PlannerMode::HUMANOID;
    std::string model_path;
    std::string config_path;
    bool enable_statistics = true;
    bool enable_hot_reload = true;

    PlannerCreateConfig() = default;

    /**
     * @brief 从配置数据创建
     */
    static PlannerCreateConfig fromConfigData(const PlannerConfigData& config_data);
};

/**
 * @brief 规划器工厂 - 统一创建接口
 *
 * 根据配置创建相应类型的规划器实例
 */
class PlannerFactory {
public:
    /**
     * @brief 规划器创建函数类型
     */
    using CreatorFunc = std::function<std::unique_ptr<IPlanner>(const PlannerCreateConfig&)>;

    /**
     * @brief 获取单例实例
     */
    static PlannerFactory& getInstance();

    // 禁止拷贝和移动
    PlannerFactory(const PlannerFactory&) = delete;
    PlannerFactory& operator=(const PlannerFactory&) = delete;
    PlannerFactory(PlannerFactory&&) = delete;
    PlannerFactory& operator=(PlannerFactory&&) = delete;

    /**
     * @brief 创建规划器
     * @param config 创建配置
     * @return 规划器实例
     */
    std::unique_ptr<IPlanner> create(const PlannerCreateConfig& config);

    /**
     * @brief 从配置文件创建规划器
     * @param config_path 配置文件路径
     * @return 规划器实例
     */
    std::unique_ptr<IPlanner> createFromConfigFile(const std::string& config_path);

    /**
     * @brief 从模式创建规划器（使用默认路径）
     * @param mode 规划器模式
     * @return 规划器实例
     */
    std::unique_ptr<IPlanner> createFromMode(PlannerMode mode);

    /**
     * @brief 从环境变量DCP_MODE创建规划器
     * @return 规划器实例
     */
    std::unique_ptr<IPlanner> createFromEnv();

    /**
     * @brief 注册自定义规划器创建函数
     * @param mode 规划器模式
     * @param creator 创建函数
     */
    void registerCreator(PlannerMode mode, CreatorFunc creator);

    /**
     * @brief 检查模式是否支持
     * @param mode 规划器模式
     * @return 是否支持
     */
    bool isModeSupported(PlannerMode mode) const;

    /**
     * @brief 获取支持的模式列表
     * @return 模式列表
     */
    std::vector<PlannerMode> getSupportedModes() const;

    /**
     * @brief 设置默认模型路径前缀
     * @param prefix 路径前缀
     */
    void setModelPathPrefix(const std::string& prefix) { model_path_prefix_ = prefix; }

    /**
     * @brief 设置默认配置路径前缀
     * @param prefix 路径前缀
     */
    void setConfigPathPrefix(const std::string& prefix) { config_path_prefix_ = prefix; }

private:
    PlannerFactory();
    ~PlannerFactory() = default;

    /**
     * @brief 创建Auto模式规划器
     */
    std::unique_ptr<IPlanner> createAutoPlanner(const PlannerCreateConfig& config);

    /**
     * @brief 创建Humanoid模式规划器
     */
    std::unique_ptr<IPlanner> createHumanoidPlanner(const PlannerCreateConfig& config);

    std::string model_path_prefix_;
    std::string config_path_prefix_;
    std::map<PlannerMode, CreatorFunc> creators_;
};

/**
 * @brief 辅助函数：从环境变量获取模式
 * @param default_mode 默认模式
 * @return 规划器模式
 */
PlannerMode getPlannerModeFromEnv(PlannerMode default_mode = PlannerMode::HUMANOID);

/**
 * @brief 辅助函数：模式转换为字符串
 */
std::string plannerModeToString(PlannerMode mode);

/**
 * @brief 辅助函数：字符串转换为模式
 */
PlannerMode stringToPlannerMode(const std::string& mode_str);

} // namespace aurora::planner

#endif // PLANNER_FACTORY_H
