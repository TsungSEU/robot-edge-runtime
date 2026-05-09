# 核心数据结构

## 概述

本文档描述 Aurora-Edge-Runtime 系统中的核心数据结构，包括空间数据、采集数据、规划数据和元数据等。

## 空间数据结构

### Point

表示二维空间中的点：

```cpp
struct Point {
    double x;  // X 坐标 (米)
    double y;  // Y 坐标 (米)

    Point(double x = 0, double y = 0) : x(x), y(y) {}

    // 距离计算
    double distanceTo(const Point& other) const {
        return std::hypot(x - other.x, y - other.y);
    }

    // 角度计算
    double angleTo(const Point& other) const {
        return std::atan2(other.y - y, other.x - x);
    }
};
```

### MissionArea

定义任务区域边界：

```cpp
struct MissionArea {
    Point center;     // 区域中心点
    double radius;    // 区域半径 (米)

    MissionArea(const Point& c = Point(), double r = 0.0)
        : center(c), radius(r) {}

    // 判断点是否在区域内
    bool contains(const Point& p) const {
        return center.distanceTo(p) <= radius;
    }

    // 获取区域边界
    Point getBoundaryPoint(double angle) const {
        return Point(
            center.x + radius * std::cos(angle),
            center.y + radius * std::sin(angle)
        );
    }
};
```

### Path

路径由一系列点组成：

```cpp
using Path = std::vector<Point>;

struct PathWithMeta {
    Path points;           // 路径点序列
    double total_length;   // 总长度
    double estimated_time; // 预估时间 (秒)
    std::string scene_type; // 场景类型

    double getLength() const {
        if (points.size() < 2) return 0;
        double len = 0;
        for (size_t i = 1; i < points.size(); ++i) {
            len += points[i-1].distanceTo(points[i]);
        }
        return len;
    }
};
```

## 采集数据结构

### DataPoint

单次数据采集的元数据：

```cpp
struct DataPoint {
    Point position;           // 采集位置
    std::string sensor_data;  // 传感器数据引用
    double timestamp;         // 时间戳
    std::string trigger_id;   // 触发器 ID
    std::string bag_path;     // bag 文件路径

    DataPoint(const Point& pos = Point(),
              const std::string& data = "",
              double time = 0.0)
        : position(pos), sensor_data(data), timestamp(time) {}
};
```

### CollectionResult

采集执行结果：

```cpp
struct CollectionResult {
    std::vector<DataPoint> collected_data;  // 采集的数据点
    std::vector<Point> planned_path;        // 规划路径
    std::vector<Point> actual_path;         // 实际路径
    double execution_time;                  // 执行时长
    double total_distance;                  // 行驶距离
    int trigger_count;                      // 触发次数
    bool success;                           // 是否成功
    std::string error_message;              // 错误信息
};
```

## 规划数据结构

### StateInfo

规划器状态信息：

```cpp
struct StateInfo {
    bool visited_new_sparse;     // 是否访问新稀疏区域
    bool trigger_success;        // 触发是否成功
    bool collision;              // 是否发生碰撞
    double distance_to_sparse;   // 到稀疏区域的距离

    StateInfo() : visited_new_sparse(false),
                  trigger_success(false),
                  collision(false),
                  distance_to_sparse(std::numeric_limits<double>::max()) {}
};
```

### 场景类型

```cpp
enum class SceneType : uint8_t {
    UNKNOWN = 0,
    INTERSECTION = 1,     // 路口
    LANE_CHANGE = 2,      // 变道
    CURVE = 3,            // 弯道
    STRAIGHT = 4,         // 直行
    PARKING = 5,          // 停车
    ROUNDABOUT = 6,       // 环岛
    MERGE = 7,            // 合流
    DIVERGE = 8           // 分流
};
```

## 人形机器人专用结构

### HumanoidStateInfo

人形机器人原始状态信息（归一化前），来源: `src/rl_planning_infer/agents/humanoid_state.h`：

```cpp
struct HumanoidStateInfo {
    // 基座线速度 (from LivelyBot)
    double vx, vy, vz;

    // 基座角速度 (from LivelyBot)
    double wx, wy, wz;

    // 归一化位置
    double x, y;

    // 朝向
    double theta;

    // 目标信息
    double goal_dx, goal_dy, goal_distance, goal_bearing;

    // 数据价值扇区 (8方向)
    std::array<double, 8> data_value_sectors;

    // 障碍物扇区 (4方向: 前/后/左/右)
    std::array<double, 4> obstacle_sectors;

    // 当前位置数据
    double current_value;
    double current_rarity;

    // 采集状态
    double collected_ratio;
    double coverage_ratio;

    // 环境
    int terrain_type;
    double obstacle_density;

    // 步态相位 (from LivelyBot)
    double gait_phase;

    // 动作历史 (最近8步 forward_vel)
    std::array<double, 8> action_history;

    // 剩余预算
    double remaining_budget;

    // 地图参数 (用于归一化)
    double map_width, map_height, max_range;
};
```

