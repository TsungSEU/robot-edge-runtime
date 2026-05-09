// humanoid_data_value.cpp
#include "humanoid_data_value.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace aurora::planner {

// ===== HumanoidDataValueModel 实现 =====

HumanoidDataValueModel::HumanoidDataValueModel(const DataValueConfig& config,
                                              double grid_resolution)
    : config_(config), grid_resolution_(grid_resolution)
    , oldest_data_time_(std::numeric_limits<double>::max())
    , newest_data_time_(0) {}

void HumanoidDataValueModel::addDataPoint(const DataPointMetadata& metadata) {
    // 更新时间统计
    if (metadata.timestamp < oldest_data_time_) {
        oldest_data_time_ = metadata.timestamp;
    }
    if (metadata.timestamp > newest_data_time_) {
        newest_data_time_ = metadata.timestamp;
    }

    // 更新场景统计
    std::string scene_key = getSceneTypeName(metadata.scene_type);
    scene_type_counts_[scene_key]++;
    scene_data_points_[metadata.scene_type].push_back(metadata);

    // 更新天气统计
    std::string weather_key = getWeatherName(metadata.weather);
    weather_counts_[weather_key]++;

    // 更新空间索引
    int grid_x = static_cast<int>(metadata.x / grid_resolution_);
    int grid_y = static_cast<int>(metadata.y / grid_resolution_);
    auto key = std::make_pair(grid_x, grid_y);

    auto& index = spatial_index_[key];
    index.grid_x = grid_x;
    index.grid_y = grid_y;
    index.x = grid_x * grid_resolution_;
    index.y = grid_y * grid_resolution_;
    index.points.push_back(const_cast<DataPointMetadata*>(&metadata));
}

void HumanoidDataValueModel::addDataPoints(const std::vector<DataPointMetadata>& metadata_list) {
    for (const auto& metadata : metadata_list) {
        addDataPoint(metadata);
    }
}

DataValueResult HumanoidDataValueModel::evaluateDataPoint(const DataPointMetadata& metadata) const {
    DataValueResult result;

    // 计算各维度价值
    result.spatial_rarity = computeSpatialRarity(metadata.x, metadata.y);
    result.temporal_freshness = computeTemporalFreshness(metadata.timestamp, metadata.timestamp);
    result.scene_diversity = computeSceneDiversity(metadata.scene_type, metadata.weather);
    result.data_quality = computeDataQuality(metadata);

    // 加权组合
    result.total_value = config_.w_spatial_rarity * result.spatial_rarity +
                       config_.w_temporal_freshness * result.temporal_freshness +
                       config_.w_scene_diversity * result.scene_diversity +
                       config_.w_quality * result.data_quality +
                       config_.w_coverage * (1.0 - result.spatial_rarity);  // 覆盖率与稀缺性互补

    result.normalized_value = std::max(0.0, std::min(1.0, result.total_value));

    // 生成评估理由
    std::stringstream ss;
    ss << "Spatial: " << std::fixed << std::setprecision(2) << result.spatial_rarity
       << ", Scene: " << result.scene_diversity
       << ", Quality: " << result.data_quality;
    result.reasoning = ss.str();

    // 添加标签
    if (result.spatial_rarity > 0.7) {
        result.tags.push_back("rare_location");
    }
    if (result.scene_diversity > 0.7) {
        result.tags.push_back("diverse_scene");
    }
    if (result.data_quality > 0.8) {
        result.tags.push_back("high_quality");
    }

    return result;
}

DataValueResult HumanoidDataValueModel::evaluateLocationValue(double x, double y,
                                                            SceneType scene_type,
                                                            double current_time) const {
    DataPointMetadata dummy_metadata;
    dummy_metadata.x = x;
    dummy_metadata.y = y;
    dummy_metadata.scene_type = scene_type;
    dummy_metadata.timestamp = current_time;
    dummy_metadata.snr = 20;
    dummy_metadata.completeness = 1.0;
    dummy_metadata.sensor_quality = 1.0;

    return evaluateDataPoint(dummy_metadata);
}

