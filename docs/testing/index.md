# 测试文档

本章节面向 QA 工程师和测试人员，提供 Aurora-Edge-Runtime 的测试策略、测试计划和验证报告。

## 目录

### 单元测试

- [测试计划](unit-tests/test-plan.md) - 单元测试计划
- [覆盖率报告](unit-tests/coverage.md) - 代码覆盖率报告
- [编写测试](unit-tests/writing-tests.md) - 测试编写指南

### 集成测试

- [测试场景](integration-tests/test-scenarios.md) - 集成测试场景
- [测试数据](integration-tests/test-data.md) - 测试数据管理
- [自动化测试](integration-tests/automation.md) - 自动化测试框架

### 验证测试

- [验证计划](validation/validation-plan.md) - 系统验证计划
- [验证报告](validation/validation-report.md) - v1.1.2 验证报告
- [性能验证](validation/performance.md) - 性能测试结果

### 仿真测试

- [MuJoCo设置](simulation/mujoco-setup.md) - MuJoCo 仿真环境
- [测试场景](simulation/test-scenarios.md) - 仿真测试场景
- [Sim2Real验证](simulation/sim-to-real.md) - 仿真到现实验证

## 测试概览

### 测试金字塔

```
              /\
             /  \
            / E2E \         ← 端到端测试 (少量)
           /--------\
          /          \
         / Integration \    ← 集成测试 (适量)
        /--------------\
       /                \
      /    Unit Tests    \  ← 单元测试 (大量)
     /--------------------\
```

### 测试统计

| 测试类型 | 测试数量 | 覆盖率 | 通过率 |
|---------|---------|-------|-------|
| 单元测试 | 120+ | 85% | 98% |
| 集成测试 | 45+ | - | 95% |
| 验证测试 | 30+ | - | 100% |
| 性能测试 | 15+ | - | 100% |

## 运行测试

### 全部测试

```bash
cd build
ctest --output-on-failure
```

### 单独运行单元测试

```bash
cd build
./tests/unit/test_dcp_core
```

### 运行集成测试

```bash
cd tests/integration
./run_integration_tests.sh
```

### 运行性能测试

```bash
cd tests/performance
./run_performance_tests.sh
```

## 测试报告

### 最新验证报告

- [v1.1.2 验证报告](validation/validation-report.md)
- 测试日期: 2026-03-07
- 测试环境: Intel i7 + Ubuntu 20.04 + ROS2 Humble
- 测试结果: ✅ 全部通过

### 性能基准

| 指标 | 目标值 | 实测值 | 状态 |
|-----|-------|-------|------|
| 推理延迟 | <20ms | 8-12ms | ✅ |
| 控制频率 | 50Hz | 50Hz | ✅ |
| 内存占用 | <200MB | 120-150MB | ✅ |
| 数据过滤率 | >70% | 82% | ✅ |

## 贡献测试

我们欢迎贡献测试用例！

1. 查看 [测试计划](unit-tests/test-plan.md)
2. 阅读 [编写测试](unit-tests/writing-tests.md)
3. 提交 Pull Request

## 联系方式

- QA 团队: qa@example.com
- 问题反馈: [GitHub Issues](https://github.com/your-org/aurora-edge-runtime/issues)