### HumanoidState

归一化后的 43 维状态向量，对齐训练侧 `humanoid_nav_data_training.yaml`：

```cpp
struct HumanoidState {
    static constexpr int STATE_DIM = 43;
    std::vector<double> features;  // 43维归一化特征

    // 维度布局:
    //  [0-2]:   base_lin_vel [vx, vy, vz]       ×2.0
    //  [3-5]:   base_ang_vel [wx, wy, wz]       ×1.0
    //  [6-7]:   norm_position [x/W, y/H]
    //  [8-9]:   heading [sinθ, cosθ]
    //  [10-11]: goal_direction [sinΔθ, cosΔθ]
    //  [12-14]: goal_distance [Δx, Δy, ‖Δ‖]     ÷max_range
    //  [15-22]: data_value_sectors (8方向)        [0,1]
    //  [23-26]: obstacle_sectors (4方向)          ÷max_range
    //  [27-28]: current_value [value, rarity]     [0,1]
    //  [29-30]: collection_status [ratio, coverage] [0,1]
    //  [31]:    terrain_type                      ÷6
    //  [32]:    obstacle_density                  [0,1]
    //  [33]:    gait_phase                        sin(2π·φ)
    //  [34-41]: action_history (8步)              raw forward_vel
    //  [42]:    remaining_budget                  [0,1]

    static HumanoidState fromStateInfo(const HumanoidStateInfo& info);
};
```

### HumanoidAction

人形机器人 3 维连续速度命令，来源: `src/rl_planning_infer/agents/humanoid_action.h`：

```cpp
struct HumanoidAction {
    static constexpr int ACTION_DIM = 3;

    double forward_vel;    // 前进速度 (m/s) [-0.3, 0.6]
    double lateral_vel;    // 侧向速度 (m/s) [-0.3, 0.3]
    double angular_vel;    // 角速度 (rad/s)  [-0.3, 0.3]

    // 从归一化向量 [-1, 1] 构建 (ONNX 输出)
    static HumanoidAction fromNormalized(const std::vector<double>& normalized);

    // 转换为归一化向量 [-1, 1]
    std::vector<double> toNormalized() const;

    // 裁剪到有效范围
    void clip();
};
```

动作范围与 LivelyBot Pi 训练范围对齐。ONNX 模型输出 [-1, 1] 归一化值，通过 `denormalize` 映射到实际速度范围。

### Footprint

足端足迹记录：

```cpp
struct Footprint {
    Point position;        // 足端世界坐标
    double timestamp;      // 时间戳
    bool is_left_foot;     // 是否左脚
    double gait_phase;     // 步态相位
    bool is_stable;        // 是否稳定

    Footprint() : position(0, 0), timestamp(0),
                  is_left_foot(false), gait_phase(0),
                  is_stable(false) {}

    Footprint(double x, double y, bool left,
              double phase, bool stable)
        : position(x, y), is_left_foot(left),
          gait_phase(phase), is_stable(stable) {
        timestamp = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
};
```

## 价值评估结构

### DataValueResult

数据价值评估结果：

```cpp
struct DataValueResult {
    double overall_value;           // 综合价值分数 [0, 1]
    double density_score;           // 密度分数 (稀疏区高)
    double diversity_score;         // 多样性分数
    double novelty_score;           // 新颖性分数
    SceneType scene_type;           // 场景类型
    std::string reasoning;          // 评估原因

    // 优先级比较
    bool operator<(const DataValueResult& other) const {
        return overall_value < other.overall_value;
    }
};
```

### DataPointMetadata

数据点元数据（轻量级，用于边缘反馈）：

```cpp
struct DataPointMetadata {
    Point position;
    double timestamp;
    SceneType scene_type;
    double value_score;        // 数据价值分数
    std::vector<double> features; // 特征向量

    // 序列化为 JSON（约 20B）
    std::string toJson() const;
    static DataPointMetadata fromJson(const std::string& json);
};
```

## 分析数据结构

### Heatmap

密度热力图：

