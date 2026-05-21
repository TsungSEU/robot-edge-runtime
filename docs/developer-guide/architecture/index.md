**Breadcrumbs:** [Docs](../../README.md) / [Developer Guide](../index.md) / [Architecture](index.md) / Index

# 软件架构文档

## 概述

本目录包含 Aurora-Edge-Runtime 系统的软件架构文档。

## 文档目录

### 核心架构

- [系统集成流程](integration-flow.md) - 端云协同架构、数据流、ROS2 接口

### 组件设计

| 文档 | 说明 |
|------|------|
| [规划器](components/planner.md) | RL 路径规划器 (PPO + ONNX) |
| [触发器](components/trigger.md) | 数据采集触发器（Auto 场景触发 + Humanoid 步态触发） |
| [执行器](components/executor.md) | 数据采集执行器 (环形缓冲) |
| [上传器](components/uploader.md) | AWS S3 数据上传器 |
| [状态机](components/state-machine.md) | 系统状态机 |

### 数据结构

- [核心数据结构](data-structures.md) - 系统核心数据结构定义

### 算法文档

| 文档 | 说明 |
|------|------|
| [RL 规划算法](algorithms/rl-planning.md) | PPO 强化学习规划算法 |
| [步态检测算法](algorithms/gait-detection.md) | 双足机器人步态检测 |

### 设计决策

- [设计决策记录](design-decisions.md) - 系统关键设计决策及原因

## 文档结构

```
architecture/
├── README.md                      # 本文件
├── integration-flow.md            # 系统集成流程
├── data-structures.md             # 数据结构
├── design-decisions.md            # 设计决策
├── components/                    # 组件文档
│   ├── planner.md
│   ├── trigger.md
│   ├── executor.md
│   ├── uploader.md
│   └── state-machine.md
└── algorithms/                    # 算法文档
    ├── rl-planning.md
    └── gait-detection.md
```