double HumanoidDataValueModel::getLocalDensity(double x, double y, double radius) const {
    int grid_x = static_cast<int>(x / grid_resolution_);
    int grid_y = static_cast<int>(y / grid_resolution_);

    int grid_radius = static_cast<int>(std::ceil(radius / grid_resolution_));
    int count = 0;
    int total_cells = 0;

    for (int dx = -grid_radius; dx <= grid_radius; ++dx) {
        for (int dy = -grid_radius; dy <= grid_radius; ++dy) {
            auto key = std::make_pair(grid_x + dx, grid_y + dy);
            auto it = spatial_index_.find(key);
            if (it != spatial_index_.end()) {
                count += it->second.points.size();
            }
            total_cells++;
        }
    }

    return total_cells > 0 ? static_cast<double>(count) / total_cells : 0.0;
}

void HumanoidDataValueModel::updateStatistics() {
    // 更新场景稀有度映射
    int total = getTotalDataPoints();
    if (total == 0) return;

    for (auto& [scene_name, count] : scene_type_counts_) {
        double rarity = 1.0 - (static_cast<double>(count) / total);
        // 可以在这里更新config_中的scene_rarity_map
    }
}

void HumanoidDataValueModel::clearOldData(double max_age) {
    double current_time = newest_data_time_;
    double cutoff_time = current_time - max_age;

    // 清理空间索引中的旧数据
    for (auto& [key, index] : spatial_index_) {
        auto it = std::remove_if(index.points.begin(), index.points.end(),
            [cutoff_time](DataPointMetadata* ptr) {
                return ptr->timestamp < cutoff_time;
            });
        index.points.erase(it, index.points.end());
    }

    // 清理场景数据
    for (auto& [scene_type, points] : scene_data_points_) {
        auto it = std::remove_if(points.begin(), points.end(),
            [cutoff_time](const DataPointMetadata& m) {
                return m.timestamp < cutoff_time;
            });
        points.erase(it, points.end());
    }
}

void HumanoidDataValueModel::reset() {
    scene_type_counts_.clear();
    weather_counts_.clear();
    scene_data_points_.clear();
    spatial_index_.clear();
    oldest_data_time_ = std::numeric_limits<double>::max();
    newest_data_time_ = 0;
}

// ===== 各维度价值计算 =====

double HumanoidDataValueModel::computeSpatialRarity(double x, double y, double radius) const {
    double density = getLocalDensity(x, y, radius);

    // 稀缺性 = 1 - 密度
    // 使用指数衰减使稀疏区域的值更高
    double rarity = std::exp(-2.0 * density);
    return std::max(0.0, std::min(1.0, rarity));
}

double HumanoidDataValueModel::computeTemporalFreshness(double timestamp, double current_time) const {
    if (newest_data_time_ == 0) return 1.0;

    double age = current_time - timestamp;

    // 时间衰减
    double freshness = std::exp(-config_.temporal_decay_rate * age);

    // 如果是最近的数据，给予额外奖励
    if (age < config_.recency_bonus_duration) {
        freshness *= 1.2;
    }

    return std::max(0.0, std::min(1.0, freshness));
}

double HumanoidDataValueModel::computeSceneDiversity(SceneType scene_type,
                                                    WeatherCondition weather) const {
    int total = getTotalDataPoints();
    if (total == 0) return 0.5;

    // 获取该场景的数量
    auto scene_it = scene_data_points_.find(scene_type);
    int scene_count = (scene_it != scene_data_points_.end()) ?
                      static_cast<int>(scene_it->second.size()) : 0;

    // 基础多样性 = 1 - 该场景占比
    double base_diversity = 1.0 - (static_cast<double>(scene_count) / total);

    // 应用场景稀有度倍数
    auto rarity_it = config_.scene_rarity_map.find(scene_type);
    if (rarity_it != config_.scene_rarity_map.end()) {
        base_diversity *= (1.0 + rarity_it->second);
    }

    // 天气加成
    auto weather_it = weather_counts_.find(getWeatherName(weather));
    if (weather_it != weather_counts_.end()) {
        double weather_ratio = static_cast<double>(weather_it->second) / total;
        base_diversity *= (1.0 + (1.0 - weather_ratio));
    }

    return std::max(0.0, std::min(1.0, base_diversity));
}

