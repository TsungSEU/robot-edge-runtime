# 性能测试文档

本目录包含 Aurora Edge Runtime 项目的性能测试和分析文档。

## 文档索引

### [性能优化指南](performance-optimization-guide.md)
完整的性能优化指南，包含：
- 性能分析工具使用说明
- 优化建议和最佳实践
- 常见性能问题解决方案

### [深度优化指南](deep-optimization-guide.md)
第三阶段深度优化，包含：
- SIMD 向量运算优化
- ONNX 模型推理优化
- 异步 I/O 机制实现

## 快速开始

### 运行性能测试

```bash
# 一键运行所有测试
./tools/performance/run_performance_tests.sh 30

# 单独运行基准测试
./tools/performance/run_benchmarks all

# CPU 性能分析
./tools/performance/perf_profiler.sh ./build/src/dcp 60

# 内存分析
./tools/performance/memory_profiler.sh ./build/src/dcp 60 --memcheck
```

### 查看测试结果

```bash
# 查看性能报告
cat docs/testing/performance/PERFORMANCE_REPORT.md

# 查看优化指南
cat docs/testing/performance/performance-optimization-guide.md

# 查看原始数据
cat tools/performance/results/*.csv
```

## 性能目标

| 指标 | 目标 | 当前状态 |
|------|------|----------|
| ONNX 推理延迟 | <10ms | ✅ ~8ms |
| 仿真更新频率 | 50Hz | ✅ ~7ms/帧 |
| 内存使用 | <100MB | ✅ ~75MB |
| CPU 使用率 | <60% | ✅ ~40% |

## 优化效果

### 第一、二阶段优化 (已实施)

| 组件 | 优化前 | 优化后 | 提升 |
|------|--------|--------|------|
| 三角函数 (sin) | 120k ops/s | 369k ops/s | +206% |
| 反正切 (atan2) | 66k ops/s | 369k ops/s | +450% |
| 逆运动学 | 77k ops/s | 321k ops/s | +300% |
| 固定向量 | 1.4M ops/s | 48M ops/s | +3243% |

### 第三阶段深度优化 (新增)

| 组件 | 优化前 | 优化后 | 提升 |
|------|--------|--------|------|
| SIMD dot_product | 0.045ms | 0.012ms | +275% |
| SIMD 距离计算 | 0.048ms | 0.013ms | +269% |
| ONNX 推理 (4线程+SIMD) | 8.5ms | 2.7ms | +213% |
| 异步文件写入 | 12.5s | 0.5s | +2400% |

## 相关工具

### 工具位置
- **性能分析脚本**: `tools/performance/`
- **优化工具库**: `src/common/performance_utils.h`
- **SIMD 优化库**: `src/common/simd_utils.h`
- **ONNX 优化器**: `src/common/onnx_optimizer.h`
- **异步 I/O**: `src/common/async_io.h`
- **基准测试**: `tools/performance/run_benchmarks`

### 依赖安装
```bash
# CPU 性能分析
sudo apt-get install linux-tools-common linux-tools-generic

# 内存分析
sudo apt-get install valgrind
```

## 贡献指南

如需添加新的性能测试或优化建议：

1. 在 `tools/performance/` 中添加测试代码
2. 运行基准测试验证效果
3. 更新相关文档
4. 提交 Pull Request

---

**最后更新**: 2026-03-08
