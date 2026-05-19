**Breadcrumbs:** [Docs](../README.md) / [Product](index.md) / Index

# 产品文档

本章节面向产品经理、项目经理和决策者，提供产品规格、路线图和版本信息。

## 目录

- [产品规格](specification.md) - Aurora-Edge-Runtime 产品规格说明书
- [产品对比](comparison.md) - 与竞品对比分析
- [路线图](roadmap.md) - 产品发展路线图
- [变更日志](changelog/) - 版本变更记录

## 产品定位

Aurora-Edge-Runtime 是 Aurora 产品矩阵中专注于**机器人端侧智能数据采集**的核心运行时系统。

### 核心价值

| 价值 | 说明 |
|-----|------|
| 🎯 **精准采集** | 步态级触发，82%冗余过滤率 |
| ⚡ **端侧智能** | <10ms 推理延迟，43维状态感知 |
| 🔄 **闭环协同** | 边缘推理 + 云端训练自动闭环 |
| 📦 **开箱即用** | Docker 容器化，systemd 服务化 |

### 目标市场

- 🤖 人形机器人研发企业
- 🚗 自动驾驶测试团队
- 🏭 工业机器人制造商
- 🛒 服务机器人公司

## 版本策略

### 版本命名

```
v{major}.{minor}.{patch}

示例: v1.1.2
- major: 重大架构变更
- minor: 新功能添加
- patch: Bug修复
```

### 发布周期

| 版本类型 | 周期 | 说明 |
|---------|------|-----|
| Major (大版本) | 6-12个月 | 架构升级、重大变更 |
| Minor (小版本) | 2-3个月 | 新功能、新特性 |
| Patch (补丁) | 随时 | Bug修复、安全更新 |

### 当前版本

**v1.1.2** (2026-03-07)

主要特性:
- ✅ 双模式支持 (Auto/Humanoid)
- ✅ 步态触发机制
- ✅ 配置热更新
- ✅ AWS S3 上传

### 即将发布

**v1.2.0** (预计 2026-Q2)

计划特性:
- 🔲 多机器人协同采集
- 🔲 实时模型更新
- 🔲 Web 监控界面

## 产品矩阵

```
┌─────────────────────────────────────────┐
│           Aurora 产品矩阵                │
├─────────────────────────────────────────┤
│                                         │
│  ☁️ Cloud                               │
│  ├── Aurora-Cloud-Trainer (模型训练)    │
│  ├── Aurora-Model-Hub (模型仓库)        │
│  └── Aurora-Data-Lake (数据湖)          │
│                                         │
│  📱 Edge                                │
│  ├── Aurora-Edge-Runtime (本项目)       │
│  ├── Aurora-Simulation (仿真)           │
│  ├── Aurora-Monitoring (监控)           │
│  └── Aurora-Deployment (部署)           │
│                                         │
└─────────────────────────────────────────┘
```

## 获取支持

### 商业支持
- 邮箱: business@example.com
- 电话: +86-xxx-xxxx-xxxx

### 技术支持
- 邮箱: support@example.com
- 工单: https://support.example.com

### 合作咨询
- 邮箱: partnership@example.com