double HumanoidDataValueModel::computeDataQuality(const DataPointMetadata& metadata) const {
    double quality = 0.0;

    // 信噪比贡献
    double snr_score = std::min(1.0, metadata.snr / 30.0);  // 30dB为满分
    quality += 0.3 * snr_score;

    // 完整性贡献
    quality += 0.3 * metadata.completeness;

    // 传感器质量贡献
    quality += 0.2 * metadata.sensor_quality;

    // 标注置信度贡献
    quality += 0.2 * metadata.annotation_confidence;

    return std::max(0.0, std::min(1.0, quality));
}

// ===== 场景识别辅助 =====

SceneType HumanoidDataValueModel::inferSceneType(double terrain_height,
                                               double terrain_slope,
                                               double terrain_roughness,
                                               bool is_indoor) {
    if (is_indoor) {
        if (std::abs(terrain_slope) > 0.1) {
            if (terrain_height > 0.5) {
                return SceneType::INDOOR_STAIR;
            } else {
                return SceneType::INDOOR_RAMP;
            }
        }
        return SceneType::INDOOR_FLAT;
    } else {
        if (terrain_roughness > 0.3) {
            return SceneType::OUTDOOR_ROUGH;
        }
        if (std::abs(terrain_slope) > 0.1) {
            return SceneType::OUTDOOR_SLOPE;
        }
        return SceneType::OUTDOOR_FLAT;
    }
}

std::string HumanoidDataValueModel::getSceneTypeName(SceneType type) {
    switch (type) {
        case SceneType::INDOOR_FLAT: return "indoor_flat";
        case SceneType::INDOOR_STAIR: return "indoor_stair";
        case SceneType::INDOOR_RAMP: return "indoor_ramp";
        case SceneType::OUTDOOR_FLAT: return "outdoor_flat";
        case SceneType::OUTDOOR_ROUGH: return "outdoor_rough";
        case SceneType::OUTDOOR_SLOPE: return "outdoor_slope";
        case SceneType::MIXED: return "mixed";
        case SceneType::UNKNOWN: return "unknown";
        default: return "unknown";
    }
}

std::string HumanoidDataValueModel::getWeatherName(WeatherCondition weather) {
    switch (weather) {
        case WeatherCondition::CLEAR: return "clear";
        case WeatherCondition::CLOUDY: return "cloudy";
        case WeatherCondition::RAIN: return "rain";
        case WeatherCondition::SNOW: return "snow";
        case WeatherCondition::FOG: return "fog";
        case WeatherCondition::NIGHT: return "night";
        case WeatherCondition::LOW_LIGHT: return "low_light";
        case WeatherCondition::UNKNOWN: return "unknown";
        default: return "unknown";
    }
}

int HumanoidDataValueModel::getTotalDataPoints() const {
    int total = 0;
    for (const auto& [name, count] : scene_type_counts_) {
        total += count;
    }
    return total;
}

int HumanoidDataValueModel::getSceneTypeCount(SceneType type) const {
    std::string name = getSceneTypeName(type);
    auto it = scene_type_counts_.find(name);
    return (it != scene_type_counts_.end()) ? it->second : 0;
}

std::vector<SceneType> HumanoidDataValueModel::getRareScenes(double threshold) const {
    std::vector<SceneType> rare_scenes;
    int total = getTotalDataPoints();

    if (total == 0) return rare_scenes;

    for (const auto& [scene_type, points] : scene_data_points_) {
        double ratio = static_cast<double>(points.size()) / total;
        if (ratio < threshold) {
            rare_scenes.push_back(scene_type);
        }
    }

    return rare_scenes;
}