```cpp
struct Heatmap {
    std::vector<std::vector<double>> density_values; // 密度值矩阵
    int width;                  // 网格宽度
    int height;                 // 网格高度
    double resolution;          // 分辨率 (米/格)

    Heatmap(int w = 0, int h = 0, double res = 1.0)
        : width(w), height(h), resolution(res) {
        density_values.resize(h, std::vector<double>(w, 0.0));
    }

    // 世界坐标转网格坐标
    std::pair<int, int> worldToGrid(const Point& p) const;

    // 获取密度值
    double getDensity(const Point& p) const;

    // 更新密度
    void addDensity(const Point& p, double amount = 1.0);
};
```

### Region

区域定义：

```cpp
struct Region {
    Point center;      // 区域中心
    double radius;     // 区域半径
    bool is_sparse;    // 是否为稀疏区域
    double avg_density; // 平均密度

    Region(const Point& c = Point(), double r = 0.0, bool sparse = false)
        : center(c), radius(r), is_sparse(sparse), avg_density(0) {}

    // 判断点是否在区域内
    bool contains(const Point& p) const {
        return center.distanceTo(p) <= radius;
    }
};
```

## 上传数据结构

### UploadTask

上传任务定义：

```cpp
struct UploadTask {
    std::string file_path;    // 本地文件路径
    std::string object_key;   // S3 对象键
    std::string task_id;      // 任务 ID
    int64_t file_size;        // 文件大小
    UploadType upload_type;   // 上传类型
    int retry_count;          // 重试次数
    UploadStatus status;      // 上传状态
};
```

### UploadStatus

上传状态枚举：

```cpp
enum class UploadStatus : uint8_t {
    PENDING = 0,      // 等待上传
    UPLOADING = 1,    // 上传中
    UPLOADED = 3,     // 上传完成
    FAILED = 4        // 上传失败
};
```

### UploadType

上传类型：

```cpp
enum class UploadType : uint8_t {
    NONE = 0,
    ACTIVELY_REPORT = 3,        // 主动上报
    INSTRUCTION_DELIVERY = 4    // 指令下发
};
```

## 配置数据结构

### StrategyConfig

采集策略配置：

```cpp
struct StrategyConfig {
    std::string id;                    // 策略 ID
    std::string name;                  // 策略名称
    std::vector<TriggerConfig> triggers; // 触发器配置
    std::vector<TopicConfig> topics;    // Topic 配置
    CacheConfig cache_config;           // 缓存配置

    // 从 JSON 解析
    static StrategyConfig fromJson(const std::string& json);
    std::string toJson() const;
};
```

### PlannerWeights

规划器权重配置：

```cpp
struct PlannerWeights {
    double sparse_threshold;      // 稀疏阈值
    double exploration_bonus;     // 探索奖励
    double redundancy_penalty;    // 冗余惩罚

    PlannerWeights(double threshold = 0.2,
                   double bonus = 0.5,
                   double penalty = 0.4)
        : sparse_threshold(threshold),
          exploration_bonus(bonus),
          redundancy_penalty(penalty) {}

    // 从 YAML 加载
    static PlannerWeights fromYaml(const std::string& yaml_path);
};
```

## 数据转换

### JSON 序列化

使用 `nlohmann/json` 实现序列化：

```cpp
// 自动序列化
void to_json(nlohmann::json& j, const Point& p);
void from_json(const nlohmann::json& j, Point& p);

void to_json(nlohmann::json& j, const DataPoint& dp);
void from_json(const nlohmann::json& j, DataPoint& dp);
```

### ROS 消息转换

与 ROS2 消息类型的转换：

```cpp
// Point ↔ geometry_msgs::msg::Point
geometry_msgs::msg::Point toRosMsg(const Point& p);
Point fromRosMsg(const geometry_msgs::msg::Point& msg);

// Path ↔ nav_msgs::msg::Path
nav_msgs::msg::Path toRosMsg(const Path& path);
Path fromRosMsg(const nav_msgs::msg::Path& msg);
```

## 内存优化

### 小字符串优化

```cpp
struct SmallString {
    std::string data;

    // 使用 SSO (Small String Optimization)
    // 短字符串 (< 15 字节) 存储在栈上
    void assign(const std::string& s) {
        if (s.size() < 16) {
            // 使用栈存储
            data = s;
        } else {
            // 使用堆存储
            data = s;
        }
    }
};
```

### 数据池

```cpp
template<typename T>
class DataPool {
public:
    // 获取对象
    std::shared_ptr<T> acquire() {
        if (!pool_.empty()) {
            auto obj = pool_.back();
            pool_.pop_back();
            return obj;
        }
        return std::make_shared<T>();
    }

    // 归还对象
    void release(std::shared_ptr<T> obj) {
        obj->reset();
        pool_.push_back(obj);
    }

private:
    std::vector<std::shared_ptr<T>> pool_;
};
```
