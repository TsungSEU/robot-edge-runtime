**Breadcrumbs:** [Docs](../../README.md) / [Developer Guide](../index.md) / [Architecture](index.md) / Integration Flow

# 系统集成流程

Aurora Edge Runtime 作为边云协同系统的边缘侧，介于 LivelyBot 机器人硬件与 aurora-planning-engine 云端训练之间。

## 整体架构

```
                    ROS2 Topics (LivelyBot → Edge)                         ROS2 Topics (Edge → LivelyBot)
                 ┌────────────────────────────────────┐                ┌──────────────────────────────────────┐
                 │ /robot/odom         Odometry   50Hz│                │ /robot/velocity_cmd  Twist     10Hz  │
                 │ /robot/joint_states JointState 50Hz│                └──────────────────────────────────────┘
                 │ /robot/imu          Imu        50Hz│
                 │ /robot/cmd_vel      Twist      50Hz│ (实际执行速度反馈)         ROS2 Services (Edge → LivelyBot)
                 │ /robot_description  String     once│                ┌───────────────────────────────────────┐
                 │ /tf                 TFMessage  50Hz│                │ /robot/set_path     SetTargetPath     │
                 └────────────────────────────────────┘                │ /robot/get_position GetCurrentPosition│
                                                                       │ /robot/get_errors   GetErrorStatistics│
┌──────────────────┐                                                   │ /robot/clear_errors ClearErrors       │ 
│   LivelyBot      │                                                   └───────────────────────────────────────┘
│   (50Hz 低层行走) │◄──────────────────────────────────────────────────────────────────────────────────────┐
│                  │──────────────────────────────────────────────────────────────────────────────────────►│
└──────────────────┘                                                                                       │
                                                                                                           │
                    ROS2 Services (Edge → DataCollection)                                                  │
                 ┌──────────────────────────────────────┐                                                  │
                 │ /robot/trigger  TriggerRecording     │                                                  │
                 └──────────────────────────────────────┘                                                  │
                                                                                                           │
                                                                                     ┌─────────────────────┘
                                                                                     │
                                                                                  ┌──┴───────────┐
                                     ONNX model ↓                                 │ aurora-      │
                                                                                  │ planning-    │
                                          ↑ Collected data (S3/MQTT)              │ engine (云端) │
                                                                                  └──────────────┘
```

### ROS2 消息详解

#### Topics: LivelyBot → Aurora Edge Runtime

| Topic | 类型 | 频率 | QoS | 用途 |
|-------|------|------|-----|------|
| `/robot/odom` | nav_msgs/Odometry | 50Hz | odometry | 机器人位姿、线/角速度 — 构造 43 维状态 [0-5]，位置跟踪 |
| `/robot/joint_states` | sensor_msgs/JointState | 50Hz | sensor_data | 12维关节位置/速度/力矩 — 步态相位检测，ring buffer 录制 |
| `/robot/imu` | sensor_msgs/Imu | 50Hz | sensor_data | 加速度计 + 陀螺仪 — ring buffer 录制 |
| `/robot/cmd_vel` | geometry_msgs/Twist | 50Hz | velocity_cmd | 实际执行速度反馈 — 闭环速度跟踪 |
| `/robot_description` | std_msgs/String | once | static_data | URDF 机器人模型描述 |
| `/tf` | tf2_msgs/TFMessage | 50Hz | tf_transforms | 坐标变换树 (base_link → odom) |

#### Topics: Aurora Edge Runtime → LivelyBot

| Topic | 类型 | 频率 | QoS | 用途 |
|-------|------|------|-----|------|
| `/robot/velocity_cmd` | geometry_msgs/Twist | 10Hz | velocity_cmd | PPO 推理输出速度命令 — forward/lateral/angular |

#### Services: Aurora Edge Runtime → LivelyBot

| Service | 类型 | 触发时机 | 用途 |
|---------|------|----------|------|
| `/robot/set_path` | SetTargetPath | 路径规划完成 | 设置导航航路点序列 |
| `/robot/get_position` | GetCurrentPosition | 每 5s 轮询 | 获取当前位姿用于闭环跟踪 |
| `/robot/get_errors` | GetErrorStatistics | 每 5s 轮询 | 获取路径跟踪误差统计 |
| `/robot/clear_errors` | ClearErrors | 按需 | 重置误差统计 |

#### Services: Aurora Edge Runtime → Data Collection

| Service | 类型 | 触发时机 | 用途 |
|---------|------|----------|------|
| `/robot/trigger` | TriggerRecording | GaitTrigger 步态事件 / 采集点到达 | 触发 rosbag2 录制 (15s pre + 5s post) |

#### Topics: Aurora Edge Runtime → RViz2 可视化