std::vector<std::vector<double>> HumanoidDataValueModel::getValueHeatmap(
    double x_min, double y_min,
    double x_max, double y_max,
    double resolution) const {
    int nx = static_cast<int>(std::ceil((x_max - x_min) / resolution));
    int ny = static_cast<int>(std::ceil((y_max - y_min) / resolution));

    std::vector<std::vector<double>> heatmap(ny, std::vector<double>(nx, 0.0));

    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            double x = x_min + i * resolution;
            double y = y_min + j * resolution;
            heatmap[j][i] = computeSpatialRarity(x, y);
        }
    }

    return heatmap;
}

// ===== AdaptiveValueModel 实现 =====

void AdaptiveValueModel::adjustWeightsByProgress(double current_coverage) {
    if (initial_coverage_ratio_ == 0) {
        initial_coverage_ratio_ = current_coverage;
    }

    double progress = (current_coverage - initial_coverage_ratio_) /
                     (1.0 - initial_coverage_ratio_);
    progress = std::max(0.0, std::min(1.0, progress));

    auto config = getConfig();

    // 早期阶段：优先探索，提高空间稀缺性权重
    if (progress < 0.3) {
        config.w_spatial_rarity = 0.4;
        config.w_scene_diversity = 0.3;
        config.w_quality = 0.1;
    }
    // 中期阶段：平衡探索和质量
    else if (progress < 0.7) {
        config.w_spatial_rarity = 0.3;
        config.w_scene_diversity = 0.25;
        config.w_quality = 0.2;
    }
    // 后期阶段：优先质量和多样性
    else {
        config.w_spatial_rarity = 0.2;
        config.w_scene_diversity = 0.35;
        config.w_quality = 0.25;
    }

    setConfig(config);
}

void AdaptiveValueModel::setWeightSchedule(const std::map<std::string, double>& schedule) {
    weight_schedule_ = schedule;
}

DataValueConfig AdaptiveValueModel::getAdaptiveConfig() const {
    return getConfig();
}

// ===== DataValueHistory 实现 =====

void DataValueHistory::recordValue(int grid_x, int grid_y, double value,
                                   const std::string& reason) {
    auto key = std::make_pair(grid_x, grid_y);
    auto& history = history_[key];

    ValueRecord record;
    record.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    record.value = value;
    record.reason = reason;

    history.push_back(record);

    // 限制历史长度
    if (history.size() > max_history_per_cell_) {
        history.erase(history.begin());
    }
}

std::vector<DataValueHistory::ValueRecord>
DataValueHistory::getValueHistory(int grid_x, int grid_y) const {
    auto key = std::make_pair(grid_x, grid_y);
    auto it = history_.find(key);
    return (it != history_.end()) ? it->second : std::vector<ValueRecord>();
}

DataValueHistory::Trend DataValueHistory::analyzeTrend(int grid_x, int grid_y,
                                                      double window) const {
    auto history = getValueHistory(grid_x, grid_y);
    if (history.size() < 2) {
        return STABLE;
    }

    // 取最近的window个记录
    size_t start = (history.size() > static_cast<size_t>(window)) ?
                   (history.size() - window) : 0;

    double sum = 0;
    int count = 0;
    for (size_t i = start; i < history.size(); ++i) {
        sum += history[i].value;
        count++;
    }

    if (count < 2) return STABLE;

    double avg = sum / count;
    double recent = history.back().value;

    constexpr double trend_threshold = 0.1;

    if (recent > avg * (1.0 + trend_threshold)) {
        return INCREASING;
    } else if (recent < avg * (1.0 - trend_threshold)) {
        return DECREASING;
    } else {
        return STABLE;
    }
}

void DataValueHistory::cleanup(double max_age) {
    double current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    double cutoff_time = current_time - max_age;

    for (auto& [key, history] : history_) {
        auto it = std::remove_if(history.begin(), history.end(),
            [cutoff_time](const ValueRecord& record) {
                return record.timestamp < cutoff_time;
            });
        history.erase(it, history.end());
    }
}

} // namespace aurora::planner
