# 性能优化工具集

此目录包含 Aurora Edge Runtime 项目的性能分析和优化工具。

## 工具列表

| 工具 | 描述 | 输出 |
|------|------|------|
| `perf_profiler.sh` | CPU 性能分析和火焰图生成 | hotspots.txt, flamegraph.svg |
| `memory_profiler.sh` | 内存泄漏检测和堆分析 | memcheck.log, massif_report.txt |
| `run_performance_tests.sh` | 一键运行所有测试 | 综合报告 |
| `benchmark_runner.cpp` | 基准测试框架 | CSV 报告 |
| `performance_utils.h` | 优化后的工具函数库 | 供项目使用 |

## 快速开始

```bash
# 一键运行所有测试
./tools/performance/run_performance_tests.sh 30

# 单独运行 CPU 分析
./tools/performance/perf_profiler.sh ./build/src/dcp 60

# 单独运行内存分析
./tools/performance/memory_profiler.sh ./build/src/dcp 60

# 编译并运行基准测试
cd tools/performance
cmake -B build . -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/run_benchmarks all
```

## 文件结构

```
tools/performance/
├── perf_profiler.sh           # CPU 性能分析脚本
├── memory_profiler.sh         # 内存分析脚本
├── run_performance_tests.sh   # 综合测试脚本
├── benchmark_runner.cpp       # 基准测试框架
├── run_benchmarks.cpp         # 基准测试用例
├── CMakeLists.txt             # 构建配置
├── results/                   # 测试结果输出目录
│   ├── perf.data
│   ├── hotspots.txt
│   ├── flamegraph.svg
│   └── *_benchmarks.csv
└── README.md                  # 本文件
```

## 优化工具使用

### 1. 快速三角函数

```cpp
#include "src/common/performance_utils.h"

auto& trig = aurora::performance::getFastTrig();
double sin_val = trig.fastSin(angle);
double cos_val = trig.fastCos(angle);
```

### 2. 优化的逆运动学

```cpp
aurora::performance::OptimizedLegIK ik(0.375, 0.385, 0.1);
double joints[6];
ik.compute(x, y, z, is_left, joints);
```

### 3. 无锁环形缓冲区

```cpp
aurora::performance::LockFreeRingBuffer<Data, 1024> buffer;
buffer.push(data);
Data value;
buffer.pop(value);
```

### 4. 性能计时

```cpp
SCOPED_TIMER("function_name");
// 或
AccumulatingTimer timer("name");
timer.start();
// ... code ...
timer.stop();
timer.print();
```

## 文档

详细优化指南请参阅: `docs/performance-optimization-guide.md`

## 依赖

- Linux perf 工具 (CPU 分析)
- Valgrind (内存分析)
- C++17 编译器

Ubuntu 安装命令:
```bash
sudo apt-get install linux-tools-common linux-tools-generic valgrind
```
