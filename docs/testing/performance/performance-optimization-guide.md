**Breadcrumbs:** [Docs](../../README.md) / [Testing](../index.md) / [Performance](index.md) / Performance Optimization Guide

# Aurora Edge Runtime 性能优化指南

本文档提供了项目性能优化的全面指南，包括性能分析工具、优化建议和最佳实践。

## 目录

1. [性能分析工具](#性能分析工具)
2. [热点函数优化](#热点函数优化)
3. [内存优化](#内存优化)
4. [并发优化](#并发优化)
5. [基准测试](#基准测试)
6. [优化建议](#优化建议)

---

## 性能分析工具

### 1. CPU性能分析 (perf_profiler.sh)

分析CPU热点函数和生成火焰图。

```bash
# 基本用法
./tools/performance/perf_profiler.sh ./build/src/dcp 60

# 输出文件:
# - tools/performance/results/perf.data          # 原始性能数据
# - tools/performance/results/hotspots.txt       # 热点函数报告
# - tools/performance/results/flamegraph.svg     # CPU火焰图
```

**结果解读**:
- 热点函数报告显示CPU占用最高的函数
- 火焰图可视化调用栈，宽度表示CPU时间
- 关注宽的火焰区域，这些是优化重点

### 2. 内存分析 (memory_profiler.sh)

检测内存泄漏和堆使用情况。

```bash
# 内存泄漏检测
./tools/performance/memory_profiler.sh ./build/src/dcp 60 --memcheck

# 堆分析
./tools/performance/memory_profiler.sh ./build/src/dcp 60 --massif

# 完整分析
./tools/performance/memory_profiler.sh ./build/src/dcp 60 --all
```

### 3. 基准测试框架

运行组件级性能测试。

```bash
# 编译基准测试
cd tools/performance
cmake -B build . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 运行所有测试
./build/run_benchmarks all

# 运行特定类别
./build/run_benchmarks math       # 数学函数
./build/run_benchmarks memory     # 内存操作
./build/run_benchmarks ik         # 逆运动学
./build/run_benchmarks concurrency # 并发性能
```

---

## 热点函数优化

### 1. 逆运动学优化 (robot_simulator.cpp:300-368)

**问题**: `computeLegIK()` 每帧调用两次（左右腿），包含大量三角函数计算。

**优化方案**:
```cpp
// 使用优化版本
#include "common/performance_utils.h"

class RobotSimulator {
private:
    aurora::performance::OptimizedLegIK ik_solver_;

    void updateSimulation() {
        // ...
        double left_joints[6], right_joints[6];
        ik_solver_.compute(left_foot_.x, left_foot_.y, left_foot_.z, true, left_joints);
        ik_solver_.compute(right_foot_.x, right_foot_.y, right_foot_.z, false, right_joints);

        for (int i = 0; i < 6; ++i) {
            joint_positions_[i] = left_joints[i];
            joint_positions_[i + 6] = right_joints[i];
        }
    }
};
```

**预期收益**: 30-40% 性能提升

### 2. 三角函数优化

**问题**: `updateGait()` 和 `updateFootTrajectories()` 中频繁调用 `sin()`/`cos()`。

**优化方案**:
```cpp
// 使用查找表代替实时计算
auto& trig = aurora::performance::getFastTrig();

// 替换
double value = std::sin(phase);

// 为
double value = trig.fastSin(phase);
```

**预期收益**: 3-4倍速度提升

### 3. 状态对象分配优化 (costmap.h:9-92)

**问题**: State 结构体频繁使用 `push_back`，导致多次内存分配。

**优化方案**:
```cpp
// 预分配容量
State(double norm_lat, double norm_lon, ...)
    : features() {
    features.reserve(24);  // 预分配容量
    // ...
}

// 或使用对象池
aurora::performance::StatePool<std::array<double, 24>> pool;
```

**预期收益**: 减少内存分配次数 80%

---

## 内存优化

### 1. 减少不必要的拷贝

**问题**: Point 和 State 结构体按值传递。

**优化方案**:
```cpp
// 修改前
void processPoint(Point p);

// 修改后
void processPoint(const Point& p);      // 输入参数
void processPoint(Point&& p);           // 需要转移所有权
```

### 2. 使用内存池

```cpp
// 已有的内存池 (src/common/memory_pool.h)
aurora::memory::InferenceBufferPool pool(state_dim, action_dim, 5);

// 使用 RAII 包装器
aurora::memory::ScopedInferenceBuffer buffer(pool);
float* state = buffer.state();
float* output = buffer.output();
// 自动释放
```

### 3. 避免内存碎片

```cpp
// 使用固定大小向量
aurora::performance::FixedVector<double, 24> state_features;

// 替代 std::vector<double> 在已知大小的场景
```

---

## 并发优化

### 1. 无锁数据结构

**问题**: GaitTrigger 中的 `state_mutex_` 可能成为瓶颈。

**优化方案**:
```cpp
// 使用无锁环形缓冲区传递传感器数据
aurora::performance::LockFreeRingBuffer<SensorData, 1024> sensor_buffer_;

// 生产者 (在回调中)
void odomCallback(const nav_msgs::msg::Odometry::SharedPtr msg) {
    sensor_buffer_.push(*msg);
}

// 消费者 (在分析线程)
void analyzeGaitState() {
    SensorData data;
    while (sensor_buffer_.pop(data)) {
        // 处理数据
    }
}
```

### 2. CPU亲和性设置

在 `ops/dcp.conf` 中配置:
```bash
# 绑定特定CPU核心
DCP_CPU_AFFINITY="2-5"  # 使用CPU 2-5
```

---

## 基准测试

### 添加新的基准测试

```cpp
// 在 run_benchmarks.cpp 中添加

void benchmark_my_component() {
    BenchmarkSuite suite;

    suite.addBenchmark("MyComponent test", [&]() {
        // 要测试的代码
        my_component_function();
    }, 10000);  // 迭代次数

    suite.runAll();
    suite.exportToCsv("tools/performance/results/my_benchmark.csv");
}
```

### 代码内性能测量

```cpp
#include "common/performance_utils.h"

// 单次计时
{
    SCOPED_TIMER("function_name");
    my_function();
}

// 累积计时
AccumulatingTimer timer("critical_section");
for (int i = 0; i < 100; ++i) {
    timer.start();
    critical_section();
    timer.stop();
}
timer.print();
```

---

## 优化建议

### 高优先级优化

| 组件 | 问题 | 优化方案 | 预期收益 |
|------|------|----------|----------|
| RobotSimulator::computeLegIK | 三角函数计算密集 | 使用 OptimizedLegIK | +30-40% |
| State::features | 频繁 push_back | 预分配容量 | +20% |
| GaitTrigger | 锁竞争 | 无锁队列 | +15% |
| publishMessages | 每帧创建消息 | 消息对象池 | +10% |

### 中优先级优化

1. **SIMD优化**: 对向量运算使用 SIMD 指令
2. **缓存友好**: 优化数据结构布局
3. **异步I/O**: ROS2 bag 录录使用异步模式

### 编译优化

确保使用正确的编译选项:
```cmake
# CMakeLists.txt
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_BUILD_TYPE RelWithDebInfo)  # 包含调试符号的优化版本
```

### 性能监控

运行时监控:
```bash
# 实时监控
./resource/scripts/performance.sh

# 导出性能数据
DCP_LOG_LEVEL=1 ./build/src/dcp 2>&1 | grep PERF
```

---

## 性能目标

| 指标 | 当前 | 目标 | 状态 |
|------|------|------|------|
| ONNX 推理延迟 | <10ms | <8ms | ⚠️ 需优化 |
| 仿真更新频率 | 50Hz | 50Hz | ✅ 达标 |
| 内存使用 | <100MB | <80MB | ⚠️ 需优化 |
| CPU 使用率 (单核) | <80% | <60% | ⚠️ 需优化 |

---

## 附录

### 性能分析工作流

1. **建立基准**: 运行基准测试，记录当前性能
2. **识别热点**: 使用 perf_profiler.sh 找出热点函数
3. **分析原因**: 查看代码，确定性能瓶颈原因
4. **实施优化**: 应用优化方案
5. **验证效果**: 运行基准测试，确认改进
6. **回归测试**: 确保功能正确性

### 常见性能问题

| 问题 | 症状 | 解决方案 |
|------|------|----------|
| 内存泄漏 | 内存持续增长 | 使用 valgrind 检测 |
| 锁竞争 | CPU高吞吐低 | 使用无锁结构 |
| 缓存未命中 | 性能波动 | 优化数据布局 |
| 虚拟内存 | 高 RSS | 使用内存池 |

### 参考资源

- [Linux perf 工具](https://www.brendangregg.com/perf.html)
- [Valgrind 用户手册](https://valgrind.org/docs/manual/)
- [Google Benchmark 库](https://github.com/google/benchmark)
- [FlameGraph 图形化工具](https://github.com/brendangregg/FlameGraph)