| Topic | 类型 | 频率 | QoS | 用途 |
|-------|------|------|-----|------|
| `/planning_path_vis` | visualization_msgs/Marker | 路径变更时 | visualization | 规划路径 (绿色) |
| `/planning_traj_vis` | visualization_msgs/Marker | 轨迹更新时 | visualization | 轨迹点 (红色) |
| `/start_goal_markers` | visualization_msgs/Marker | 路径更新时 | visualization | 起点/终点标记 |
| `/collected_path_vis` | visualization_msgs/Marker | 采集更新时 | visualization | 已采集路径 (蓝色) |
| `/robot/trail` | nav_msgs/Path | 5Hz | visualization | 实时机器人轨迹 |
| `/collection_points_vis` | visualization_msgs/Marker | 采集点更新时 | visualization | 采集点 (青色) |
| `/robot/health` | std_msgs/String | 1Hz | static_data | 系统健康 JSON |

### 消息时序

```
LivelyBot                     Aurora Edge Runtime                Aurora Planning Engine(Cloud)
   │                                │                                  │
   │──── /robot/odom (50Hz) ───────►│                                  │
   │──── /robot/joint_states ──────►│  ───► 步态相位检测 (GaitTrigger)   │
   │──── /robot/imu ───────────────►│  ───► Ring Buffer 写入            │
   │                                │                                  │
   │                                │── ONNX inference (10Hz) ──►      │
   │  (43-dim state → 3-dim action) │                                  │
   │                                │                                  │
   │◄─── /robot/velocity_cmd ───────│                                  │
   │     (forward, lateral, angular)│                                  │
   │                                │                                  │
   │  [50Hz gait controller]        │                                  │
   │──── /robot/cmd_vel (实际速度) ─►│  ───► 闭环速度跟踪                 │
   │                                │                                  │
   │                                │── /robot/set_path ──────────────►│
   │                                │── /robot/get_position (5s轮询) ──►│
   │                                │── /robot/get_errors (5s轮询) ────►│
   │                                │                                   │
   │   [步态事件触发]                 │                                   │
   │                                │── /robot/trigger ──► Recording    │
   │                                │                      (15s+5s)     │
   │                                │── S3 upload ─────────────────────►│
   │                                │── MQTT metadata ─────────────────►│
```

三层分工：

| Layer | 频率 | 职责 |
|-------|------|------|
| Aurora Edge | 10 Hz | 高层导航决策 (PPO 推理)，输出速度命令 |
| LivelyBot | 50 Hz | 低层双足行走控制，接收速度命令 → 12维关节目标 |
| Physics/MuJoCo | 1000 Hz | 物理仿真 (训练时) |

## 数据流详解

### Phase 1: 推理 (Cloud → Edge → Robot)

```
aurora-planning-engine 训练产出 ONNX 模型
         │
         ↓  部署到 models/humanoid_ppo.onnx (S3/MQTT/手动)
Aurora Edge Runtime 加载 ONNX
         │
         ↓  构造 43 维状态 (HumanoidStateInfo):
         │    [0-5]:   基座线速度/角速度 ← LivelyBot /robot/odom
         │    [6-14]:  位置/朝向/目标距离
         │    [15-22]: 8方向数据价值       ← CostMap 扇区扫描
         │    [23-26]: 4方向障碍距离       ← CostMap 射线检测
         │    [27-33]: 采集状态/地形/步态
         │    [34-42]: 动作历史(8步 forward_vel) + 剩余预算
         ↓
    ONNX 推理 → action_mean [3]
         │
         ↓  反归一化: [-1,1] → forward [-0.3, 0.6], lateral [-0.3, 0.3], angular [-0.3, 0.3]
         │
    发布 /robot/velocity_cmd (geometry_msgs/Twist) @ 10Hz
         │
         ↓
LivelyBot 接收速度命令 → gait 控制器 (50Hz) → 12 维关节执行
```

### Phase 2: 数据采集 (Robot → Edge → Cloud)

```
LivelyBot 发布传感器数据:
    /robot/odom (50Hz)       → Edge 订阅，跟踪机器人位置
    /robot/joint_states      → 记录到 ring buffer
    /robot/imu               → 记录到 ring buffer
         │
         ↓
DataCollectionExecutor:
    - GaitTrigger 检测步态事件 (步态相位变化点)
    - 触发 rosbag2 录制 (ring buffer: 15s pre + 5s post)
    - 冷却期 1s, 最小采集间隔 0.5s
         │
         ↓  生成 .db3 文件
AwsDataUploader:
    - S3 multipart upload → caic-dataset bucket
    - MQTT 上报元数据
```

### Phase 3: 模型迭代 (Cloud → Edge)

