**Breadcrumbs:** [Docs](../../../README.md) / [Developer Guide](../../index.md) / [Architecture](../index.md) / [Algorithms](index.md) / Gait Detection

# 步态检测算法

## 概述

步态检测算法用于双足机器人数据采集场景，通过分析机器人实际运动状态，在**双脚支撑稳定期**触发数据采集。与传统基于规划路径点的采集不同，该方法基于**实际足端落地位置**进行采集决策。

## 设计原则

1. **实际落地优先**：采集点基于实际足端位置，而非规划路径点
2. **稳定期采集**：只在双脚支撑的稳定相位触发采集
3. **步长约束**：确保采集点间最小距离避免冗余
4. **去重机制**：记录历史足迹，避免重复采集

## 步态周期

```mermaid
graph LR
    A[左脚着地] --> B[左脚支撑<br/>右脚摆动]
    B --> C[双脚支撑]
    C --> D[右脚着地]
    D --> E[右脚支撑<br/>左脚摆动]
    E --> C
    C --> A
```

### 步态相位定义

| 相位 | 范围 | 状态 | 可采集 |
|------|------|------|--------|
| 左脚支撑 | [0.6π, 1.4π] | 左脚支撑，右脚摆动 | 否 |
| 右脚支撑 | [0, 0.4π] ∪ [1.6π, 2π] | 右脚支撑，左脚摆动 | 否 |
| 双脚支撑 | [0.4π, 0.6π] ∪ [1.4π, 1.6π] | 双脚接触地面 | **是** |

## 算法实现

### 步态状态机

```cpp
class GaitStateMachine {
public:
    enum class Phase {
        LEFT_STANCE = 0,      // 左脚支撑相
        RIGHT_STANCE = 1,     // 右脚支撑相
        DOUBLE_SUPPORT = 2,   // 双脚支撑
        SWING = 3             // 摆动相
    };

    struct GaitState {
        Phase phase;              // 当前相位
        double phase_value;       // 相位值 [0, 2π]
        bool left_contact;        // 左脚接触
        bool right_contact;       // 右脚接触
        double left_foot_z;       // 左脚高度
        double right_foot_z;      // 右脚高度
        uint64_t step_count;      // 步数计数

        bool is_stable() const {
            return phase == Phase::DOUBLE_SUPPORT &&
                   left_contact && right_contact;
        }
    };

    // 更新步态状态
    void update(const FootState& left, const FootState& right);

    // 判断是否在稳定期
    bool isInStablePhase() const;

private:
    GaitState state_;
    double phase_start_time_;
};
```

### 足端落地检测

```cpp
class FootstrikeDetector {
public:
    struct FootState {
        Point position;       // 足端位置
        double height;        // 足端高度
        double velocity_z;    // 垂直速度
        double force;         // 接触力
        bool is_contact;      // 是否接触
    };

    // 检测足端落地事件
    bool detectFootstrike(const FootState& foot_state) {
        // 条件 1: 高度接近地面
        bool near_ground = foot_state.height < ground_threshold_;

        // 条件 2: 垂直速度接近零 (从负变正)
        bool velocity_sign_change =
            prev_velocity_z_ < 0 && foot_state.velocity_z >= 0;

        // 条件 3: 接触力突然增加
        bool force_increase =
            foot_state.force > force_threshold_ &&
            (foot_state.force - prev_force_) > force_delta_threshold_;

        bool detected = near_ground &&
                        (velocity_sign_change || force_increase);

        prev_velocity_z_ = foot_state.velocity_z;
        prev_force_ = foot_state.force;

        return detected;
    }

private:
    double ground_threshold_ = 0.01;      // 地面阈值 1cm
    double force_threshold_ = 10.0;       // 力阈值 10N
    double force_delta_threshold_ = 5.0;  // 力变化阈值
    double prev_velocity_z_ = 0;
    double prev_force_ = 0;
};
```

### 稳定期判断

```cpp
class StablePhaseDetector {
public:
    // 判断是否处于稳定期 (可采集)
    bool isStable(const GaitState& state) {
        // 双脚必须同时接触
        if (!state.left_contact || !state.right_contact) {
            return false;
        }

        // 检查相位是否在稳定范围内
        double phase = state.phase_value;

        // 稳定相位: [0.4π, 0.6π] 和 [1.4π, 1.6π]
        bool in_stable_phase =
            (phase >= 0.4 * M_PI && phase <= 0.6 * M_PI) ||
            (phase >= 1.4 * M_PI && phase <= 1.6 * M_PI);

        if (!in_stable_phase) {
            return false;
        }

        // 检查足端速度是否足够小 (稳定)
        bool velocity_stable =
            std::abs(state.left_foot_velocity) < velocity_threshold_ &&
            std::abs(state.right_foot_velocity) < velocity_threshold_;

        return velocity_stable;
    }

private:
    double velocity_threshold_ = 0.1;  // 速度阈值 0.1 m/s
};
```

## 数据采集触发

### 触发条件

```cpp
class GaitTrigger {
public:
    bool shouldTriggerCollection(
        const Point& current_pos,
        const Point& last_collect_pos,
        const std::chrono::steady_clock::time_point& last_collect_time
    ) {
        // 条件 1: 在稳定期
        if (!isInStablePhase()) {
            return false;
        }

        // 条件 2: 步长满足最小距离
        double distance = current_pos.distanceTo(last_collect_pos);
        if (distance < min_step_distance_) {
            return false;
        }

        // 条件 3: 时间间隔满足
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_collect_time
        ).count() / 1000.0;

        if (elapsed < min_collection_interval_) {
            return false;
        }

        // 条件 4: 去重检查
        if (isDuplicateLocation(current_pos)) {
            return false;
        }

        return true;
    }

private:
    double min_step_distance_ = 0.15;       // 最小步长 15cm
    double min_collection_interval_ = 1.0;  // 最小间隔 1s
    double duplicate_threshold_ = 0.05;     // 去重阈值 5cm
};
```

