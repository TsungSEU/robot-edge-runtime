// humanoid_data_value.h
#ifndef HUMANOID_DATA_VALUE_H
#define HUMANOID_DATA_VALUE_H

#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <functional>

namespace aurora::planner {

/**
 * @brief 场景类型枚举
 */
enum class SceneType : int {
    INDOOR_FLAT = 0,        // 室内平地
    INDOOR_STAIR = 1,       // 室内楼梯
    INDOOR_RAMP = 2,        // 室内坡道
    OUTDOOR_FLAT = 3,       // 室外平地
    OUTDOOR_ROUGH = 4,      // 室外粗糙地形
    OUTDOOR_SLOPE = 5,      // 室外坡地
    MIXED = 6,              // 混合场景
    UNKNOWN = 7             // 未知场景
};

/**
 * @brief 天气条件
 */
enum class WeatherCondition : int {
    CLEAR = 0,              // 晴朗
    CLOUDY = 1,            // 多云
    RAIN = 2,              // 下雨
    SNOW = 3,              // 下雪
    FOG = 4,               // 有雾
    NIGHT = 5,             // 夜间
    LOW_LIGHT = 6,         // 低光
    UNKNOWN = 7            // 未知
};

/**
 * @brief 数据点元数据
 */
struct DataPointMetadata {
    // 空间信息
    double x, y, z;                     // 3D坐标
    double resolution;                  // 数据分辨率

    // 时间信息
    double timestamp;                   // 时间戳
    double collection_duration;         // 采集时长

    // 场景信息
    SceneType scene_type;               // 场景类型
    WeatherCondition weather;           // 天气条件
    double lighting_level;              // 光照等级 [0,1]
    double visibility_score;            // 能见度评分 [0,1]

    // 数据质量信息
    double snr;                         // 信噪比
    double completeness;                // 完整性 [0,1]
    double annotation_confidence;       // 标注置信度 [0,1]
    std::vector<std::string> tags;      // 数据标签

    // 传感器信息
    std::string sensor_type;            // 传感器类型
    double sensor_quality;              // 传感器质量评分 [0,1]
    std::map<std::string, double> sensor_metrics;  // 传感器指标

    // 机器人状态
    double gait_phase;                  // 采集时步态相位
    double stability_score;             // 稳定性评分
    double energy_level;                // 能量水平

    // 统计信息
    int visit_count;                    // 该位置访问次数
    double last_visit_time;             // 上次访问时间

    DataPointMetadata()
        : x(0), y(0), z(0), resolution(0.1)
        , timestamp(0), collection_duration(0)
        , scene_type(SceneType::UNKNOWN)
        , weather(WeatherCondition::UNKNOWN)
        , lighting_level(1), visibility_score(1)
        , snr(20), completeness(1), annotation_confidence(0.8)
        , sensor_quality(1), gait_phase(0), stability_score(1)
        , energy_level(1), visit_count(1), last_visit_time(0) {}
};

/**
 * @brief 数据价值评估配置
 */
struct DataValueConfig {
    // 空间维度权重
    double w_spatial_rarity = 0.3;      // 空间稀缺性权重
    double w_coverage = 0.2;            // 覆盖率权重

    // 时间维度权重
    double w_temporal_freshness = 0.15; // 时间新鲜度权重
    double temporal_decay_rate = 0.001; // 时间衰减率 (每秒)
    double recency_bonus_duration = 3600;  // 最近数据奖励时长 (秒)

    // 场景维度权重
    double w_scene_diversity = 0.2;     // 场景多样性权重
    double rare_scene_bonus = 2.0;      // 稀有场景奖励倍数

    // 质量维度权重
    double w_quality = 0.15;            // 数据质量权重
    double min_quality_threshold = 0.5; // 最低质量阈值

    // 稀有场景定义
    std::map<SceneType, double> scene_rarity_map;  // 场景稀有度映射

    DataValueConfig() {
        // 初始化场景稀有度 (越低越稀有)
        scene_rarity_map[SceneType::INDOOR_FLAT] = 0.3;
        scene_rarity_map[SceneType::INDOOR_STAIR] = 0.7;
        scene_rarity_map[SceneType::INDOOR_RAMP] = 0.6;
        scene_rarity_map[SceneType::OUTDOOR_FLAT] = 0.4;
        scene_rarity_map[SceneType::OUTDOOR_ROUGH] = 0.8;
        scene_rarity_map[SceneType::OUTDOOR_SLOPE] = 0.7;
        scene_rarity_map[SceneType::MIXED] = 0.9;
        scene_rarity_map[SceneType::UNKNOWN] = 0.5;
    }
};

/**
 * @brief 多维度数据价值评估结果
 */
struct DataValueResult {
    // 各维度评分
    double spatial_rarity;              // 空间稀缺性 [0,1]
    double temporal_freshness;          // 时间新鲜度 [0,1]
    double scene_diversity;             // 场景多样性 [0,1]
    double data_quality;                // 数据质量 [0,1]

    // 综合价值
    double total_value;                 // 综合价值评分 [0,1]
    double normalized_value;            // 归一化价值 [0,1]

    // 辅助信息
    std::string reasoning;              // 评估理由
    std::vector<std::string> tags;      // 价值标签

    DataValueResult()
        : spatial_rarity(0), temporal_freshness(0), scene_diversity(0)
        , data_quality(0), total_value(0), normalized_value(0) {}
};

/**
 * @brief 人形机器人数据价值模型
 *
 * 综合评估数据的多维度价值，为RL提供更精细的奖励信号
 */
class HumanoidDataValueModel {
private:
    DataValueConfig config_;

