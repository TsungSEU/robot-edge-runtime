**Breadcrumbs:** [Docs](../README.md) / [Developer Guide](index.md) / Index

# 开发者指南

欢迎来到 Aurora-Edge-Runtime 开发者指南。本章节面向开发者，提供架构设计、API 参考和扩展开发指南。

## 目录

### 开发环境

- [环境配置](setup/dev-environment.md) - 开发环境搭建
- [编译指南](setup/building.md) - 编译系统说明
- [测试指南](setup/testing.md) - 运行和编写测试
- [IDE配置](setup/ide-setup.md) - 开发工具配置

### 架构文档

#### 系统架构
- [系统集成流程](architecture/integration-flow.md) - 端云协同架构、数据流、ROS2 接口
- [核心数据结构](architecture/data-structures.md) - 数据结构定义
- [设计决策记录](architecture/design-decisions.md) - 设计决策

#### 组件设计
- [规划器](architecture/components/planner.md) - RL 规划器
- [触发器](architecture/components/trigger.md) - 数据采集触发器
- [执行器](architecture/components/executor.md) - 采集执行器
- [上传器](architecture/components/uploader.md) - 云端上传器
- [状态机](architecture/components/state-machine.md) - 系统状态机

#### 算法说明
- [RL规划算法](architecture/algorithms/rl-planning.md) - 强化学习规划
- [步态检测](architecture/algorithms/gait-detection.md) - 步态相位检测
- [价值评估](architecture/algorithms/value-estimation.md) - 数据价值评估

### 贡献指南

- [贡献指南](contributing/contributing.md) - 如何贡献代码
- [代码规范](contributing/coding-style.md) - C++/Python 代码风格
- [提交规范](contributing/commit-conventions.md) - Git 提交规范
- [PR指南](contributing/pr-guidelines.md) - Pull Request 指南
- [审查清单](contributing/review-checklist.md) - 代码审查清单

### 扩展开发

- [自定义触发器](extensions/custom-trigger.md) - 开发自定义触发器
- [添加新模式](extensions/new-mode.md) - 添加新的运行模式
- [集成指南](extensions/integration-guide.md) - 与其他系统集成

## 快速开始

### 1. 获取代码

```bash
git clone https://github.com/your-org/aurora-edge-runtime.git
cd aurora-edge-runtime
```

### 2. 安装依赖

```bash
sudo apt install -y \
    ros-humble-desktop \
    python3-colcon-common-extensions \
    cmake build-essential libboost-all-dev
```

### 3. 编译项目

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### 4. 运行测试

```bash
cd build
ctest --output-on-failure
```

## 开发工作流

```
┌─────────────────────────────────────────────────────┐
│                    开发工作流                         │
├─────────────────────────────────────────────────────┤
│                                                      │
│  1. 创建分支                                         │
│     git checkout -b feature/your-feature            │
│                                                      │
│  2. 进行开发                                         │
│     ├── 编写代码                                     │
│     ├── 编写测试                                     │
│     └── 更新文档                                     │
│                                                      │
│  3. 本地验证                                         │
│     make -j$(nproc)                                  │
│     ctest                                            │
│                                                      │
│  4. 提交代码                                         │
│     git add .                                        │
│     git commit -m "feat: add your feature"          │
│                                                      │
│  5. 推送并创建 PR                                    │
│     git push origin feature/your-feature            │
│                                                      │
│  6. 代码审查与合并                                   │
│                                                      │
└─────────────────────────────────────────────────────┘
```

## 资源链接

- [API 文档](../api/index.md)
- [产品规格](../product/specification.md)
- [问题跟踪](https://github.com/your-org/aurora-edge-runtime/issues)
- [设计讨论](https://github.com/your-org/aurora-edge-runtime/discussions)

## 联系方式

- 开发者邮件: dev@example.com
- Slack 频道: #aurora-dev
