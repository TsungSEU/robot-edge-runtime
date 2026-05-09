# Aurora-Edge-Runtime

**机器人端侧数据采集系统 | Aurora 产品矩阵**

基于强化学习 (PPO) 的智能路径规划与数据采集闭环系统，支持自动驾驶车辆和双足机器人平台。

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.10.3-blue)](CHANGELOG.md)

## 快速开始

### 编译

```bash
# 克隆项目（含子模块）
git clone --recursive https://gitlab.t3caic.com/icr11/dataengine/data-infra/Aurora/aurora-edge-runtime.git
cd aurora-edge-runtime

# 如果已克隆但未初始化子模块
git submodule update --init --recursive

# 设置环境变量
source resource/scripts/setup.bash

# 编译
mkdir build && cd build
cmake ..
make -j8
```

### 运行

详细的服务管理和部署指南请参考：**[ops/README.md](ops/README.md)**

### 可视化

```bash
# 启动 RViz2 可视化
ros2 launch aurora_edge_runtime robot_demo_visualization.launch.py
```

## 核心特性

- 🤖 **PPO 强化学习规划** - 云端训练，边缘 ONNX 推理
- 📍 **Execution-Aware CostMap** - 追踪规划点与实际到达点的误差
- 🦮 **双足步态采集** - 基于实际足端落地的智能触发
- 🚶 **Zhiyuan 运动控制** - Ruckig 轨迹规划、速度控制、多模式运动
- 🔄 **环形缓冲区** - 触发前 15s + 触发后 5s 数据记录
- ☁️ **AWS S3 分片上传** - 大文件可靠传输和断点续传
- 🔥 **配置热加载** - 无需重启的动态参数调整
- 📚 **完整文档体系** - 遵循 Diátaxis Framework 的结构化文档

## 支持模式

| 模式 | 状态空间            | 动作空间 | 适用平台 |
|------|-----------------|----------|----------|
| `auto` | 25维（含可达性）       | 4 离散动作 | 自动驾驶车辆 |
| `humanoid` | 75-77维（含可达性+扩展） | 18 连续动作 / 3-DOF 速度 | 双足机器人 |

## 文档

### 用户文档

| 文档 | 说明 |
|------|------|
| [快速入门](docs/getting-started/index.md) | 安装和第一次运行 |
| [系统概览](docs/user-guide/concepts/overview.md) | 核心概念和原理 |
| [配置指南](docs/user-guide/configuration/planner-config.md) | 参数配置说明 |
| [问题排查](docs/user-guide/operation/troubleshooting.md) | 常见问题解决 |

### 开发文档

| 文档 | 说明 |
|------|------|
| [系统架构](docs/developer-guide/architecture/system-architecture.md) | 整体架构设计 |
| [组件设计](docs/developer-guide/architecture/components/) | 核心组件说明 |
| [算法文档](docs/developer-guide/architecture/algorithms/) | RL 规划算法 |
| [设计决策](docs/developer-guide/architecture/design-decisions.md) | 设计决策记录 |

### 运维文档

| 文档 | 说明 |
|------|------|
| [Ops 目录说明](ops/README.md) | 运维脚本详细说明 |
| [快速参考](ops/QUICKREF.md) | 命令快速参考 |
| [运维手册](docs/operations/index.md) | 部署、监控、维护 |

### 产品文档

| 文档 | 说明 |
|------|------|
| [产品规格](docs/product/specification.md) | 产品规格说明书 |
| [Aurora 产品矩阵](docs/user-guide/concepts/aurora-matrix.md) | Aurora 产品体系 |

## 项目结构

```
aurora-edge-runtime/
├── config/              # 配置文件
├── docs/                # 文档
├── ops/                 # 运维脚本
├── src/                 # 源代码
│   └── rl_planning_infer/  # RL规划推理（子模块）
├── tests/               # 测试套件
├── cmake/               # cmake模块
└── 3rdparty/            # 第三方依赖（子模块）
```

## 版本历史

