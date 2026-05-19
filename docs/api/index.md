**Breadcrumbs:** [Docs](../README.md) / [Api](index.md) / Index

# API 文档

本章节包含 Aurora-Edge-Runtime 的 API 参考文档，面向开发者进行集成和扩展开发。

## 目录

### C++ API

- [命名空间](cpp/namespaces.md) - 命名空间索引
- [类索引](cpp/classes.md) - 类列表
- [文件索引](cpp/files.md) - 源文件索引

### Python API

> Python API 正在开发中，敬请期待...

### ROS2 接口

- [话题列表](ros2/topics.md) - 发布/订阅的话题
- [服务列表](ros2/services.md) - 提供的服务
- [消息定义](ros2/messages.md) - 自定义消息类型

## 快速参考

### 核心类

| 类 | 头文件 | 说明 |
|----|--------|------|
| `DataCollectionPlanner` | `data_collection_planner.h` | 主控制器 |
| `HumanoidPlanner` | `humanoid_planner.h` | 人形机器人规划器 |
| `GaitTrigger` | `gait_trigger.h` | 步态触发器 |
| `DataManager` | `data_manager.h` | 数据管理器 |
| `AwsDataUploader` | `aws_data_uploader.h` | AWS上传器 |

### ROS2 话题

| 话题 | 类型 | 方向 | 频率 |
|-----|------|------|------|
| `/robot/odom` | nav_msgs/Odometry | 订阅 | 50Hz |
| `/robot/joint_states` | sensor_msgs/JointState | 订阅 | 50Hz |
| `/robot/cmd_vel` | geometry_msgs/Twist | 发布 | 50Hz |
| `/planned_path` | nav_msgs/Path | 发布 | 1Hz |

### 代码示例

#### 初始化规划器

```cpp
#include "data_collection_planner.h"

using namespace dcp;

// 创建规划器
DataCollectionPlanner planner(
    "models/humanoid_ppo.onnx",
    "config/planner_weights.yaml",
    PlannerMode::HUMANOID
);

// 初始化
if (!planner.initialize()) {
    std::cerr << "Failed to initialize planner" << std::endl;
    return -1;
}

// 设置任务区域
MissionArea area;
area.center = Point(0, 0);
area.radius = 10.0;
planner.setMissionArea(area);
```

#### 执行采集任务

```cpp
// 规划路径
auto path = planner.planDataCollectionMission();

// 执行采集（带反馈）
planner.executeWithFeedback(path);

// 获取统计
auto stats = planner.getLearningStats();
std::cout << "Average reward: " << stats.avg_reward << std::endl;
```

#### 使用 ROS2 服务

```bash
# 触发采集
ros2 service call /aer/trigger_collection \
  dcp_msgs/srv/TriggerCollection \
  "{trigger_id: 'manual_001', x: 1.0, y: 2.0}"

# 获取状态
ros2 service call /aer/get_status \
  dcp_msgs/srv/GetStatus
```

## 生成文档

### Doxygen

```bash
# 生成 C++ API 文档
cd docs
doxygen Doxyfile

# 查看文档
firefox html/index.html
```

### Sphinx

```bash
# 生成完整文档
cd docs
sphinx-build -b html . _build/html

# 查看文档
firefox _build/html/index.html
```

## 在线文档

完整的 API 文档可在以下地址查看：

- 📖 [在线文档](https://docs.example.com)
- 📦 [Doxygen XML](https://doxygen.example.com)
