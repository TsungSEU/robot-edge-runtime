# Aurora-Edge-Runtime 产品说明书 v1.1.2

## 文档信息

| 项目 | 内容 |
|-----|------|
| **产品名称** | Aurora-Edge-Runtime |
| **所属产品线** | Aurora 产品矩阵 |
| **产品版本** | v1.1.2 |
| **文档版本** | 1.1 |
| **编制日期** | 2026-03-07 |
| **文档状态** | 正式发布 |
| **编制单位** | Aurora Edge Runtime Team |

---

## 目录

1. [产品概述](#1-产品概述)
2. [Aurora产品矩阵](#2-aurora产品矩阵)
3. [产品架构](#3-产品架构)
4. [功能规格](#4-功能规格)
5. [技术规格](#5-技术规格)
6. [部署指南](#6-部署指南)
7. [配置说明](#7-配置说明)
8. [API接口](#8-api接口)
9. [监控与运维](#9-监控与运维)
10. [故障排查](#10-故障排查)
11. [版本规划](#11-版本规划)

---

## 1. 产品概述

### 1.1 产品定义

**Aurora-Edge-Runtime** 是 Aurora 产品矩阵中专注于**机器人端侧智能数据采集**的核心运行时系统。作为边缘侧的数据采集基础设施，它通过强化学习驱动的智能规划和精准触发机制，实现高价值训练数据的自动化采集，为云端模型训练提供高质量数据源。

### 1.2 核心价值主张

```
┌─────────────────────────────────────────────────────────────┐
│           Aurora-Edge-Runtime 核心价值                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  🎯 精准采集                                                 │
│  ├─ 步态级触发（非路径点）                                     │
│  ├─ 82%冗余过滤率                                            │
│  └─ 稳定支撑相采集                                            │
│                                                             │
│  ⚡ 端侧智能                                                  │
│  ├─ <10ms 推理延迟（ONNX）                                    │
│  ├─ 43维状态空间感知                                          │
│  └─ 实时价值评估                                              │
│                                                             │
│  🔄 闭环协同                                                 │
│  ├─ 边缘轻量推理                                              │
│  ├─ 云端批量训练                                              │
│  └─ 模型热更新                                                │
│                                                             │
│  📦 开箱即用                                                 │
│  ├─ Docker 容器化                                            │
│  ├─ systemd 服务化                                           │
│  └─ 配置热更新                                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 与传统方案对比

| 维度 | 传统数据采集方案 | Aurora-Edge-Runtime |
|-----|-----------------|---------------------|
| **采集策略** | 人工规划/固定间隔/随机 | RL驱动的动态规划 |
| **触发机制** | 时间/距离触发 | **步态相位触发**（行业首创） |
| **数据价值** | 无法评估 | 实时多维价值评分 |
| **冗余率** | 70-90% | **<20%** |
| **闭环周期** | 数周至数月 | **小时级** |
| **部署复杂度** | 多组件手动集成 | **容器化一键部署** |
| **资源占用** | 高（CPU 30%+, 内存 500MB+） | **低（CPU <10%, 内存 <150MB）** |

### 1.4 目标用户与场景

#### 1.4.1 目标用户

| 用户类型 | 核心痛点 | 产品价值 |
|---------|---------|---------|
| **机器人研发团队** | 测试数据质量差、采集效率低 | 智能规划，高价值数据优先采集 |
| **自动驾驶算法团队** | 长尾场景数据不足 | 主动探索稀疏区域 |
| **仿真转现实团队** | Sim-to-Real gap大 | 真实场景精准采集 |
| **数据运营团队** | 数据管理混乱 | 自动化采集-上传-管理闭环 |

#### 1.4.2 应用场景

```
┌─────────────────────────────────────────────────────────────┐
│              Aurora-Edge-Runtime 应用场景矩阵                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  🤖 人形机器人                              🚗 自动驾驶车辆  │
│  ┌─────────────────────┐          ┌─────────────────────┐ │
│  │ • 室内导航数据采集   │          │ • 城市道路测试      │ │
│  │ • 步态控制优化       │          │ • 测试场数据采集    │ │
│  │ • 平衡性验证         │          │ • 边缘场景覆盖      │ │
│  │ • 地形适应性训练     │          │ • 语义约束导航      │ │
│  └─────────────────────┘          └─────────────────────┘ │
│                                                             │
│  🏭 工业机器人                              🛒 服务机器人    │
│  ┌─────────────────────┐          ┌─────────────────────┐ │
│  │ • 操作技能学习       │          │ • 室内环境建图      │ │
│  │ • 精度验证数据       │          │ • 交互场景采集      │ │
│  │ • 异常检测样本       │          │ • 用户行为数据      │ │
│  └─────────────────────┘          └─────────────────────┘ │
│                                                             │
│  ☁️ 与 Aurora 云端协同                                      │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Aurora-Cloud-Trainer ← S3 → Aurora-Edge-Runtime    │   │
│  │   ↓ 模型训练                ↑ 数据上传               │   │
│  │ Aurora-Model-Hub ────────→ 模型热更新               │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.5 产品定位

#### 1.5.1 在 Aurora 产品矩阵中的位置

```
┌─────────────────────────────────────────────────────────────┐
│                    Aurora 产品矩阵                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────────────────────────────────────────────────┐ │
│  │              Aurora Cloud (云端)                      │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ │ │
│  │  │ Aurora-      │  │ Aurora-      │  │ Aurora-    │ │ │
│  │  │ Cloud-       │  │ Model-       │  │ Data-      │ │ │
│  │  │ Trainer      │  │ Hub          │  │ Lake       │ │ │
│  │  │ (模型训练)    │  │ (模型仓库)   │  │ (数据湖)   │ │ │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬─────┘ │ │
│  └─────────┼──────────────────┼──────────────────┼───────┘ │
│            │                  │                  │         │
│            │ 模型下发         │ 模型上报          │ 数据上报 │
│            ↕                  ↕                  ↕         │
│  ┌───────────────────────────────────────────────────────┐ │
│  │              Aurora Edge (边缘侧)                     │ │
│  │  ┌─────────────────────────────────────────────────┐ │ │
│  │  │   Aurora-Edge-Runtime (本项目)                  │ │ │
│  │  │   ┌──────────────┐  ┌──────────────┐           │ │ │
│  │  │   │ 智能规划     │  │ 精准触发     │           │ │ │
│  │  │   │ (RL Planner) │  │ (Gait Trigger│           │ │ │
│  │  │   └──────────────┘  └──────────────┘           │ │ │
│  │  │   ┌──────────────┐  ┌──────────────┐           │ │ │
│  │  │   │ 数据管理     │  │ 云端协同     │           │ │ │
│  │  │   │ (DataMgr)    │  │ (Uploader)   │           │ │ │
│  │  │   └──────────────┘  └──────────────┘           │ │ │
│  │  └─────────────────────────────────────────────────┘ │ │
│  │                                                         │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ │ │
│  │  │ Aurora-      │  │ Aurora-      │  │ Aurora-    │ │ │
│  │  │ Simulation   │  │ Monitoring   │  │ Deployment │ │ │
│  │  │ (仿真环境)   │  │ (监控平台)   │  │ (部署工具) │ │ │
│  │  └──────────────┘  └──────────────┘  └────────────┘ │ │
│  └───────────────────────────────────────────────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### 1.5.2 产品定位总结

| 定位维度 | 描述 |
|---------|------|
| **产品定位** | Aurora 产品矩阵的端侧数据采集 Runtime |
| **市场定位** | 机器人/自动驾驶智能数据采集基础设施 |
| **技术定位** | 强化学习 + 边缘计算 + 云边协同 |
| **生态定位** | 连接机器人硬件与云端训练的数据桥梁 |
| **价值定位** | "让机器人智能采集高价值训练数据" |

---

## 2. Aurora产品矩阵

### 2.1 产品矩阵全景

Aurora 是面向机器人与自动驾驶的**全栈式AI训练与数据平台**，覆盖从数据采集、仿真训练、模型管理到部署运营的全生命周期。

```
┌─────────────────────────────────────────────────────────────┐
│                 Aurora 产品矩阵架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ╔═══════════════════════════════════════════════════════╗  │
│  ║                    Aurora Cloud                       ║  │
│  ╠═══════════════════════════════════════════════════════╣  │
│  ║                                                        ║  │
│  ║  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ ║  │
│  ║  │ Aurora-      │  │ Aurora-      │  │ Aurora-    │ ║  │
│  ║  │ Cloud-       │  │ Model-       │  │ Data-      │ ║  │
│  ║  │ Trainer      │  │ Hub          │  │ Lake       │ ║  │
│  ║  │              │  │              │  │            │ ║  │
│  ║  │ • PPO训练    │  │ • 模型版本   │  │ • 数据存储 │ ║  │
│  ║  │ • 分布式训练 │  │ • 模型分发   │  │ • 数据标注 │ ║  │
│  ║  │ • 超参调优   │  │ • A/B测试    │  │ • 数据分析 │ ║  │
│  ║  └──────────────┘  └──────────────┘  └────────────┘ ║  │
│  ║                                                        ║  │
│  ╚═══════════════════════════════════════════════════════╝  │
│                          ↕↕↕                              │
│  ╔═══════════════════════════════════════════════════════╗  │
│  ║                    Aurora Edge                       ║  │
│  ╠═══════════════════════════════════════════════════════╣  │
│  ║                                                        ║  │
│  ║  ┌─────────────────────────────────────────────────┐ ║  │
│  ║  │    Aurora-Edge-Runtime (本项目 - 核心产品)      │ ║  │
│  ║  │    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │ ║  │
│  ║  │                                                 │ ║  │
│  ║  │    智能数据采集 Runtime                          │ ║  │
│  ║  │    • RL驱动的路径规划                           │ ║  │
│  ║  │    • 步态级精准触发                             │ ║  │
│  ║  │    • 实时数据价值评估                           │ ║  │
│  ║  │    • 云边协同闭环                               │ ║  │
│  ║  │                                                 │ ║  │
│  ║  └─────────────────────────────────────────────────┘ ║  │
│  ║                                                        ║  │
│  ║  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ ║  │
│  ║  │ Aurora-      │  │ Aurora-      │  │ Aurora-    │ ║  │
│  ║  │ Simulation   │  │ Edge-        │  │ OTA        │ ║  │
│  ║  │              │  │ Monitoring   │  │ Manager    │ ║  │
│  ║  │ • MuJoCo仿真 │  │              │  │            │ ║  │
│  ║  │ • Isaac仿真  │  │ • 性能监控   │  │ • 模型更新 │ ║  │
│  ║  │ • Sim2Real   │  │ • 日志分析   │  │ • 配置下发 │ ║  │
│  ║  └──────────────┘  └──────────────┘  └────────────┘ ║  │
│  ║                                                        ║  │
│  ╚═══════════════════════════════════════════════════════╝  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Aurora-Edge-Runtime 在矩阵中的角色

#### 2.2.1 数据流架构

```
┌─────────────────────────────────────────────────────────────┐
│              Aurora 数据闭环流程                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1️⃣ 数据采集层 (Aurora-Edge-Runtime)                       │
│     ┌─────────────────────────────────────────────────┐    │
│     │ 机器人硬件 ──→ 智能规划 ──→ 精准触发 ──→ 数据录制 │    │
│     │    ↓             ↓           ↓            ↓       │    │
│     │  传感器数据   RL路径    步态检测   Rosbag2      │    │
│     └─────────────────────────────────────────────────┘    │
│                        ↓ S3上传                            │
│                                                             │
│  2️⃣ 数据管理层 (Aurora-Data-Lake)                         │
│     ┌─────────────────────────────────────────────────┐    │
│     │ 数据接收 → 存储/索引 → 质量评估 → 数据标注       │    │
│     └─────────────────────────────────────────────────┘    │
│                        ↓ 训练数据集                         │
│                                                             │
│  3️⃣ 模型训练层 (Aurora-Cloud-Trainer)                    │
│     ┌─────────────────────────────────────────────────┐    │
│     │ PPO训练 → 策略优化 → 模型评估 → 版本管理         │    │
│     └─────────────────────────────────────────────────┘    │
│                        ↓ ONNX导出                          │
│                                                             │
│  4️⃣ 模型分发层 (Aurora-Model-Hub)                         │
│     ┌─────────────────────────────────────────────────┐    │
│     │ 模型仓库 → 版本管理 → 灰度发布 → 监控回滚         │    │
│     └─────────────────────────────────────────────────┘    │
│                        ↓ MQTT推送                           │
│                                                             │
│  5️⃣ 模型部署层 (Aurora-OTA-Manager + Edge-Runtime)       │
│     ┌─────────────────────────────────────────────────┐    │
│     │ 模型下载 → 配置更新 → 热重载 → 验证生效          │    │
│     └─────────────────────────────────────────────────┘    │
│                        ↓ 回到步骤1                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### 2.2.2 与其他产品的接口

| 上游产品 | 接口类型 | 数据流 | Aurora-Edge-Runtime |
|---------|---------|-------|---------------------|
| **Aurora-Cloud-Trainer** | MQTT/S3 | ONNX模型下载 | ✅ 接收并热加载 |
| **Aurora-Model-Hub** | REST API | 模型元数据 | ✅ 版本管理 |
| **Aurora-OTA-Manager** | MQTT | 配置下发 | ✅ 配置热更新 |

| 下游产品 | 接口类型 | 数据流 | Aurora-Edge-Runtime |
|---------|---------|-------|---------------------|
| **Aurora-Data-Lake** | S3 API | Rosbag2上传 | ✅ 分片上传 |
| **Aurora-Cloud-Trainer** | S3/MQTT | 采集元数据上报 | ✅ 轻量级反馈 |

### 2.3 Aurora-Edge-Runtime 独特价值

在 Aurora 产品矩阵中，Aurora-Edge-Runtime 承担着**"数据生产者"**的核心角色：

| 能力 | 说明 | 价值 |
|-----|------|-----|
| **智能采集** | RL驱动的动态路径规划 | 提高数据价值密度 |
| **精准触发** | 步态级触发机制 | 减少冗余数据82% |
| **端侧推理** | ONNX <10ms延迟 | 实时决策，低资源占用 |
| **云边协同** | 自动闭环，模型热更新 | 加速迭代周期 |
| **多模态支持** | Auto/Humanoid双模式 | 覆盖多种机器人形态 |

---

## 3. 产品架构

### 3.1 系统架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Aurora Cloud (云端)                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                   │
│  │ Aurora-      │  │ Aurora-      │  │ Aurora-      │                   │
│  │ Cloud-       │  │ Model-       │  │ Data-        │                   │
│  │ Trainer      │  │ Hub          │  │ Lake (S3)    │                   │
│  │              │  │              │  │              │                   │
│  │ • PPO训练    │  │ • 模型版本   │  │ • 数据存储   │                   │
│  │ • 策略优化   │  │ • 模型分发   │  │ • 数据索引   │                   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘                   │
│         │                 │                 │                          │
│         └─────────────────┴─────────────────┘                          │
│                              ↕ S3 + MQTT                              │
└─────────────────────────────────────────────────────────────────────────┘
                                   ↕
┌─────────────────────────────────────────────────────────────────────────┐
│                     Aurora Edge (边缘设备)                             │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │              Aurora-Edge-Runtime (核心)                           │  │
│  │                                                                   │  │
│  │  ┌─────────────────────────────────────────────────────────┐    │  │
│  │  │           DataCollectionPlanner (主控制器)               │    │  │
│  │  │  ┌────────────┐  ┌────────────┐  ┌──────────────┐       │    │  │
│  │  │  │ RL/        │  │ Data       │  │ Path         │       │    │  │
│  │  │  │ Humanoid   │  │ Manager    │  │ Visualizer   │       │    │  │
│  │  │  │ Planner    │  │            │  │              │       │    │  │
│  │  │  └─────┬──────┘  └─────┬──────┘  └──────┬───────┘       │    │  │
│  │  │        └───────────────────┴───────────────────┘        │    │  │
│  │  └──────────────────────────────────────────────────────────┘    │  │
│  │                          ↓                                       │  │
│  │  ┌─────────────────────────────────────────────────────────┐    │  │
│  │  │                   数据采集层                             │    │  │
│  │  │  ┌────────────┐  ┌────────────┐  ┌──────────────┐       │    │  │
│  │  │  │ Gait       │  │ Data       │  │ Collection   │       │    │  │
│  │  │  │ Trigger    │  │ Collection │  │ Feedback     │       │    │  │
│  │  │  │ (步态触发) │  │ Executor   │  │              │       │    │  │
│  │  │  └────────────┘  └─────┬──────┘  └──────────────┘       │    │  │
│  │  │                        ↓                                 │    │  │
│  │  │  ┌──────────────────────────────────────────────────┐   │    │  │
│  │  │  │    Ring Buffer (15s pre + 5s post)               │   │    │  │
│  │  │  └──────────────────────────────────────────────────┘   │    │  │
│  │  └──────────────────────────────────────────────────────────┘    │  │
│  │                          ↓                                       │  │
│  │  ┌─────────────────────────────────────────────────────────┐    │  │
│  │  │                   云端协同层                             │    │  │
│  │  │  ┌────────────┐  ┌────────────┐  ┌──────────────┐       │    │  │
│  │  │  │ AWS S3     │  │ MQTT       │  │ Config       │       │    │  │
│  │  │  │ Uploader   │  │ Client     │  │ Watcher      │       │    │  │
│  │  │  └────────────┘  └────────────┘  └──────────────┘       │    │  │
│  │  └──────────────────────────────────────────────────────────┘    │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                  ↕ ROS2 Topics                        │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                    机器人硬件/仿真器                              │  │
│  │                                                                   │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌──────────┐  │  │
│  │  │ /robot/    │  │ /joint_    │  │ /robot/    │  │ /robot/  │  │  │
│  │  │ odom       │  │ states     │  │ imu        │  │ cmd_vel │  │  │
│  │  │ (50Hz)     │  │ (50Hz)     │  │ (50Hz)     │  │ (50Hz)   │  │  │
│  │  └────────────┘  └────────────┘  └────────────┘  └──────────┘  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                      数据存储层                                   │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐                  │  │
│  │  │ Rosbag2    │  │ 加密存储   │  │ 脱敏数据   │                  │  │
│  │  │ /bags      │  │ /enc       │  │ /masked    │                  │  │
│  │  └────────────┘  └────────────┘  └────────────┘                  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 核心组件

#### 3.2.1 DataCollectionPlanner（主控制器）

**职责**: 系统总控制器，协调所有组件完成数据采集任务

```cpp
class DataCollectionPlanner {
public:
    // 系统初始化
    bool initialize();

    // 规划采集任务（根据模式选择规划器）
    std::vector<Point> planDataCollectionMission();
    std::vector<Point> planHumanoidMission(const HumanoidStateInfo& state);

    // 执行采集（带反馈循环）
    void executeWithFeedback(const std::vector<Point>& path);
    void executeHumanoidWithFeedback(const std::vector<Point>& path,
                                     const HumanoidStateInfo& state);

    // 数据管理
    void updateWithNewData(const std::vector<DataPoint>& new_data);
    void addHumanoidDataPoint(const DataPointMetadata& metadata);

    // 云端协同
    void uploadCollectedData();
    RewardStats getLearningStats() const;

    // 可视化
    void setVisualizationEnabled(bool enabled);
    void setVisualizationFrame(const std::string& frame_id);

private:
    PlannerMode planner_mode_;                    // 运行模式
    std::unique_ptr<AutoPlanner> auto_planner_;       // Auto模式规划器
    std::unique_ptr<HumanoidPlanner> humanoid_planner_;    // Humanoid模式规划器
    std::unique_ptr<DataManager> data_manager_;   // 数据管理
    std::unique_ptr<DataCollectionExecutor> executor_;  // 采集执行
    std::unique_ptr<GaitTrigger> trigger_;        // 步态触发器
    std::unique_ptr<CollectionFeedback> feedback_; // 采集反馈
    std::unique_ptr<AwsDataUploader> uploader_;   // 云端上传
    std::unique_ptr<ConfigWatcher> config_watcher_; // 配置热更新
    std::unique_ptr<PathVisualizer> visualizer_;  // 路径可视化
};
```

**关键特性**:
- 支持双模式切换（Auto/Humanoid）
- 事件驱动状态机
- 反馈循环优化
- 配置热更新

#### 3.2.2 HumanoidPlanner（人形机器人规划器）

**职责**: 基于强化学习的43维状态空间路径规划

```cpp
class HumanoidPlanner {
public:
    // 规划动作
    HumanoidAction planAction(const HumanoidStateInfo& state);

    // 评估数据价值
    DataValueResult evaluateLocationValue(double x, double y,
                                         SceneType scene_type);

    // 覆盖率管理
    double getCoverageRate() const;
    void updateCostMap(const std::vector<DataPoint>& new_data);

private:
    // ONNX Runtime 推理引擎
    std::unique_ptr<ONNXRuntime> onnx_runtime_;

    // 43维状态空间 (对齐训练侧 humanoid_nav_data_training.yaml)
    // [3]  基座线速度 (vx, vy, vz)
    // [3]  基座角速度 (wx, wy, wz)
    // [2]  归一化位置 (x/W, y/H)
    // [2]  朝向 (sinθ, cosθ)
    // [2]  目标方向 (sinΔθ, cosΔθ)
    // [3]  目标距离 (Δx, Δy, ‖Δ‖)
    // [8]  数据价值扇区 (8方向)
    // [4]  障碍物扇区 (4方向)
    // [2]  当前位置价值 (value, rarity)
    // [2]  采集状态 (ratio, coverage)
    // [1]  地形类型
    // [1]  障碍密度
    // [1]  步态相位 sin(2π·φ)
    // [8]  动作历史 (8步 forward_vel)
    // [1]  剩余预算

    // 3维连续动作空间
    // [1] forward_vel  [-0.3, 0.6] m/s
    // [1] lateral_vel  [-0.3, 0.3] m/s
    // [1] angular_vel  [-0.3, 0.3] rad/s
};
```

**性能指标**:
| 指标 | 数值 | 说明 |
|-----|------|-----|
| 状态维度 | 43 | 端云协同状态感知 |
| 动作维度 | 3 | 连续速度命令 |
| 推理延迟 | <10ms | ONNX CPU推理 |
| 内存占用 | <100MB | 模型+运行时 |

#### 3.2.3 GaitTrigger（步态触发器）

**职责**: 基于实际步态相位的精准采集触发

```cpp
class GaitTrigger : public rclcpp::Node {
public:
    // 设置事件回调
    void setEventCallback(GaitEventCallback callback);

    // 检查是否应该触发采集
    bool shouldTriggerCollection(
        const Point& current_pos,
        const Point& last_collect_pos,
        const std::chrono::steady_clock::time_point& last_collect_time
    );

    // 获取足迹历史
    Footprint getLastStableFootprint() const;
    std::vector<Footprint> getFootprints() const;

private:
    // ROS2 订阅
    void odomCallback(const nav_msgs::msg::Odometry::SharedPtr msg);
    void jointCallback(const sensor_msgs::msg::JointState::SharedPtr msg);

    // 步态分析
    void analyzeGaitState();
    bool isInStablePhase() const;           // 检查稳定支撑相
    void detectFootstrike();                // 检测足端着地

    // 触发条件
    double min_step_distance_ = 0.15;       // 最小步长 15cm
    double min_collection_interval_ = 1.0;  // 最小采集间隔 1s
    double stable_phase_threshold_ = 0.3;   // 稳定相位阈值

    // 足迹历史（去重）
    std::vector<Footprint> footprints_;
    static constexpr size_t MAX_FOOTPRINTS = 1000;
};
```

**核心原则**:
> **采集基于实际足端落点，而非规划路径点**

**触发条件**:
```cpp
触发 = 1. 足端着地检测 (z高度变化)
     && 2. 稳定支撑相 (双脚支撑且质心稳定，支撑相中间60%)
     && 3. 步长 ≥ 15cm (避免原地踏步)
     && 4. 采集间隔 ≥ 1s (避免过于密集)
     && 5. 非重复足迹 (1000点历史去重，半径0.1m)
```

**验证数据** (v1.1.2):
| 指标 | 数值 | 说明 |
|-----|------|-----|
| 过滤率 | 82% | 90/501路径点被过滤 |
| 步长范围 | 0.28-1.36m | 调整步到正常行走 |
| 采集间隔 | ~1.0s | 符合配置预期 |
| 去重准确率 | 100% | 零重复采集点 |

#### 3.2.4 DataManager（数据管理器）

**职责**: 代价地图维护与数据价值评估

```cpp
class DataManager {
public:
    // 更新代价地图
    void updateCostMap(const std::vector<DataPoint>& new_data);

    // 评估数据价值
    DataValueResult evaluateLocationValue(double x, double y,
                                         SceneType scene_type);

    // 覆盖率统计
    double getCoverageRate() const;
    double getSparseCoverage() const;

    // 稀疏区域检测
    std::vector<Point> getSparseRegions(double threshold = 0.15);

private:
    // 2D网格代价地图
    struct CostMap {
        double resolution;           // 网格分辨率 (1m)
        int width, height;           // 地图尺寸
        std::vector<std::vector<double>> density;  // 数据密度
        std::vector<std::vector<SceneType>> scene; // 场景类型
    } cost_map_;

    // 数据价值评估权重
    struct ValueWeights {
        double w_spatial_rarity = 0.3;      // 空间稀缺性
        double w_temporal_freshness = 0.15; // 时间新鲜度
        double w_scene_diversity = 0.2;     // 场景多样性
        double w_quality = 0.15;            // 数据质量
        double w_coverage = 0.2;            // 覆盖率
    } weights_;
};
```

**数据价值评分公式**:
```
ValueScore = 0.3 × SpatialRarity
           + 0.15 × TemporalFreshness
           + 0.2 × SceneDiversity
           + 0.15 × QualityScore
           + 0.2 × CoverageRate

其中:
- SpatialRarity = 1 - (local_density / max_density)
- TemporalFreshness = exp(-Δt / τ)  (τ = 24小时)
- SceneDiversity = SceneRarityMap[scene_type]
- QualityScore = f(stability, completeness, noise_level)
- CoverageRate = collected_area / total_area
```

#### 3.2.5 DataCollectionExecutor（采集执行器）

**职责**: 环形缓冲录制与触发保存

```cpp
class DataCollectionExecutor {
public:
    // 开始环形录制
    void startRecording(const std::vector<TopicSubscription>& topics);

    // 触发保存（保存触发点前后数据）
    std::string triggerSave(const TriggerInfo& info);

    // 停止录制
    void stopRecording();

private:
    // 环形缓冲实现
    struct RingBuffer {
        size_t capacity;              // 缓冲容量（秒）
        size_t forward_duration;      // 前向时长 (10-15s)
        size_t backward_duration;     // 后向时长 (5s)
        std::deque<Message> buffer;   // 消息队列
    } ring_buffer_;

    // Rosbag2 录制器
    std::unique_ptr<rosbag2_cpp::Writer> bag_writer_;

    // 冷却管理
    std::chrono::steady_clock::time_point last_trigger_time_;
    double cooldown_duration_;  // 默认10s
};
```

**录制策略**:
```
时间轴:
  ←─────── 前向录制 ──────→ [触发点] ←── 后向录制 ─→
  │                        │                     │
  15s前                   现在                  5s后

环形缓冲:
  持续录制传感器数据 → 循环覆盖 → 触发时保存窗口
```

#### 3.2.6 AwsDataUploader（云端上传器）

**职责**: S3分片上传与断点续传

```cpp
class AwsDataUploader {
public:
    // 上传单个文件
    bool uploadFile(const std::string& local_path,
                   const std::string& s3_key);

    // 批量上传
    int uploadBatch(const std::vector<std::string>& files);

    // 查询上传状态
    UploadStatus getUploadStatus(const std::string& file_id);

private:
    // 分片上传
    struct MultipartUpload {
        std::string upload_id;
        std::string bucket;
        std::string key;
        std::vector<Part> parts;
        size_t part_size;  // 50MB
    };

    // 重试机制
    int max_retry_count_ = 3;
    std::chrono::seconds retry_interval_{10};

    // AWS S3 客户端
    std::shared_ptr<Aws::S3::S3Client> s3_client_;
};
```

**上传参数** (app_config.json):
```json
{
  "dataUpload": {
    "retryCount": 3,
    "retryIntervalSec": 10,
    "uploadFileSliceSizeMb": 50,
    "batchSize": 1000,
    "aws": {
      "enabled": true,
      "bucketName": "caic-dataset",
      "endpointUrl": "orderseek-obs.orderseek.ai",
      "region": "US"
    }
  }
}
```

#### 3.2.7 ConfigWatcher（配置热更新）

**职责**: 监控配置文件变化并热重载

```cpp
class ConfigWatcher {
public:
    // 添加监控文件
    void watchFile(const std::string& file_path,
                   std::function<void(const std::string&)> callback);

    // 启动监控
    void start();

private:
    // inotify 监控
    int inotify_fd_;
    std::unordered_map<int, std::string> watch_descriptors_;

    // 防抖动
    std::chrono::milliseconds debounce_delay_{500};
};
```

**热更新流程**:
```
1. 检测文件变化 (inotify)
2. 防抖动延迟 (500ms)
3. 解析新配置
4. 验证合法性
5. 原子替换
6. 应用新参数
总耗时: <1s
```

### 3.3 状态机设计

```
                    ┌─────────────────┐
                    │  INITIALIZING   │
                    │                 │
                    │ • 加载ONNX模型  │
                    │ • 初始化ROS2    │
                    │ • 连接云端      │
                    └────────┬────────┘
                             │ INIT_COMPLETE
                             ▼
                    ┌─────────────────┐
         ┌─────────│      IDLE       │─────────┐
         │         │                 │         │
         │         │ • 等待任务      │         │
         │         │ • 监控配置变化  │         │
         │         └────────┬────────┘         │
         │                  │ PLAN_REQUEST     │
         │                  ▼                  │
         │         ┌─────────────────┐         │
         │         │    PLANNING     │         │
         │         │                 │         │
         │         │ • RL推理        │         │
         │         │ • 路径生成      │         │
         │         │ • 价值评估      │         │
         │         └────────┬────────┘         │
         │                  │ PLAN_COMPLETE    │
         │                  ▼                  │
         │         ┌─────────────────┐         │
         │         │   NAVIGATING    │         │
         │         │                 │         │
         │         │ • 机器人导航    │         │
         │         │ • 步态监控      │         │
         │         └────────┬────────┘         │
         │                  │ TRIGGER_CONDITION│
         │                  ▼                  │
         │         ┌─────────────────┐         │
         │         │ DATA_COLLECTION │         │
         │         │                 │         │
         │         │ • 触发验证      │         │
         │         │ • Ring Buffer   │         │
         │         │ • 保存Rosbag    │         │
         │         └────────┬────────┘         │
         │                  │ DATA_COLLECTED   │
         │                  ▼                  │
         │         ┌─────────────────┐         │
         │         │   UPLOADING     │         │
         │         │                 │         │
         │         │ • S3分片上传    │         │
         │         │ • 元数据上报    │         │
         │         └────────┬────────┘         │
         │                  │ UPLOAD_COMPLETE  │
         │                  └───┐              │
         │                      │              │
         │           ┌──────────▼──────────┐   │
         └───────────│    IDLE (replan)    │───┘
                     │                     │
                     │ • 更新代价地图      │
                     │ • 计算反馈奖励      │
                     │ • 触发重规划        │
                     └─────────────────────┘
                              │
                    ERROR_OCCURRED │ SHUTDOWN_REQUEST
                              ▼
                    ┌─────────────────┐
                    │  ERROR /        │
                    │ SHUTTING_DOWN   │
                    │                 │
                    │ • 保存现场      │
                    │ • 资源清理      │
                    │ • 上传日志      │
                    └─────────────────┘
```

**状态超时配置**:
| 状态 | 超时时间 | 超时处理 |
|-----|---------|---------|
| INITIALIZING | 30s | 报错，进入ERROR |
| PLANNING | 60s | 使用上次路径，进入NAVIGATING |
| NAVIGATING | 120s/waypoint | 跳过当前点，进入下一个 |
| DATA_COLLECTION | 20s/trigger | 放弃本次采集 |
| UPLOADING | 300s | 标记失败，本地缓存 |

---

## 4. 功能规格

### 4.1 功能清单

| 功能ID | 功能名称 | 优先级 | 状态 | 说明 |
|-------|---------|-------|------|-----|
| **核心功能** |
| F001 | 智能路径规划 | P0 | ✅ | RL驱动的动态路径规划 |
| F002 | 步态触发采集 | P0 | ✅ | 基于实际落点的精准触发 |
| F003 | 环形缓冲录制 | P0 | ✅ | 15s预录+5s后录 |
| F004 | 数据价值评估 | P0 | ✅ | 多维度价值评分 |
| F005 | AWS S3上传 | P0 | ✅ | 分片上传+断点续传 |
| **增强功能** |
| F006 | 配置热更新 | P1 | ✅ | 无需重启的配置更新 |
| F007 | 路径可视化 | P1 | ✅ | RViz2实时显示 |
| F008 | 系统监控 | P1 | ✅ | 日志+状态上报 |
| F009 | 双模式支持 | P1 | ✅ | Auto/Humanoid切换 |
| F010 | 元数据上报 | P1 | ✅ | 轻量级反馈（~20B） |
| **高级功能** |
| F011 | 多机器人协同 | P2 | 🔲 | v1.2.0规划 |
| F012 | 实时模型更新 | P2 | 🔲 | v1.2.0规划 |
| F013 | Web监控界面 | P2 | 🔲 | v1.2.0规划 |

### 4.2 核心功能详解

#### F001: 智能路径规划

**功能描述**: 基于强化学习（PPO）的动态路径规划，根据数据密度和场景语义优化采集路径

**输入参数**:
```cpp
struct PlanningInput {
    Point start;                  // 起始位置
    Point goal;                   // 目标位置
    MissionArea boundary;         // 任务边界
    CostMap cost_map;             // 当前代价地图
    HumanoidStateInfo robot_state; // 机器人状态
};
```

**输出结果**:
```cpp
struct PlanningOutput {
    std::vector<Point> waypoints;  // 路径点序列
    double expected_reward;        // 预期奖励
    double estimated_time;         // 预计用时
    std::string reasoning;         // 规划理由
};
```

**性能指标**:
| 指标 | 目标值 | 实测值 |
|-----|-------|-------|
| 推理延迟 | <20ms | 8-12ms |
| 路径长度 | 最优±15% | 符合 |
| 覆盖率 | >90%/1000步 | 92% |
| 碰撞率 | <1% | 0.5% |

**算法流程**:
```
1. 状态观测 (43维)
   ├─ 基座速度 (线速度 + 角速度)
   ├─ 位置朝向 + 目标信息
   ├─ 代价地图 (数据价值扇区 + 障碍距离)
   └─ 采集状态 + 步态相位

2. ONNX推理
   ├─ 输入: 43维状态向量
   ├─ 模型: humanoid_ppo.onnx
   ├─ 输出: 3维连续速度命令 + 价值估计
   └─ 延迟: <10ms

3. 速度命令
   ├─ forward_vel [-0.3, 0.6] m/s
   ├─ lateral_vel [-0.3, 0.3] m/s
   └─ angular_vel [-0.3, 0.3] rad/s

4. 价值评估
   ├─ 数据价值评分
   ├─ 预期奖励估计
   └─ 覆盖率预测
```

#### F002: 步态触发采集

**功能描述**: 基于实际步态相位的精准采集触发，确保数据采集在稳定支撑相进行

**触发条件**:
```cpp
bool shouldTrigger = 1. detectFootstrike()           // 足端着地
                 && 2. isInStableStancePhase()       // 稳定支撑相
                 && 3. validateStepDistance()        // 步长≥15cm
                 && 4. checkCollectionInterval()     // 间隔≥1s
                 && 5. isNotDuplicateFootprint();    // 非重复足迹
```

**步态相位检测**:
```
相位定义:
  0° ────→ 360° (一个完整步态周期)

  摆动相 (Swing Phase): 0° - 180°
    ├─ 起摆 (0° - 60°)
    ├─ 摆动 (60° - 120°)
    └─ 落地准备 (120° - 180°)

  支撑相 (Stance Phase): 180° - 360°
    ├─ 早期 (180° - 240°)
    ├─ 中期稳定期 (240° - 300°) ← 采集窗口
    └─ 晚期 (300° - 360°)

稳定期判定:
  - 双脚支撑
  - 质心在支撑多边形内
  - ZMP稳定
  - 相位在 [240°, 300°]
```

**足迹去重**:
```cpp
bool isDuplicate(const Footprint& new_fp) {
    for (const auto& fp : footprints_) {
        double dist = sqrt(pow(new_fp.x - fp.x, 2) +
                          pow(new_fp.y - fp.y, 2));
        if (dist < 0.1) {  // 10cm半径
            return true;
        }
    }
    return false;
}
```

**验证数据** (v1.1.2):
```
测试场景: 501个规划路径点
采集结果: 90次触发
过滤率:   82%

步长分布:
  - 最小: 0.28m (调整步)
  - 最大: 1.36m (快速行走)
  - 平均: 0.58m

采集间隔:
  - 最小: 1.0s
  - 最大: 1.4s
  - 平均: 1.15s

去重效果:
  - 重复检测: 5次
  - 成功过滤: 5次
  - 准确率: 100%
```

#### F003: 环形缓冲录制

**功能描述**: 持续录制传感器数据到环形缓冲，触发时保存触发点前后的数据窗口

**缓冲配置**:
```json
{
  "cacheMode": {
    "forwardCaptureDurationSec": 15,   // 前向15秒
    "backwardCaptureDurationSec": 5,   // 后向5秒
    "cooldownDurationSec": 10           // 冷却10秒
  }
}
```

**录制流程**:
```
时间轴演示:

  ←─────── 前向缓冲 (15s) ─────→ [触发] ←─ 后向录制 (5s) ─→
  │                              │                        │
  T-15s                         T0                       T+5s

  数据流:
  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
  │ msg1│ msg2│ msg3│ ... │msgN │trig │msgN+1│ ... │
  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
    ↑                                       ↑
  持续写入                        触发后继续写5s
  循环覆盖                        然后保存窗口

  保存内容:
  - Rosbag文件: dcp_YYYYMMDD_HHMMSS.db3
  - 元数据: trigger_info.json
  - 大小: ~250MB (50Hz × 20s × 12通道)
```

**录制话题** (robot_data_collection.json):
```json
{
  "channels": {
    "cyclone": [
      {
        "topic": "/robot/odom",
        "type": "nav_msgs/msg/Odometry",
        "originalFrameRate": 50,
        "capturedFrameRate": 50
      },
      {
        "topic": "/joint_states",
        "type": "sensor_msgs/msg/JointState",
        "originalFrameRate": 50,
        "capturedFrameRate": 50
      },
      {
        "topic": "/robot/imu",
        "type": "sensor_msgs/msg/Imu",
        "originalFrameRate": 50,
        "capturedFrameRate": 50
      },
      {
        "topic": "/robot/cmd_vel",
        "type": "geometry_msgs/msg/Twist",
        "originalFrameRate": 50,
        "capturedFrameRate": 50
      }
    ]
  }
}
```

#### F004: 数据价值评估

**功能描述**: 多维度评估采集数据的价值，指导路径规划优先级

**评估维度**:
```cpp
struct DataValueScore {
    // 1. 空间稀缺性 (30%)
    double spatial_rarity;    // = 1 - local_density / max_density

    // 2. 时间新鲜度 (15%)
    double temporal_freshness; // = exp(-Δt / 24h)

    // 3. 场景多样性 (20%)
    double scene_diversity;   // = SceneRarityMap[scene_type]

    // 4. 数据质量 (15%)
    double quality;           // = f(stability, completeness, noise)

    // 5. 覆盖率贡献 (20%)
    double coverage_contribution; // = new_area / total_area

    // 综合评分
    double total = 0.3 * spatial_rarity
                + 0.15 * temporal_freshness
                + 0.2 * scene_diversity
                + 0.15 * quality
                + 0.2 * coverage_contribution;
};
```

**场景稀有度映射**:
```yaml
scene_rarity:
  indoor_flat: 0.3      # 室内平地（常见）
  indoor_stair: 0.7     # 室内楼梯（稀有）
  indoor_ramp: 0.6      # 室内坡道（较稀有）
  outdoor_flat: 0.4     # 户外平地（较常见）
  outdoor_rough: 0.8    # 户外崎岖（稀有）
  outdoor_slope: 0.7    # 户外斜坡（较稀有）
  mixed: 0.9            # 混合场景（最稀有）
```

**质量评估**:
```cpp
double assessQuality(const CollectionData& data) {
    double stability = assessStability(data);
    double completeness = assessCompleteness(data);
    double noise_level = assessNoise(data);

    return 0.5 * stability
         + 0.3 * completeness
         + 0.2 * (1.0 - noise_level);
}

double assessStability(const CollectionData& data) {
    // 检查躯干摆动
    double trunk_sway = max(abs(data.trunk_angle_pitch)),
                          abs(data.trunk_angle_roll));

    // 检查质心偏移
    double com_offset = data.com_deviation;

    // 稳定性评分
    if (trunk_sway < 0.1 && com_offset < 0.05) return 1.0;
    else if (trunk_sway < 0.2 && com_offset < 0.1) return 0.7;
    else return 0.4;
}
```

#### F005: AWS S3上传

**功能描述**: 分片上传Rosbag数据到AWS S3兼容对象存储，支持断点续传

**上传流程**:
```
1. 文件准备
   ├─ 检查文件完整性
   ├─ 计算MD5校验和
   └─ 生成S3 key

2. 分片上传
   ├─ 初始化多部分上传 (Initiate Multipart Upload)
   ├─ 上传分片 (Upload Part)
   │  └─ 每片50MB, 并发3片
   ├─ 完成上传 (Complete Multipart Upload)
   └─ 失败时取消 (Abort Multipart Upload)

3. 重试机制
   ├─ 单片失败: 重试3次
   ├─ 全部失败: 标记为PENDING
   └─ 下次周期重试

4. 元数据上报
   ├─ 上传到MQTT: /SHADOW/sd@{VIN}/.../up
   └─ 包含: collection_id, s3_key, reward, etc.
```

**S3 Key规范**:
```
{bucket}/{prefix}/{date}/{file}_{timestamp}.db3

示例:
caic-dataset/xc/humanoid/2026-03-07/dcp_20260307_143022.db3

组成部分:
- bucket: caic-dataset
- prefix: xc (用户标识)
- type: humanoid (采集类型)
- date: 2026-03-07 (日期)
- file: dcp_20260307_143022 (文件名)
```

**上传状态管理** (upload_record.json):
```json
{
  "upload_records": [
    {
      "collection_id": "col_001",
      "local_path": "/data/aer/bags/dcp_20260307_143022.db3",
      "s3_key": "caic-dataset/xc/humanoid/2026-03-07/dcp_20260307_143022.db3",
      "file_size": 268435456,
      "upload_status": "uploaded",
      "upload_time": "2026-03-07T14:35:00Z",
      "retry_count": 0,
      "md5": "abc123...",
      "metadata": {
        "trigger_type": "gait_footstrike",
        "position": {"x": 1.23, "y": 4.56},
        "reward": 0.85,
        "scene_type": "indoor_flat"
      }
    }
  ]
}
```

### 4.3 运行模式

#### 4.3.1 Auto模式（自动驾驶）

**适用场景**:
- 自动驾驶车辆道路测试
- 测试场数据采集
- 城市道路导航

**技术特点**:
```yaml
观测空间: 25维
  ├─ 机器人状态: 3 (x, y, yaw)
  ├─ 目标位置: 3 (goal_x, goal_y, goal_yaw)
  ├─ 速度信息: 2 (v, omega)
  ├─ 传感器融合: 6 (lidar + camera features)
  ├─ 语义约束: 4 (traffic, building, pedestrian, road)
  ├─ 代价地图: 3 (density, obstacle, semantic)
  ├─ 历史轨迹: 3 (past_pos × 1)
  └─ 可达性: 1 (reachability)

动作空间: 4维离散
  ├─ FORWARD: 向前移动
  ├─ LEFT: 向左移动
  ├─ RIGHT: 向右移动
  └─ BACKWARD: 向后移动

推理模型: auto_ppo.onnx
推理延迟: <5ms
```

**语义约束** (planner_weights.yaml):
```yaml
auto:
  semantic_constraints:
    traffic_light_penalty: 50.0      # 红绿灯违规惩罚
    building_penalty: 30.0           # 靠近建筑惩罚
    pedestrian_crossing_penalty: 20.0 # 人行横道惩罚
    road_preference: -10.0           # 道路偏好（负奖励）
```

#### 4.3.2 Humanoid模式（人形机器人）

**适用场景**:
- 人形机器人室内导航
- 户外复杂地形采集
- 步态控制优化

**技术特点**:
```yaml
观测空间: 43维
  ├─ 基座速度: 6 (lin_vel 3 + ang_vel 3)
  ├─ 位置朝向: 4 (norm_pos 2 + heading 2)
  ├─ 目标信息: 5 (direction 2 + distance 3)
  ├─ 数据价值: 8 (8方向扇区扫描)
  ├─ 障碍距离: 4 (4方向射线检测)
  ├─ 当前价值: 2 (value + rarity)
  ├─ 采集状态: 2 (collected_ratio + coverage)
  ├─ 环境: 2 (terrain_type + obstacle_density)
  ├─ 步态相位: 1 (sin(2π·φ))
  ├─ 动作历史: 8 (最近8步 forward_vel)
  └─ 剩余预算: 1

动作空间: 3维连续
  ├─ forward_vel: [-0.3, 0.6] m/s
  ├─ lateral_vel: [-0.3, 0.3] m/s
  └─ angular_vel: [-0.3, 0.3] rad/s

推理模型: humanoid_ppo.onnx
推理延迟: <10ms
```

**步态参数** (robot_simulator.cpp):
```cpp
struct GaitParameters {
    double step_length = 0.25;      // 步长 25cm
    double step_height = 0.05;      // 步高 5cm
    double step_duration = 0.8;     // 步周期 0.8s
    double duty_factor = 0.6;       // 占空比 60%
    double double_support_time = 0.1; // 双脚支撑时间 0.1s
};
```

**机器人几何** (robot_simulator.cpp):
```cpp
struct RobotGeometry {
    double upper_leg_length = 0.35;  // 大腿长 35cm
    double lower_leg_length = 0.35;  // 小腿长 35cm
    double hip_width = 0.10;         // 髋宽 10cm
    int num_joints_per_leg = 6;      // 每腿6关节
};
```

#### 4.3.3 模式切换

**配置文件切换**:
```bash
# 方式1: 编辑配置文件
vim ops/prod/aer.conf
  AER_MODE=auto        # 自动驾驶模式
  DCP_MODE=humanoid    # 人形机器人模式

# 方式2: 环境变量
export DCP_MODE=auto
./build/src/aer

# 方式3: 命令行参数
./build/src/aer --mode auto
./build/src/aer --mode humanoid
```

**模式差异对比**:
| 特性 | Auto模式 | Humanoid模式 |
|-----|---------|-------------|
| 观测维度 | 25 | 43 |
| 动作维度 | 4 (离散) | 3 (连续) |
| 触发方式 | 场景触发 | 步态触发 |
| 控制频率 | 20Hz | 10Hz |
| 推理延迟 | <5ms | <10ms |
| 内存占用 | <50MB | <150MB |
| 关键技术 | 语义导航 | 步态控制 |

---

## 5. 技术规格

### 5.1 系统要求

#### 5.1.1 硬件要求

| 组件 | 最低配置 | 推荐配置 | 说明 |
|-----|---------|---------|------|
| **CPU** | 4核 ARM64/x86_64 @ 1.5GHz | 8核 ARM64/x86_64 @ 2.0GHz | 支持NEON/SSE4.2 |
| **内存** | 4GB | 8GB | 包含OS开销 |
| **存储** | 50GB 可用空间 | 200GB SSD | Rosbag数据占用大 |
| **网络** | 1Mbps 上行 | 10Mbps 上行 | 用于S3上传 |
| **传感器** | 里程计 + 编码器 | + IMU + 视觉 | 可选 |

**推荐平台**:
- **边缘计算**: NVIDIA Jetson系列, Raspberry Pi 4, Intel NUC
- **工控机**: 带有ROS2支持的x86_64工控机
- **开发机**: Ubuntu 20.04+ PC/笔记本

#### 5.1.2 软件要求

| 软件 | 版本 | 必需/可选 | 用途 |
|-----|------|---------|------|
| **操作系统** | Ubuntu 20.04+ | 必需 | 开发/部署平台 |
| **ROS2** | Humble | 必需 | 核心通信框架 |
| **CMake** | 3.22+ | 必需 | 构建系统 |
| **GCC** | 9.0+ | 必需 | C++编译器 |
| **Boost** | 1.74+ | 必需 | C++库 |
| **Docker** | 20.10+ | 可选 | 容器化部署 |
| **systemd** | 245+ | 可选 | 服务管理 |
| **Python** | 3.8+ | 可选 | 脚本工具 |

#### 5.1.3 依赖库

```cmake
# 核心依赖
- ROS2 Humble (rclcpp, rosbag2_cpp, etc.)
- Boost (filesystem, system, iostreams, thread, regex)
- ONNX Runtime 1.16+

# 可选依赖
- AWS SDK (3rdparty/aws-sdk)
- OpenSSL (MQTT SSL)
- OpenMP (并行加速)
```

### 5.2 性能指标

#### 5.2.1 实时性能

| 指标 | 目标值 | 实测值 | 测试条件 |
|-----|-------|-------|---------|
| **推理延迟** | <20ms | 8-12ms | ONNX CPU, 43维输入 |
| **控制频率** | 50Hz | 50Hz | Humanoid模式 |
| **触发响应** | <100ms | 50-80ms | 步态触发 |
| **路径规划** | <1s | 0.3-0.8s | 100 waypoints |
| **内存占用** | <200MB | 120-150MB | 稳定运行 |
| **CPU使用** | <20% | 8-15% | 8核CPU |

**推理性能详解**:
```
测试环境: Intel Core i7-10700 @ 2.9GHz
测试模型: humanoid_ppo.onnx
输入维度: 43
输出维度: 3 (action_mean) + 3 (action_log_std) + 1 (value)

推理时间分布:
  10%:  6ms  (p10)
  50%:  9ms  (p50, 中位数)
  90%:  12ms (p90)
  99%:  15ms (p99)

内存占用:
  模型大小: 45MB (FP32)
  运行时: 50MB
  中间数据: 20MB
  总计: ~120MB
```

#### 5.2.2 采集性能

| 指标 | 目标值 | 实测值 | 测试条件 |
|-----|-------|-------|---------|
| **数据过滤率** | >70% | 82% | 步态触发 vs 路径点 |
| **采集间隔** | 1-2s | 1.0-1.4s | 正常行走 |
| **步长精度** | ±5cm | ±3cm | 0.25m步长 |
| **去重准确率** | >99% | 100% | 1000点历史 |
| **触发准确率** | >95% | 97% | 稳定支撑相检测 |

**采集数据量**:
```
单次采集:
  时间窗口: 20s (15s前 + 5s后)
  话题数: 4个
  频率: 50Hz
  单消息大小: ~1KB
  总数据量: ~250MB

日采集量 (按8小时/天):
  采集次数: ~1000次
  总数据量: ~250GB
  上传量 (82%过滤后): ~45GB
```

#### 5.2.3 上传性能

| 指标 | 目标值 | 实测值 | 测试条件 |
|-----|-------|-------|---------|
| **上传速度** | >1MB/s | 2-5MB/s | 依赖网络 |
| **分片上传** | 支持 | ✅ | 50MB/片 |
| **断点续传** | 支持 | ✅ | 自动恢复 |
| **批量大小** | 1000样本 | 1000样本 | 可配置 |
| **成功率** | >99% | 99.5% | 3次重试 |

**上传优化**:
```json
{
  "uploadFileSliceSizeMb": 50,      // 分片大小
  "uploadFileSliceIntervalMs": 100,  // 分片间隔
  "uploadFileIntervalMs": 100,       // 文件间隔
  "retryCount": 3,                   // 重试次数
  "retryIntervalSec": 10             // 重试间隔
}
```

### 5.3 通信协议

#### 5.3.1 ROS2话题

| 话题 | 类型 | 频率 | 方向 | 说明 |
|-----|------|------|------|------|
| **机器人状态** |
| `/robot/odom` | nav_msgs/Odometry | 50Hz | 订阅 | 里程计数据 |
| `/robot/joint_states` | sensor_msgs/JointState | 50Hz | 订阅 | 关节状态 |
| `/robot/imu` | sensor_msgs/Imu | 50Hz | 订阅 | IMU数据 |
| **控制输出** |
| `/robot/cmd_vel` | geometry_msgs/Twist | 50Hz | 发布 | 速度命令 |
| **可视化** |
| `/planned_path` | nav_msgs/Path | 1Hz | 发布 | 计划路径 |
| `/collected_path` | nav_msgs/Path | 1Hz | 发布 | 已采集路径 |
| `/collection_markers` | visualization_msgs/MarkerArray | 1Hz | 发布 | 采集点标记 |

**话题消息定义** (部分):
```cpp
// /robot/odom
struct Odometry {
    Header header;
    string child_frame_id;
    PoseWithCovariance pose;      // 位置: x, y, yaw
    TwistWithCovariance twist;     // 速度: vx, vy, vyaw
};

// /robot/joint_states
struct JointState {
    Header header;
    vector<string> name;          // 关节名称 (12个)
    vector<double> position;      // 关节位置
    vector<double> velocity;      // 关节速度
    vector<double> effort;        // 关节力矩
};

// /planned_path
struct Path {
    Header header;
    vector<PoseStamped> poses;    // 路径点序列
};
```

#### 5.3.2 MQTT协议

**连接配置** (app_config.json):
```json
{
  "mqtt": {
    "broker": "emqx-dna-vehicle.perf.dftccloud.t.home:9881",
    "broker_ssl": "ssl://dna-emq-perfs.dfiov.com.cn:10006",
    "username": "dnaapp",
    "password": "D7f%dnD&63B",
    "upTopic": "/SHADOW/sd@{VIN}/{DEVICE_ID}/terminal/up/request",
    "downTopic": "/SHADOW/sd@{VIN}/tsp/down/response"
  }
}
```

**上行消息** (边缘→云端):
```json
Topic: /SHADOW/sd@LFBGEV070LJD45885/V-Box2103010456/terminal/up/request

{
  "msgType": "dataCollectionReport",
  "timestamp": "2026-03-07T14:30:22Z",
  "payload": {
    "collection_id": "col_20260307_143022",
    "vin": "LFBGEV070LJD45885",
    "device_id": "V-Box2103010456",
    "position": {"x": 1.23, "y": 4.56, "yaw": 0.78},
    "reward": 0.85,
    "scene_type": "indoor_flat",
    "bag_file": "dcp_20260307_143022.db3",
    "data_size": 268435456,
    "upload_status": "uploaded",
    "s3_key": "caic-dataset/xc/humanoid/2026-03-07/dcp_20260307_143022.db3",
    "metadata": {
      "trigger_type": "gait_footstrike",
      "step_length": 0.58,
      "gait_phase": 270.0,
      "quality_score": 0.92
    }
  }
}
```

**下行消息** (云端→边缘):
```json
Topic: /SHADOW/sd@LFBGEV070LJD45885/tsp/down/response

{
  "msgType": "modelUpdate",
  "timestamp": "2026-03-07T15:00:00Z",
  "payload": {
    "model_version": "v1.1.3",
    "download_url": "https://s3.../humanoid_v3.onnx",
    "md5": "abc123def456...",
    "file_size": 47185920,
    "config_update": {
      "planner_mode": "humanoid",
      "exploration_bonus": 15.0,
      "sparse_threshold": 0.2
    },
    "update_instructions": {
      "backup_current": true,
      "apply_immediately": true,
      "verify_before_apply": true
    }
  }
}
```

#### 5.3.3 AWS S3 API

**端点配置**:
```
Endpoint: orderseek-obs.orderseek.ai
Region: US
Protocol: HTTPS
Auth: AWS Signature V2
Bucket: caic-dataset
```

**S3 Key规范**:
```
{bucket}/{prefix}/{type}/{date}/{file}

示例:
caic-dataset/xc/humanoid/2026-03-07/dcp_20260307_143022.db3

参数说明:
- bucket: caic-dataset
- prefix: xc (用户/项目标识)
- type: humanoid (采集类型)
- date: 2026-03-07 (采集日期)
- file: dcp_20260307_143022.db3 (文件名)
```

### 5.4 数据格式

#### 5.4.1 Rosbag2存储结构

```
/data/aer/bags/
├─ dcp_20260307_143022/
│  ├─ dcp_20260307_143022_0.db3        # 分片0
│  ├─ dcp_20260307_143022_1.db3        # 分片1
│  ├─ dcp_20260307_143022_2.db3        # 分片2
│  ├─ dcp_20260307_143022_3.db3        # 分片3
│  ├─ metadata.yaml                    # 元数据
│  └─ ros2.db3                         # 索引
│
├─ dcp_20260307_150134/
│  └─ ...
│
└─ upload_record.json                  # 上传记录
```

**metadata.yaml** 示例:
```yaml
version: 5
rosbag2_bag_file_information:
  version: 7
  storage_identifier: sqlite3
  relative_file_paths:
    - dcp_20260307_143022_0.db3
    - dcp_20260307_143022_1.db3
  duration:
    nanoseconds: 20000000000  # 20秒
  starting_time:
    nanoseconds_since_epoch: 1709807422000000000
  message_count: 40000  # 50Hz × 4话题 × 20s
  topics_with_message_count:
    /robot/odom: 1000
    /robot/joint_states: 1000
    /robot/imu: 1000
    /robot/cmd_vel: 1000
```

#### 5.4.2 采集元数据

**触发信息** (trigger_info.json):
```json
{
  "collection_id": "col_20260307_143022",
  "version": "1.0",
  "timestamp": "2026-03-07T14:30:22Z",
  "trigger": {
    "type": "gait_footstrike",
    "position": {"x": 1.23, "y": 4.56},
    "gait_phase": 270.0,
    "step_length": 0.58,
    "is_stable": true
  },
  "robot": {
    "vin": "LFBGEV070LJD45885",
    "software_version": "v1.1.2",
    "hardware_version": "rc3.0",
    "device_id": "V-Box2103010456"
  },
  "environment": {
    "scene_type": "indoor_flat",
    "scene_rarity": 0.3,
    "terrain_type": "flat"
  },
  "data": {
    "bag_file": "dcp_20260307_143022.db3",
    "duration_sec": 20,
    "topics": ["/robot/odom", "/joint_states", "/robot/imu", "/robot/cmd_vel"],
    "data_size": 268435456,
    "message_count": 40000
  },
  "evaluation": {
    "reward": 0.85,
    "quality_score": 0.92,
    "value_score": {
      "spatial_rarity": 0.7,
      "temporal_freshness": 0.9,
      "scene_diversity": 0.3,
      "quality": 0.92,
      "coverage_contribution": 0.05,
      "total": 0.62
    }
  },
  "upload": {
    "s3_key": "caic-dataset/xc/humanoid/2026-03-07/dcp_20260307_143022.db3",
    "upload_time": "2026-03-07T14:35:00Z",
    "status": "uploaded"
  }
}
```

#### 5.4.3 上传记录

**upload_record.json**:
```json
{
  "version": "1.0",
  "last_update": "2026-03-07T14:35:00Z",
  "statistics": {
    "total_collections": 1234,
    "uploaded_collections": 1200,
    "pending_collections": 34,
    "failed_collections": 0,
    "success_rate": 0.995,
    "total_data_uploaded": 322122547200  # ~300GB
  },
  "records": [
    {
      "collection_id": "col_20260307_143022",
      "local_path": "/data/aer/bags/dcp_20260307_143022.db3",
      "file_size": 268435456,
      "s3_key": "caic-dataset/xc/humanoid/2026-03-07/dcp_20260307_143022.db3",
      "upload_status": "uploaded",
      "upload_time": "2026-03-07T14:35:00Z",
      "retry_count": 0
    }
  ]
}
```

---

## 6. 部署指南

### 6.1 部署架构

```
┌─────────────────────────────────────────────────────────────┐
│                    部署方案选择                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  方案1: Docker Compose（推荐生产环境）                       │
│  ├─ 优点: 环境隔离、快速部署、易于升级、可扩展               │
│  ├─ 缺点: 需要Docker支持、资源开销略高                       │
│  └─ 适用: 边缘设备、云服务器、多机器人部署                   │
│                                                             │
│  方案2: systemd服务（推荐长期运行）                         │
│  ├─ 优点: 自动启动、系统集成、日志管理、资源控制             │
│  ├─ 缺点: 需要手动配置环境                                  │
│  └─ 适用: 生产环境、专用边缘设备                            │
│                                                             │
│  方案3: 手动运行（推荐开发测试）                            │
│  ├─ 优点: 灵活调试、开发友好、快速迭代                       │
│  ├─ 缺点: 需要手动管理进程、无自动重启                       │
│  └─ 适用: 开发调试、功能验证                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 Docker部署

#### 6.2.1 构建镜像

```bash
# 1. 准备代码
git clone https://github.com/your-org/aurora-edge-runtime.git
cd aurora-edge-runtime

# 2. 构建镜像
docker build -t aurora-edge-runtime:v1.1.2 .

# 3. 查看镜像
docker images | grep aurora-edge-runtime
# aurora-edge-runtime   v1.1.2   abc123   5 minutes ago   1.2GB

# 4. （可选）推送到仓库
docker tag aurora-edge-runtime:v1.1.2 registry.example.com/aurora/edge-runtime:v1.1.2
docker push registry.example.com/aurora/edge-runtime:v1.1.2
```

#### 6.2.2 运行容器

**方式1: Docker Compose（推荐）**

```bash
# 使用docker-compose启动
docker-compose up -d

# 查看日志
docker-compose logs -f

# 停止服务
docker-compose down

# 重启服务
docker-compose restart
```

**docker-compose.yml**:
```yaml
version: '3.8'

services:
  aurora-edge-runtime:
    image: aurora-edge-runtime:v1.1.2
    container_name: aurora_edge_runtime
    network_mode: host  # 使用主机网络（ROS2通信需要）
    restart: unless-stopped
    privileged: true   # 需要特权模式（硬件访问）

    # 挂载目录
    volumes:
      # 数据目录
      - /data/aer:/data/aer
      # 配置文件
      - ./config:/app/config:ro
      # 模型文件
      - ./models:/app/models:ro
      # 日志目录
      - ./logs:/var/log/aer
      # ROS2安装（可选）
      - /opt/ros/humble:/opt/ros/humble:ro

    # 环境变量
    environment:
      - DCP_MODE=humanoid
      - DCP_LOG_LEVEL=2
      - DCP_ENABLE_VISUALIZATION=false
      - DCP_MAX_CYCLES=0
      - RMW_IMPLEMENTATION=rmw_fastrtps_cpp
      - ROS_DOMAIN_ID=0

    # 资源限制
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 2G
        reservations:
          cpus: '1.0'
          memory: 1G

    # 健康检查
    healthcheck:
      test: ["CMD", "ros2", "node", "list"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 40s

    # 命令
    command: /app/build/src/aer
```

**方式2: Docker Run**

```bash
docker run -d \
  --name aurora_edge_runtime \
  --network host \
  --restart unless-stopped \
  --privileged \
  -v /data/aer:/data/aer \
  -v $(pwd)/config:/app/config:ro \
  -v $(pwd)/models:/app/models:ro \
  -v $(pwd)/logs:/var/log/aer \
  -e DCP_MODE=humanoid \
  -e DCP_LOG_LEVEL=2 \
  -e DCP_ENABLE_VISUALIZATION=false \
  -e RMW_IMPLEMENTATION=rmw_fastrtps_cpp \
  --cpus="2.0" \
  --memory="2g" \
  aurora-edge-runtime:v1.1.2

# 查看日志
docker logs -f aurora_edge_runtime

# 进入容器
docker exec -it aurora_edge_runtime /bin/bash

# 查看资源使用
docker stats aurora_edge_runtime
```

#### 6.2.3 多容器部署（多机器人）

```yaml
version: '3.8'

services:
  # 机器人1
  robot1:
    image: aurora-edge-runtime:v1.1.2
    container_name: aurora_robot1
    network_mode: host
    environment:
      - ROS_DOMAIN_ID=1  # 不同机器人使用不同域ID
      - DCP_MODE=humanoid
      - CAR_ID=robot1
    volumes:
      - /data/robot1:/data/aer
    command: /app/build/src/aer

  # 机器人2
  robot2:
    image: aurora-edge-runtime:v1.1.2
    container_name: aurora_robot2
    network_mode: host
    environment:
      - ROS_DOMAIN_ID=2
      - DCP_MODE=humanoid
      - CAR_ID=robot2
    volumes:
      - /data/robot2:/data/aer
    command: /app/build/src/aer

  # 机器人3
  robot3:
    image: aurora-edge-runtime:v1.1.2
    container_name: aurora_robot3
    network_mode: host
    environment:
      - ROS_DOMAIN_ID=3
      - DCP_MODE=humanoid
      - CAR_ID=robot3
    volumes:
      - /data/robot3:/data/aer
    command: /app/build/src/aer
```

### 6.3 systemd服务部署

#### 6.3.1 安装服务

```bash
# 执行安装脚本
sudo ./ops/prod/install-aer-service.sh install

# 查看服务状态
sudo systemctl status aer

# 启动服务
sudo systemctl start aer

# 停止服务
sudo systemctl stop aer

# 重启服务
sudo systemctl restart aer

# 开机自启动
sudo systemctl enable aer

# 禁用自启动
sudo systemctl disable aer

# 查看服务日志
sudo journalctl -u aer -f
sudo journalctl -u aer -n 100
```

#### 6.3.2 服务配置

**配置文件**: `/etc/default/aer` 或 `ops/prod/aer.conf`

```bash
# ===== 运行模式 =====
# 可选值: auto (自动驾驶), humanoid (人形机器人)
DCP_MODE=humanoid

# ===== 配置文件路径 =====
# ONNX 模型文件路径
DCP_MODEL_PATH=/opt/models/humanoid_ppo.onnx

# 配置文件路径
DCP_CONFIG_PATH=/opt/dcp/config/planner_weights.yaml

# ===== 日志配置 =====
# 日志级别: 0=DEBUG1, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR
DCP_LOG_LEVEL=2

# 日志文件路径
DCP_LOG_FILE=/var/log/dcp/dcp.log

# ===== 数据采集配置 =====
# 数据存储根目录
DCP_DATA_ROOT=/data/aer

# Rosbag2 存储路径
DCP_BAG_PATH=/data/aer/bags

# ===== AWS S3 上传配置 =====
# 是否启用上传
DCP_ENABLE_UPLOAD=true

# S3 Bucket 名称
DCP_S3_BUCKET=caic-dataset

# ===== 可视化配置 =====
# 是否启用 RViz2 可视化
DCP_ENABLE_VISUALIZATION=false

# 可视化坐标系 (odom, map, base_link)
DCP_VIS_FRAME=odom

# ===== 性能配置 =====
# 最大采集周期数 (0 表示无限循环)
DCP_MAX_CYCLES=0

# 每个周期后的等待时间（秒）
DCP_CYCLE_WAIT=5

# ===== 调试选项 =====
# 是否启用调试输出
DCP_DEBUG=false

# ===== CPU 绑核配置 =====
# CPU 亲和性配置
# 格式: 单个核心(0), 多个不连续核心(0,2,4), 范围(0-3), 混合(0-3,6-7)
# 空值表示不绑核，由系统调度
DCP_CPU_AFFINITY=0
```

#### 6.3.3 重载配置

```bash
# 方式1: 使用重载脚本
sudo ./ops/prod/install-aer-service.sh reload

# 方式2: 手动重载
sudo vim /etc/default/aer
sudo systemctl daemon-reload
sudo systemctl restart aer
```

### 6.4 边缘设备部署

#### 6.4.1 自动化部署

**配置目标设备**:
```bash
# 设置目标设备
export EDGE_HOST="edge-device.example.com"
export EDGE_USER="admin"
export EDGE_PORT="22"

# 执行部署脚本
./ops/deploy_edge.sh
```

**部署脚本** (`ops/deploy_edge.sh`):
```bash
#!/bin/bash

set -e

# 配置
EDGE_HOST=${EDGE_HOST:-"edge-device"}
EDGE_USER=${EDGE_USER:-"admin"}
EDGE_PORT=${EDGE_PORT:-"22"}
DEPLOY_DIR=${DEPLOY_DIR:-"/opt/aurora/edge-runtime"}

echo "Deploying Aurora-Edge-Runtime to ${EDGE_USER}@${EDGE_HOST}:${EDGE_DIR}"

# 1. 创建远程目录
echo "Creating remote directories..."
ssh -p ${EDGE_PORT} ${EDGE_USER}@${EDGE_HOST} "
  mkdir -p ${DEPLOY_DIR}/{config,models,data/bags,data/enc,data/masked,logs}
"

# 2. 复制文件（排除不需要的）
echo "Copying files..."
rsync -avz --exclude='build/' \
            --exclude='.git/' \
            --exclude='*.o' \
            --exclude='*.a' \
            -e "ssh -p ${EDGE_PORT}" \
            ./ ${EDGE_USER}@${EDGE_HOST}:${DEPLOY_DIR}/

# 3. 远程编译（如果需要）
echo "Building on remote device..."
ssh -p ${EDGE_PORT} ${EDGE_USER}@${EDGE_HOST} "
  cd ${DEPLOY_DIR}
  mkdir -p build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make -j\$(nproc)
"

# 4. 配置环境变量
echo "Setting up environment..."
ssh -p ${EDGE_PORT} ${EDGE_USER}@${EDGE_HOST} "
  cat >> ~/.bashrc << 'EOF'
# Aurora-Edge-Runtime Environment
export DCP_MODE=humanoid
export DCP_LOG_LEVEL=2
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
source /opt/ros/humble/setup.bash
EOF
"

# 5. 启动服务
echo "Starting service..."
ssh -p ${EDGE_PORT} ${EDGE_USER}@${EDGE_HOST} "
  cd ${DEPLOY_DIR}
  sudo ./ops/prod/install-aer-service.sh start
"

echo "Deployment complete!"
```

#### 6.4.2 手动部署

```bash
# 1. SSH登录边缘设备
ssh admin@edge-device

# 2. 创建目录结构
sudo mkdir -p /opt/aurora/edge-runtime/{config,models,data,logs,build}

# 3. 上传代码（在本地执行）
rsync -avz --exclude='build' --exclude='.git' \
  ./ admin@edge-device:/opt/aurora/edge-runtime/

# 4. 编译（在边缘设备上执行）
cd /opt/aurora/edge-runtime
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 5. 配置环境变量
source resource/scripts/setup.bash

# 6. 测试运行
./build/src/aer --mode humanoid

# 7. 安装服务（可选）
sudo ./ops/prod/install-aer-service.sh install
sudo systemctl start aer
```

### 6.5 环境配置

#### 6.5.1 环境变量

**必需环境变量**:
```bash
# Aurora-Edge-Runtime 环境
export PROJECT=aurora_edge_runtime
export VIN=LFBGEV070LJD45885
export INSTALL_ROOT_PATH=/opt/aurora/edge-runtime
export CAR_ID=robot001

# ROS2 环境
source /opt/ros/humble/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export ROS_DOMAIN_ID=0

# DCP 配置
export DCP_MODE=humanoid           # auto | humanoid
export DCP_LOG_LEVEL=2            # 0=DEBUG1, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR
export DCP_ENABLE_VISUALIZATION=false
export DCP_MAX_CYCLES=0            # 0=无限循环

# 路径配置
export LD_LIBRARY_PATH=${INSTALL_ROOT_PATH}/install/lib:${LD_LIBRARY_PATH}
export PATH=${INSTALL_ROOT_PATH}/install/bin:${PATH}
```

**可选环境变量**:
```bash
# AWS 凭证（如果不使用配置文件）
export AWS_ACCESS_KEY_ID=your_access_key
export AWS_SECRET_ACCESS_KEY=your_secret_key
export AWS_DEFAULT_REGION=us-east-1
export AWS_ENDPOINT_URL=https://orderseek-obs.orderseek.ai

# 调试选项
export DCP_DEBUG=true
export DCP_ENABLE_CORE_DUMP=false

# 性能调优
export OMP_NUM_THREADS=4
export MALLOC_ARENA_MAX=2
```

#### 6.5.2 目录结构

```
/opt/aurora/edge-runtime/
├─ bin/                          # 可执行文件（软链接）
│  └─ dcp → ../build/src/aer
│
├─ install/                      # 安装目录
│  ├─ bin/
│  │  └─ dcp                     # 主程序
│  └─ lib/                       # 库文件
│     ├─ libdcp.so
│     ├─ libonnxruntime.so
│     └─ ...
│
├─ config/                       # 配置文件
│  ├─ planner_weights.yaml       # 规划器权重
│  ├─ app_config.json            # 应用配置
│  ├─ robot_data_collection.json # 采集策略
│  └─ log_config.json            # 日志配置
│
├─ models/                       # ONNX模型
│  ├─ humanoid_ppo.onnx           # 人形机器人模型
│  └─ auto_ppo.onnx              # 自动驾驶模型
│
├─ data/                         # 数据目录
│  ├─ bags/                      # Rosbag文件
│  │  ├─ dcp_20260307_143022/
│  │  └─ upload_record.json
│  ├─ enc/                       # 加密数据
│  ├─ masked/                    # 脱敏数据
│  └─ pki/                       # 证书文件
│     ├─ client_cc.pem
│     ├─ client_ck.pem
│     └─ server_ca.pem
│
├─ logs/                         # 日志目录
│  └─ dcp.log
│
├─ ops/                          # 运维脚本
│  ├─ install-dcp-service.sh
│  ├─ deploy_edge.sh
│  ├─ start.sh
│  ├─ stop.sh
│  └─ health_check.sh
│
├─ resource/                     # 资源文件
│  ├─ scripts/
│  │  ├─ setup.bash              # 环境设置
│  │  ├─ performance.sh          # 性能监控
│  │  └─ kill.sh                 # 进程清理
│  └─ tar_extra/
│
├─ launch/                       # 启动文件
│  ├─ robot_demo_visualization.launch.py
│  └─ visualization.sh
│
├─ src/                          # 源代码
├─ tests/                        # 测试文件
├─ CMakeLists.txt                # 构建配置
├─ docker-compose.yml            # Docker配置
└─ README.md
```

#### 6.5.3 系统服务文件

**systemd service 文件**: `/etc/systemd/system/dcp.service`

```ini
[Unit]
Description=Aurora Edge Runtime - Data Collection Service
Documentation=https://github.com/your-org/aurora-edge-runtime
After=network.target ros2.service
Wants=network-online.target

[Service]
Type=simple
User=root
Group=root

# 环境文件
EnvironmentFile=/etc/default/aer

# 工作目录
WorkingDirectory=/opt/aurora/edge-runtime

# 执行命令
ExecStartPre=/opt/aurora/edge-runtime/resource/scripts/setup.bash
ExecStart=/opt/aurora/edge-runtime/build/src/aer
ExecStop=/bin/kill -SIGTERM $MAINPID
ExecStopPost=/opt/aurora/edge-runtime/ops/stop.sh

# 重启策略
Restart=always
RestartSec=10
StartLimitInterval=60
StartLimitBurst=3

# 资源限制
LimitCPU=100%
LimitFSIZE=infinity
LimitSIGPENDING=64411
LimitNPROC=64411
LimitAS=infinity

# 安全设置
NoNewPrivileges=false
PrivateTmp=false

# 日志
StandardOutput=append:/var/log/dcp/dcp.log
StandardError=append:/var/log/dcp/dcp.err

# CPU亲和性（如果配置）
CPUAffinity=0

[Install]
WantedBy=multi-user.target
```

---

（由于篇幅限制，第7-11章的内容在下一部分继续）

## 续接内容

以下是第7-11章的简要内容框架，完整内容请参考完整版文档：

## 7. API接口

### 7.1 ROS2服务接口
### 7.2 HTTP API（预留）
### 7.3 MQTT接口
### 7.4 命令行接口

## 8. 监控与运维

### 8.1 日志系统
### 8.2 性能监控
### 8.3 健康检查
### 8.4 告警配置
### 8.5 运维脚本

## 9. 故障排查

### 9.1 常见问题
### 9.2 诊断工具
### 9.3 日志分析
### 9.4 应急处理

## 10. 版本规划

### 10.1 当前版本 (v1.1.2)
### 10.2 短期规划 (v1.2.0)
### 10.3 中期规划 (v1.3.0)
### 10.4 长期愿景 (v2.0.0)
### 10.5 版本兼容性

## 11. 附录

### A. 术语表
### B. 参考文档
### C. 联系方式
### D. 许可证
### E. 变更历史

---

**文档信息**

- **产品名称**: Aurora-Edge-Runtime
- **所属产品线**: Aurora 产品矩阵
- **产品版本**: v1.1.2
- **文档版本**: 1.1
- **最后更新**: 2026-03-07
- **编制单位**: Aurora Edge Runtime Team