```
aurora-planning-engine:
    1. 读取 S3 采集数据, 构建训练集
    2. PPO 训练 (43-dim state, 3-dim action, [256,128,64])
       - 4096 并行环境, 24 steps/update
       - 10 分量奖励: w_data_value=5.0 (核心) + w_approach=3.0 + ...
    3. 导出 ONNX → 部署到 Edge
    4. 闭环验证 (MuJoCo 联合仿真)
```

## ROS2 接口

> 完整的 Topic/Service 清单、QoS 配置及消息时序图见上方「整体架构 → ROS2 消息详解」章节。

## Humanoid 状态空间 (43维)

对齐训练侧 `humanoid_nav_data_training.yaml`:

| 索引 | 组件 | 维度 | 归一化 | 来源 |
|------|------|------|--------|------|
| 0-2 | 基座线速度 | 3 | x2.0 | LivelyBot odom |
| 3-5 | 基座角速度 | 3 | x1.0 | LivelyBot odom |
| 6-7 | 归一化位置 | 2 | x/W, y/H | odom + map |
| 8-9 | 朝向 | 2 | sin,cos | odom theta |
| 10-11 | 目标方向 | 2 | sin,cos | 目标方位角 |
| 12-14 | 目标距离 | 3 | /max_range | 距目标Δx,Δy,‖Δ‖ |
| 15-22 | 数据价值扇区 | 8 | [0,1] | CostMap 8方向扫描 |
| 23-26 | 障碍物扇区 | 4 | /max_range | CostMap 射线检测 |
| 27-28 | 当前位置价值 | 2 | [0,1] | DataValueModel |
| 29-30 | 采集状态 | 2 | [0,1] | collected_ratio, coverage |
| 31 | 地形类型 | 1 | /6 | 地形分类 |
| 32 | 障碍密度 | 1 | [0,1] | 局部密度 |
| 33 | 步态相位 | 1 | sin(2πφ) | LivelyBot |
| 34-41 | 动作历史 | 8 | raw | 最近8步 forward_vel |
| 42 | 剩余预算 | 1 | [0,1] | 时间/步数 |

## 动作空间 (3维连续)

| 索引 | 名称 | 范围 | 单位 | 说明 |
|------|------|------|------|------|
| 0 | forward_vel | [-0.3, 0.6] | m/s | 前进/后退速度 |
| 1 | lateral_vel | [-0.3, 0.3] | m/s | 侧向速度 |
| 2 | angular_vel | [-0.3, 0.3] | rad/s | 角速度 |

动作范围与 LivelyBot Pi 训练范围对齐。

## 运行模式

| 模式 | `--mode` | 状态维度 | 动作维度 | 说明 |
|------|----------|----------|----------|------|
| Auto | `auto` | 25 | 4 (离散) | 自动驾驶 |
| Humanoid | `humanoid` | 43 | 3 (连续) | 分层导航+数据采集 (默认) |

```bash
# 启动 humanoid 模式 (默认)
./build/src/aer --mode humanoid --config config/planner_weights.yaml

# 启动 auto 模式
./build/src/aer --mode auto --config config/planner_weights.yaml
```

## 源码结构

```
src/rl_planning_infer/
├── config/          # 配置管理 (原 common/)
├── core/            # 核心规划器 (原 planner/)
│   ├── auto_planner.h/cpp
│   ├── humanoid_planner.h/cpp
│   ├── planner_base.hpp
│   ├── planner_factory.h/cpp
│   └── path_visualizer.h/cpp
├── agents/          # RL 推理代理 (原 rl_policy/)
│   ├── auto_ppo_agent.h/cpp
│   ├── humanoid_ppo_agent.h/cpp
│   ├── humanoid_state.h/cpp
│   ├── humanoid_action.h
│   ├── humanoid_reward.h
│   └── onnx_inference_engine.h
├── observation/     # 状态表示 (原 state/)
│   ├── traits/      # 状态特征定义
│   ├── factory/     # 状态工厂
│   └── utils/       # 状态转换器
├── safety/          # 语义约束 (原 safety_layer/)
├── maps/            # 代价地图 (原 value_map/)
└── utils/           # 工具函数
```

## robot_sim 的角色

`robot_sim` 是开发/测试工具，**不是生产组件**：

| 场景 | 是否需要 |
|------|----------|
| 本地开发调试 RL 推理 | 需要 |
| 集成测试 | 需要 |
| 生产部署 (systemd) | 不需要 — 连接真实 LivelyBot |
| LivelyBot 实机联调 | 不需要 — LivelyBot 提供相同 ROS2 接口 |

```bash
# 开发模式: 同时启动 robot_sim + aer
bash launch/launch_dual_process.sh

# 生产模式: 仅启动 aer (连接真实 LivelyBot)
sudo systemctl start aer
```