### 足迹记录与去重

```cpp
class FootprintTracker {
public:
    static constexpr size_t MAX_FOOTPRINTS = 1000;

    // 添加足迹
    void addFootprint(const Footprint& fp) {
        std::lock_guard<std::mutex> lock(mutex_);

        footprints_.push_back(fp);

        // 超过最大数量时移除最旧的
        if (footprints_.size() > MAX_FOOTPRINTS) {
            footprints_.erase(footprints_.begin());
        }
    }

    // 检查是否重复
    bool isDuplicate(const Point& pos, double threshold = 0.05) const {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& fp : footprints_) {
            if (fp.position.distanceTo(pos) < threshold) {
                return true;
            }
        }
        return false;
    }

    // 获取最近的稳定足迹
    Footprint getLastStableFootprint() const {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto it = footprints_.rbegin();
             it != footprints_.rend(); ++it) {
            if (it->is_stable) {
                return *it;
            }
        }
        return Footprint();
    }

private:
    std::vector<Footprint> footprints_;
    mutable std::mutex mutex_;
};
```

## ROS2 集成

### Topic 订阅

```cpp
class GaitTrigger : public rclcpp::Node {
public:
    GaitTrigger() : Node("gait_trigger") {
        // 订阅里程计
        odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/robot/odom", 50,
            std::bind(&GaitTrigger::odomCallback, this, std::placeholders::_1)
        );

        // 订阅关节状态
        joint_sub_ = this->create_subscription<sensor_msgs::msg::JointState>(
            "/robot/joint_states", 100,
            std::bind(&GaitTrigger::jointCallback, this, std::placeholders::_1)
        );
    }

private:
    void odomCallback(const nav_msgs::msg::Odometry::SharedPtr msg) {
        // 更新机器人位置
        robot_position_.x = msg->pose.pose.position.x;
        robot_position_.y = msg->pose.pose.position.y;

        // 提取姿态
        auto& q = msg->pose.pose.orientation;
        robot_yaw_ = std::atan2(
            2.0 * (q.w * q.z + q.x * q.y),
            1.0 - 2.0 * (q.y * q.y + q.z * q.z)
        );

        // 分析步态状态
        analyzeGaitState();
    }

    void jointCallback(const sensor_msgs::msg::JointState::SharedPtr msg) {
        // 更新关节状态用于运动学计算
        // ... (具体实现)
    }
};
```

### 足端运动学

```cpp
class LegKinematics {
public:
    struct JointAngles {
        double hip_yaw;   // 髋关节偏航
        double hip_roll;  // 髋关节滚转
        double hip_pitch; // 髋关节俯仰
        double knee_pitch;// 膝关节俯仰
        double ankle_pitch;// 踝关节俯仰
        double ankle_roll;// 踝关节滚转
    };

    // 正向运动学: 关节角度 → 足端位置
    Point forwardKinematics(const JointAngles& joints, bool is_left) {
        // 腿部分段长度
        const double upper_leg = 0.35;  // 大腿 35cm
        const double lower_leg = 0.35;  // 小腿 35cm

        // 简化计算 (实际需要完整 DH 参数)
        double x = upper_leg * std::sin(joints.hip_pitch) +
                   lower_leg * std::sin(joints.hip_pitch + joints.knee_pitch);

        double y = is_left ? -0.05 : 0.05;  // 髋宽 10cm
        double z = -(upper_leg * std::cos(joints.hip_pitch) +
                     lower_leg * std::cos(joints.hip_pitch + joints.knee_pitch));

        return Point(x, y);
    }

    // 逆向运动学: 足端位置 → 关节角度
    JointAngles inverseKinematics(const Point& foot_pos, bool is_left) {
        // 解析解 (简化)
        JointAngles result;

        // ... (具体实现)

        return result;
    }
};
```

## 参数配置

### 可调参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `min_step_distance` | 0.15m | 最小步长，防止过于密集采集 |
| `min_collection_interval` | 1.0s | 最小采集间隔 |
| `stable_phase_threshold` | 0.3 | 稳定相位阈值 (支撑相中间 60%) |
| `duplicate_threshold` | 0.05m | 去重距离阈值 |
| `ground_threshold` | 0.01m | 地面检测阈值 |

### 配置文件

```yaml
# config/gait_trigger.yaml
gait_trigger:
  # 步长约束
  min_step_distance: 0.15      # 15cm
  max_step_distance: 1.5       # 1.5m

  # 时间约束
  min_collection_interval: 1.0 # 1秒

  # 稳定相位
  stable_phase_start: 0.4      # 0.4π
  stable_phase_end: 0.6        # 0.6π

  # 去重
  duplicate_threshold: 0.05    # 5cm
  max_footprints: 1000         # 最大足迹记录数

  # 机器人参数
  hip_width: 0.10              # 髋宽 10cm
  upper_leg_length: 0.35       # 大腿 35cm
  lower_leg_length: 0.35       # 小腿 35cm
```

## 验证结果

### 采集统计

| 指标 | 数值 |
|------|------|
| 规划路径点 | 501 |
| 实际采集点 | 90 |
| 过滤率 | 82% |
| 平均步长 | 0.65m |
| 平均采集间隔 | 1.1s |

### 步长分布

```
0.28m |████ (调整步)
0.65m |████████████████ (正常步行)
1.36m |████ (大步)
```

## 性能指标

| 指标 | 数值 |
|------|------|
| 检测频率 | 50Hz |
| 检测延迟 | < 5ms |
| CPU 占用 | < 5% |
| 内存占用 | < 10MB |
