# Aurora-Edge-Runtime 文档中心

欢迎来到 Aurora-Edge-Runtime 文档中心。本项目遵循 [Diátaxis Framework](https://diataxis.fr/) 文档组织原则。

## 文档目录

```
docs/
├── README.md                    # 文档导航（本文件）
│
├── getting-started/             # 🚀 快速入门
│   ├── index.md                 # 快速开始指南
│   ├── installation.md          # 安装指南
│   └── first-run.md             # 第一次运行
│
├── user-guide/                  # 📖 用户手册
│   ├── index.md                 # 用户指南首页
│   ├── concepts/                # 核心概念
│   │   ├── overview.md          # 系统概览
│   │   └── aurora-matrix.md     # Aurora产品矩阵
│   ├── configuration/           # 配置指南
│   │   └── planner-config.md    # 规划器配置
│   └── operation/               # 操作指南
│       └── troubleshooting.md   # 问题排查
│
├── developer-guide/             # 👨‍💻 开发者指南
│   ├── index.md                 # 开发者指南首页
│   ├── architecture/            # 架构文档
│   │   ├── integration-flow.md  # 系统集成流程
│   │   ├── data-structures.md   # 核心数据结构
│   │   ├── design-decisions.md  # 设计决策记录
│   │   ├── components/          # 核心组件
│   │   │   ├── planner.md       # 规划器
│   │   │   ├── trigger.md       # 触发器
│   │   │   ├── executor.md      # 执行器
│   │   │   ├── uploader.md      # 上传器
│   │   │   └── state-machine.md # 状态机
│   │   ├── algorithms/          # 算法文档
│   │   │   ├── rl-planning.md   # RL 规划算法
│   │   │   └── gait-detection.md # 步态检测算法
│   │   └── README.md            # 架构说明
│   └── setup/                   # 开发环境
│       ├── dev-environment.md   # 环境配置
│       └── building.md          # 编译指南
│
├── operations/                  # 🔧 运维手册
│   └── index.md                 # 运维首页
│
├── testing/                     # ✅ 测试文档
│   └── index.md                 # 测试首页
│
├── product/                     # 📦 产品文档
│   ├── index.md                 # 产品首页
│   ├── specification.md         # 产品规格说明书
│   └── changelog/               # 变更日志
│       └── v1.1.2.md            # v1.1.2 变更
│
├── hardware/                    # 🤖 硬件文档
│   └── index.md                 # 硬件首页
│
├── api/                         # 🔌 API 文档
│   └── index.md                 # API 首页
│
├── faq/                         # ❓ 常见问题
│   └── index.md                 # FAQ 首页
│
└── resources/                   # 📁 资源文件
    └── images/                  # 图片资源
```

## 文档类型说明

| 文档类型 | 目标读者 | 内容特点 |
|---------|---------|---------|
| **教程 (Tutorials)** | 初学者 | 步骤式学习，强调动手实践 |
| **操作指南 (How-to)** | 实践者 | 解决具体问题，目标导向 |
| **解释 (Explanation)** | 理解者 | 深入讲解概念和原理 |
| **参考 (Reference)** | 专家用户 | 精确描述，信息密度高 |

## 快速导航

### 我是新用户，想开始使用
👉 [快速入门](getting-started/index.md)

### 我想了解系统如何工作
👉 [核心概念](user-guide/concepts/overview.md)

### 我是开发者，想参与开发
👉 [开发者指南](developer-guide/index.md)

### 我需要部署到生产环境
👉 [运维手册](operations/index.md)

### 我遇到了问题
👉 [常见问题](faq/index.md) | [问题排查](user-guide/operation/troubleshooting.md)

### 我想查看API文档
👉 [API文档](api/index.md)

## 已完成的文档

### ✅ 快速入门 (getting-started/)
- [x] [快速开始指南](getting-started/index.md)
- [x] [安装指南](getting-started/installation.md)
- [x] [第一次运行](getting-started/first-run.md)

### ✅ 用户指南 (user-guide/)
- [x] [用户指南首页](user-guide/index.md)
- [x] [系统概览](user-guide/concepts/overview.md)
- [x] [Aurora产品矩阵](user-guide/concepts/aurora-matrix.md)
- [x] [规划器配置](user-guide/configuration/planner-config.md)
- [x] [问题排查](user-guide/operation/troubleshooting.md)

### ✅ 开发者指南 (developer-guide/)
- [x] [开发者指南首页](developer-guide/index.md)
- [x] [系统集成流程](developer-guide/architecture/integration-flow.md)
- [x] [核心数据结构](developer-guide/architecture/data-structures.md)
- [x] [设计决策记录](developer-guide/architecture/design-decisions.md)
- [x] [核心组件](developer-guide/architecture/components/)
  - [规划器](developer-guide/architecture/components/planner.md)
  - [触发器](developer-guide/architecture/components/trigger.md)
  - [执行器](developer-guide/architecture/components/executor.md)
  - [上传器](developer-guide/architecture/components/uploader.md)
  - [状态机](developer-guide/architecture/components/state-machine.md)
- [x] [算法文档](developer-guide/architecture/algorithms/)
  - [RL 规划算法](developer-guide/architecture/algorithms/rl-planning.md)
  - [步态检测算法](developer-guide/architecture/algorithms/gait-detection.md)
- [x] [开发环境配置](developer-guide/setup/dev-environment.md)
- [x] [编译指南](developer-guide/setup/building.md)

### ✅ 产品文档 (product/)
- [x] [产品首页](product/index.md)
- [x] [产品规格说明书](product/specification.md)
- [x] [v1.1.2 变更日志](product/changelog/v1.1.2.md)

### ✅ 其他
- [x] [运维手册首页](operations/index.md)
- [x] [测试文档首页](testing/index.md)
- [x] [硬件文档首页](hardware/index.md)
- [x] [API 文档首页](api/index.md)
- [x] [常见问题](faq/index.md)

## 📋 待补充的文档

### 用户指南 (user-guide/)
- [ ] configuration/collection-config.md - 采集配置详解
- [ ] configuration/app-config.md - 应用配置详解
- [ ] configuration/tuning-guide.md - 参数调优指南
- [ ] operation/data-collection.md - 数据采集完整流程
- [ ] operation/visualization.md - 可视化使用
- [ ] operation/data-upload.md - 数据上传指南
- [ ] modes/auto-mode.md - 自动驾驶模式详解
- [ ] modes/humanoid-mode.md - 人形机器人模式详解

### 开发者指南 (developer-guide/)
- [ ] setup/testing.md - 测试指南
- [ ] setup/ide-setup.md - IDE 配置
- [ ] architecture/algorithms/value-estimation.md - 价值评估算法
- [ ] contributing/ - 贡献指南
- [ ] extensions/ - 扩展开发

### 运维手册 (operations/)
- [ ] deployment/docker.md - Docker 部署
- [ ] deployment/systemd.md - systemd 服务
- [ ] deployment/edge-device.md - 边缘设备部署
- [ ] monitoring/metrics.md - 监控指标
- [ ] monitoring/logging.md - 日志管理
- [ ] monitoring/alerts.md - 告警配置
- [ ] maintenance/backup.md - 备份恢复
- [ ] maintenance/upgrade.md - 升级指南
- [ ] security/ - 安全配置

### 测试文档 (testing/)
- [ ] unit-tests/ - 单元测试
- [ ] integration-tests/ - 集成测试
- [ ] validation/validation-plan.md - 验证计划
- [ ] simulation/mujoco-setup.md - MuJoCo 设置

### 硬件文档 (hardware/)
- [ ] requirements/minimum.md - 最低配置
- [ ] requirements/recommended.md - 推荐配置
- [ ] requirements/compatibility.md - 兼容性列表
- [ ] robot-platforms/ - 机器人平台
- [ ] sensors/ - 传感器配置

## 文档规范

### 编写规范
- 使用 Markdown 格式
- 代码块指定语言
- 图片使用相对路径 `../resources/images/`
- 链接使用相对路径

### 命名规范
- 文件名：小写字母 + 连字符 (e.g., `installation-guide.md`)
- 章节标题：从 H2 开始（H1留给文档标题）
- 代码标识：使用反引号 (\`code\`)

## 贡献文档

我们欢迎任何形式的文档贡献！

- 🐛 发现错误？直接修复或提 Issue
- ✨ 有新想法？欢迎讨论
- 📝 改进现有文档？请提交 PR

详见：[贡献指南](developer-guide/contributing/contributing.md)

## 许可证

文档采用 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) 许可证。

---

**最后更新**: 2026-04-20
**维护者**: Aurora Edge Runtime Team

## 相关链接

- [项目主页](https://github.com/your-org/aurora-edge-runtime)
- [问题反馈](https://github.com/your-org/aurora-edge-runtime/issues)
- [讨论区](https://github.com/your-org/aurora-edge-runtime/discussions)
