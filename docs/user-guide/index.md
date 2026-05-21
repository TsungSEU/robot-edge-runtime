**Breadcrumbs:** [Docs](../README.md) / [User Guide](index.md) / Index

# 用户指南

欢迎使用 Aurora-Edge-Runtime 用户指南。本章节面向系统使用者，提供完整的操作说明和配置参考。

## 目录

### 核心概念

- [系统概览](concepts/overview.md) - 系统整体介绍
- [架构概览](concepts/architecture.md) - 系统架构说明
- [数据流说明](concepts/data-flow.md) - 数据采集流程
- [Aurora产品矩阵](concepts/aurora-matrix.md) - 在产品矩阵中的定位

### 配置指南

- [规划器配置](configuration/planner-config.md) - RL规划器参数配置
- [采集配置](configuration/collection-config.md) - 数据采集策略配置
- [应用配置](configuration/app-config.md) - 系统应用配置
- [参数调优指南](configuration/tuning-guide.md) - 性能调优

### 操作指南

- [数据采集](operation/data-collection.md) - 数据采集完整流程
- [可视化使用](operation/visualization.md) - RViz2 可视化
- [数据上传](operation/data-upload.md) - 云端数据上传
- [问题排查](operation/troubleshooting.md) - 常见问题解决

### 运行模式

- [自动驾驶模式](modes/auto-mode.md) - 车辆数据采集
- [人形机器人模式](modes/humanoid-mode.md) - 机器人数据采集

## 快速链接

| 我想... | 查看 |
|---------|------|
| 快速上手 | [快速入门](../getting-started/index.md) |
| 了解系统工作原理 | [核心概念](concepts/overview.md) |
| 配置采集参数 | [配置指南](configuration/planner-config.md) |
| 排查问题 | [问题排查](operation/troubleshooting.md) |

## 文档约定

### 代码块

```bash
# Shell 命令
./build/src/dcp
```

```yaml
# 配置文件
planner_mode: "humanoid"
```

```cpp
// C++ 代码
auto planner = std::make_unique<HumanoidPlanner>();
```

### 语法高亮

- **命令**: 使用 `command`
- **文件名**: 使用 `filename`
- **路径**: 使用 `/path/to/file`

### 提示框

> **提示**: 有用的提示信息

> **警告**: 需要注意的事项

> **注意**: 重要但非错误信息

## 反馈与贡献

如果您发现文档问题或有改进建议，欢迎：

1. 提交 [Issue](https://github.com/your-org/aurora-edge-runtime/issues)
2. 提交 [Pull Request](https://github.com/your-org/aurora-edge-runtime/pulls)
3. 发送邮件到 docs@example.com