    // 统计信息
    std::map<std::string, int> scene_type_counts_;
    std::map<std::string, int> weather_counts_;
    std::map<SceneType, std::vector<DataPointMetadata>> scene_data_points_;

    // 空间索引 (用于快速查询)
    struct SpatialIndex {
        int grid_x, grid_y;
        double x, y;
        std::vector<DataPointMetadata*> points;
    };
    std::map<std::pair<int, int>, SpatialIndex> spatial_index_;
    double grid_resolution_;

    // 时间统计
    double oldest_data_time_;
    double newest_data_time_;

public:
    HumanoidDataValueModel(const DataValueConfig& config = DataValueConfig(),
                          double grid_resolution = 0.5);

    /**
     * @brief 添加数据点记录
     */
    void addDataPoint(const DataPointMetadata& metadata);

    /**
     * @brief 批量添加数据点
     */
    void addDataPoints(const std::vector<DataPointMetadata>& metadata_list);

    /**
     * @brief 评估单个数据点的价值
     */
    DataValueResult evaluateDataPoint(const DataPointMetadata& metadata) const;

    /**
     * @brief 评估位置的数据价值
     */
    DataValueResult evaluateLocationValue(double x, double y,
                                         SceneType scene_type = SceneType::UNKNOWN,
                                         double current_time = 0) const;

    /**
     * @brief 获取区域数据密度
     */
    double getLocalDensity(double x, double y, double radius = 1.0) const;

    /**
     * @brief 更新统计信息
     */
    void updateStatistics();

    /**
     * @brief 清除旧数据
     */
    void clearOldData(double max_age = 86400);  // 默认24小时

    /**
     * @brief 重置模型
     */
    void reset();

    // ===== 各维度价值计算 =====

    /**
     * @brief 计算空间稀缺性
     */
    double computeSpatialRarity(double x, double y, double radius = 1.0) const;

    /**
     * @brief 计算时间新鲜度
     */
    double computeTemporalFreshness(double timestamp, double current_time) const;

    /**
     * @brief 计算场景多样性
     */
    double computeSceneDiversity(SceneType scene_type,
                                WeatherCondition weather) const;

    /**
     * @brief 计算数据质量
     */
    double computeDataQuality(const DataPointMetadata& metadata) const;

    // ===== 场景识别辅助 =====

    /**
     * @brief 从环境特征推断场景类型
     */
    static SceneType inferSceneType(double terrain_height,
                                   double terrain_slope,
                                   double terrain_roughness,
                                   bool is_indoor);

    /**
     * @brief 获取场景名称
     */
    static std::string getSceneTypeName(SceneType type);

    /**
     * @brief 获取天气名称
     */
    static std::string getWeatherName(WeatherCondition weather);

    // ===== 配置管理 =====

    void setConfig(const DataValueConfig& config) { config_ = config; }
    const DataValueConfig& getConfig() const { return config_; }

    // ===== 统计查询 =====

    int getTotalDataPoints() const;
    int getSceneTypeCount(SceneType type) const;
    std::vector<SceneType> getRareScenes(double threshold = 0.5) const;

    /**
     * @brief 获取价值热力图
     */
    std::vector<std::vector<double>> getValueHeatmap(
        double x_min, double y_min,
        double x_max, double y_max,
        double resolution = 0.5) const;
};

/**
 * @brief 自适应价值模型
 *
 * 根据采集进度自动调整各维度的权重
 */
class AdaptiveValueModel : public HumanoidDataValueModel {
private:
    double initial_coverage_ratio_;
    std::map<std::string, double> weight_schedule_;

public:
    AdaptiveValueModel(const DataValueConfig& config = DataValueConfig(),
                      double grid_resolution = 0.5)
        : HumanoidDataValueModel(config, grid_resolution)
        , initial_coverage_ratio_(0) {}

    /**
     * @brief 根据当前覆盖率调整权重
     */
    void adjustWeightsByProgress(double current_coverage);

    /**
     * @brief 设置权重调度
     */
    void setWeightSchedule(const std::map<std::string, double>& schedule);

    /**
     * @brief 获取当前权重配置
     */
    DataValueConfig getAdaptiveConfig() const;
};

/**
 * @brief 数据价值历史记录
 *
 * 用于跟踪价值变化趋势
 */
class DataValueHistory {
private:
    struct ValueRecord {
        double timestamp;
        double value;
        std::string reason;
    };

    std::map<std::pair<int, int>, std::vector<ValueRecord>> history_;
    size_t max_history_per_cell_;

public:
    DataValueHistory(size_t max_history = 100)
        : max_history_per_cell_(max_history) {}

    /**
     * @brief 记录价值变化
     */
    void recordValue(int grid_x, int grid_y, double value,
                    const std::string& reason = "");

    /**
     * @brief 获取价值历史
     */
    std::vector<ValueRecord> getValueHistory(int grid_x, int grid_y) const;

    /**
     * @brief 分析价值趋势
     */
    enum Trend { INCREASING, DECREASING, STABLE };
    Trend analyzeTrend(int grid_x, int grid_y, double window = 10) const;

    /**
     * @brief 清理旧记录
     */
    void cleanup(double max_age = 86400);
};

} // namespace aurora::planner

#endif // HUMANOID_DATA_VALUE_H
