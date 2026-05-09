// planner_factory.cpp
#include "planner_factory.h"
#include "auto_planner.h"
#include "humanoid_planner.h"
#include "common/log/logger.h"
#include <cstdlib>
#include <algorithm>

namespace aurora::planner {

// ===== PlannerCreateConfig =====

PlannerCreateConfig PlannerCreateConfig::fromConfigData(const PlannerConfigData& config_data) {
    PlannerCreateConfig config;
    config.mode = config_data.mode;
    config.model_path = config_data.model_path;
    config.config_path = config_data.config_file;
    return config;
}

// ===== PlannerFactory =====

PlannerFactory& PlannerFactory::getInstance() {
    static PlannerFactory instance;
    return instance;
}

PlannerFactory::PlannerFactory() {
    // 注册默认创建函数
    creators_[PlannerMode::AUTO] = [this](const PlannerCreateConfig& config) {
        return createAutoPlanner(config);
    };
    creators_[PlannerMode::HUMANOID] = [this](const PlannerCreateConfig& config) {
        return createHumanoidPlanner(config);
    };

    // 设置默认路径前缀
    const char* model_path = std::getenv("AER_MODEL_PATH");
    if (model_path) {
        model_path_prefix_ = model_path;
        // 移除文件名，只保留目录
        size_t pos = model_path_prefix_.find_last_of('/');
        if (pos != std::string::npos) {
            model_path_prefix_ = model_path_prefix_.substr(0, pos);
        }
    }

    const char* config_path = std::getenv("AER_CONFIG_PATH");
    if (config_path) {
        config_path_prefix_ = config_path;
        // 移除文件名，只保留目录
        size_t pos = config_path_prefix_.find_last_of('/');
        if (pos != std::string::npos) {
            config_path_prefix_ = config_path_prefix_.substr(0, pos);
        }
    }

    AD_INFO("PlannerFactory", "Factory initialized (model_prefix: %s, config_prefix: %s)",
            model_path_prefix_.c_str(), config_path_prefix_.c_str());
}

std::unique_ptr<IPlanner> PlannerFactory::create(const PlannerCreateConfig& config) {
    auto it = creators_.find(config.mode);
    if (it != creators_.end() && it->second) {
        return it->second(config);
    }

    AD_ERROR("PlannerFactory", "Unsupported planner mode: %d",
              static_cast<int>(config.mode));
    return nullptr;
}

std::unique_ptr<IPlanner> PlannerFactory::createFromConfigFile(const std::string& config_path) {
    PlannerConfigData config_data;
    auto& manager = PlannerConfigManager::getInstance();

    if (!manager.loadFromYaml(config_path, config_data)) {
        AD_WARN("PlannerFactory", "Failed to load config from %s, using defaults",
                config_path.c_str());
        // 使用默认配置
        config_data = manager.getDefaultConfig(PlannerMode::HUMANOID);
    }

    config_data.config_file = config_path;
    return create(PlannerCreateConfig::fromConfigData(config_data));
}

std::unique_ptr<IPlanner> PlannerFactory::createFromMode(PlannerMode mode) {
    PlannerCreateConfig config;
    config.mode = mode;

    // 设置默认路径
    config.config_path = PlannerConfigManager::getConfigPath(mode, config_path_prefix_);
    config.model_path = PlannerConfigManager::getModelPath(mode, model_path_prefix_);

    return create(config);
}

std::unique_ptr<IPlanner> PlannerFactory::createFromEnv() {
    PlannerMode mode = getPlannerModeFromEnv();
    return createFromMode(mode);
}

std::unique_ptr<IPlanner> PlannerFactory::createAutoPlanner(const PlannerCreateConfig& config) {
    std::string model_path = config.model_path;
    std::string config_path = config.config_path;

    if (model_path.empty()) {
        model_path = PlannerConfigManager::getModelPath(PlannerMode::AUTO, model_path_prefix_);
    }
    if (config_path.empty()) {
        config_path = PlannerConfigManager::getConfigPath(PlannerMode::AUTO, config_path_prefix_);
    }

    auto planner = std::make_unique<AutoPlanner>(model_path, config_path);
    AD_INFO("PlannerFactory", "Created %s planner", plannerModeToString(PlannerMode::AUTO).c_str());
    return planner;
}

std::unique_ptr<IPlanner> PlannerFactory::createHumanoidPlanner(const PlannerCreateConfig& config) {
    std::string model_path = config.model_path;
    std::string config_path = config.config_path;

    if (model_path.empty()) {
        model_path = PlannerConfigManager::getModelPath(PlannerMode::HUMANOID, model_path_prefix_);
    }
    if (config_path.empty()) {
        config_path = PlannerConfigManager::getConfigPath(PlannerMode::HUMANOID, config_path_prefix_);
    }

    HumanoidPlannerConfig planner_config;

    auto planner = std::make_unique<HumanoidPlanner>(model_path, config_path, planner_config);
    AD_INFO("PlannerFactory", "Created %s planner", plannerModeToString(PlannerMode::HUMANOID).c_str());
    return planner;
}

void PlannerFactory::registerCreator(PlannerMode mode, CreatorFunc creator) {
    creators_[mode] = std::move(creator);
    AD_INFO("PlannerFactory", "Registered custom creator for mode: %s",
            plannerModeToString(mode).c_str());
}

bool PlannerFactory::isModeSupported(PlannerMode mode) const {
    return creators_.find(mode) != creators_.end();
}

std::vector<PlannerMode> PlannerFactory::getSupportedModes() const {
    std::vector<PlannerMode> modes;
    for (const auto& kv : creators_) {
        modes.push_back(kv.first);
    }
    return modes;
}

// ===== 辅助函数 =====

PlannerMode getPlannerModeFromEnv(PlannerMode default_mode) {
    const char* mode_str = std::getenv("AER_MODE");
    if (mode_str) {
        std::string mode(mode_str);
        std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

        if (mode == "auto") {
            return PlannerMode::AUTO;
        } else if (mode == "humanoid") {
            return PlannerMode::HUMANOID;
        }
    }

    return default_mode;
}

std::string plannerModeToString(PlannerMode mode) {
    switch (mode) {
        case PlannerMode::AUTO: return "auto";
        case PlannerMode::HUMANOID: return "humanoid";
        default: return "unknown";
    }
}

PlannerMode stringToPlannerMode(const std::string& mode_str) {
    std::string mode = mode_str;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

    if (mode == "auto") {
        return PlannerMode::AUTO;
    } else if (mode == "humanoid") {
        return PlannerMode::HUMANOID;
    }

    return PlannerMode::HUMANOID;  // 默认
}

} // namespace aurora::planner