| 版本号               | 日期         | 说明 |
|-------------------|------------|-------------|
| **[1.10.3]**      | 2026-05-08 | 🔗 去 robot_sim 硬依赖、推理奖励权重对齐训练配置、aer logs 默认 aer |
| **[1.10.2]**      | 2026-05-08 | 🔧 子模块提取（rl_planning_infer）、可执行文件重命名 dcp→aer、日志节流优化 |
| **[1.10.1]**      | 2026-05-07 | 🔗 Edge-LivelyBot ROS2 通信增强：闭环速度反馈、topic 修复、QoS 诊断回调 |
| **[1.10.0]**      | 2026-04-30 | 📦 gzip 压缩支持、录制时长扩展至 60s、后录数据缺失修复 |
| **[1.9.0]**       | 2026-04-30 | 🛡️ 合规模块：地信坐标偏转、图像全帧脱敏（无OpenCV）、录制元数据清单+SHA-256校验 |
| **[1.8.1]**       | 2026-04-30 | 🔧 配置外部化、环境变量统一 AER_* 前缀、日志降噪、删除废弃测试 |
| **[1.8.0]**       | 2026-04-25 | 🔧 IPlanner 接口抽象、DataCollectionPlanner 解耦重构、环形缓冲区录制时长修复 |
| **[1.7.0]**       | 2026-04-17 | ♻️ 架构重构：双模式规划器（auto/humanoid）、目录重组 rl_planner_infer、43-dim 状态空间统一 |
| **[1.6.0]**       | 2026-04-14 | 🚀 RL Planner 推理优化：闭环速度跟踪、直线fallback、采集率提升（1→3）、实时轨迹可视化 |
| **[1.5.0]**       | 2026-04-14 | 🔧 可观测性：QoS策略集中管理、结构化日志、审计追踪、健康监控、测试套件 |
| **[1.4.0]**       | 2026-04-10 | 🛡️ 安全加固：状态机激活、异常处理、信号处理、凭证安全 |
| **[1.3.1]**       | 2026-04-09 | 🐛 修复录制统计累积问题、启动日志增强 |
| **[1.3.0]**       | 2026-04-08 | 🚀 3-DOF速度命令模式、PPO训练数据增强、奖励权重重平衡 |
| **[1.2.1]**       | 2026-04-03 | 🐛 修复 ROS2 回调饥饿导致里程计陈旧问题 |
| **[1.2.0]**       | 2026-03-23 | 🎉 重大版本：双进程同步优化、触发精度提升 |
| **[1.1.9]**       | 2026-03-19 | 🚀 统一AER命令、ops 目录重构 |
| **[1.1.8]**       | 2026-03-17 | ✨ ROS2 Service事件驱动触发机制、解耦架构 |
| **[1.1.7]**       | 2026-03-16 | 🐛 修复双进程路径连续性、航点等待机制同步 |
| **[1.1.6]**       | 2026-03-13 | 🔧 DataCollectionPlanner 执行反馈优化 |
| **[1.1.5]**       | 2026-03-11 | 🔧 Ruckig 实时轨迹规划库、3rdparty 模块结构优化 |
| **[1.1.4]**       | 2026-03-11 | 🚶 Zhiyuan 运动控制集成、文档重构、测试套件 |
| **[1.1.3]**       | 2026-03-10 | 📍 可达性追踪、状态空间扩展、执行反馈机制 | 
| **[1.1.2]**       | 2026-03-06 | 🦮 真实双足步态模拟器、基于实际足迹的采集触发 |
| **[1.1.1]**       | 2026-03-06 | 🔧 ONNX Runtime 1.16.3 升级、opset 17支持 |
| **[1.1.0]**       | 2026-03-04 | 🤖 人形机器人模式、采集路径可视化、YAML配置统一 |
| **[1.0.0]**       | 2026-02-25 | 🎉 首个稳定发布 - 边缘云协同数据采集架构、机器人模拟器 |
| **[0.4.0]**       | 2026-02-09 | S3分片上传、状态机调度 |
| **[0.3.0]**       | 2025-12-10 | 构建系统重构 |
| **[0.2.0]**       | 2025-12-09 | PPO路径规划、ONNX集成 |
| **[0.1.0]**       | 2025-12-05 | 子模块架构、日志系统 |
| **[0.1.0-rc.2]**  | 2025-12-04 | Release Candidate 2 |
| **[0.1.0-rc.1]**  | 2025-12-03 | Release Candidate 1 |
| **[0.1.0-alpha]** | 2025-12-02 | Alpha 版本 |
| **[0.0.3]**       | 2025-11-07 | 开发工具配置 |
| **[0.0.2]**       | 2025-11-05 | 文档更新 |
| **[0.0.1]**       | 2025-11-04 | 导航规划核心 |
| **[0.0.0]**       | 2025-10-23 | 初始提交 |

See [CHANGELOG.md](CHANGELOG.md) for detailed release notes.

## 许可证

本项目采用 Apache License 2.0 开源许可证。

Copyright © 2026 OrderSeek AI Inc. All rights reserved.

---

**Aurora 产品矩阵** | [文档中心](docs/README.md) | [贡献指南](docs/developer-guide/contributing/contributing.md)
