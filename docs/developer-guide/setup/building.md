# 编译指南

本文档介绍 Aurora-Edge-Runtime 的编译系统和构建流程。

## 目录

1. [构建系统](#1-构建系统)
2. [编译选项](#2-编译选项)
3. [编译流程](#3-编译流程)
4. [交叉编译](#4-交叉编译)
5. [常见问题](#5-常见问题)

---

## 1. 构建系统

### 1.1 构建工具

Aurora-Edge-Runtime 使用 CMake 作为构建系统。

| 组件 | 说明 |
|-----|------|
| **CMake** | 3.22+ |
| **Make** | GNU Make 3.81+ |
| **GCC** | 9.0+ |
| **colcon** | ROS2 构建工具 |

### 1.2 项目结构

```
aurora-edge-runtime/
├── CMakeLists.txt          # 主 CMake 文件
├── src/                    # 源代码
│   ├── CMakeLists.txt
│   ├── rl_planning_infer/ # RL 规划推理（子模块）
│   ├── data_collection/
│   ├── state_machine/
│   ├── simulator/
│   └── common/
├── tests/                  # 测试代码
│   └── CMakeLists.txt
├── 3rdparty/               # 第三方库（子模块）
│   └── onnxruntime/
└── build/                  # 构建输出（生成）
```

### 1.3 依赖关系

```
┌─────────────────────────────────────────────────────────────┐
│                    Aurora-Edge-Runtime                        │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │   Core       │  │   Planner    │  │    Data      │        │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘        │
│         │                 │                 │                 │
│         └─────────────────┴─────────────────┘                 │
│                           │                                 │
│                    ┌──────▼───────┐                           │
│                    │ Data Colle-   │                           │
│                    │   Planner    │                           │
│                    └──────┬───────┘                           │
│                           │                                 │
│  ┌────────────────────────┴──────────────────────────────┐  │
│  │                     Dependencies                        │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────┐  │  │
│  │  │ ROS2      │  │ Boost     │  │ ONNX      │  │ AWS  │  │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────┘  │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. 编译选项

### 2.1 CMake 选项

| 选项 | 默认值 | 说明 |
|-----|-------|------|
| `CMAKE_BUILD_TYPE` | `Release` | Debug/Release/RelWithDebInfo |
| `ENABLE_ROS2` | `ON` | 启用 ROS2 支持 |
| `ENABLE_RSCL` | `OFF` | 启用 RSCL 支持 |
| `DOWNLOAD_ONNXRUNTIME` | `ON` | 自动下载 ONNX Runtime |
| `BUILD_TESTS` | `ON` | 构建测试 |

### 2.2 编译类型

| 类型 | 优化级别 | 调试信息 | 使用场景 |
|-----|---------|---------|---------|
| `Debug` | `-g -O0` | 完全 | 开发调试 |
| `Release` | `-O3 -DNDEBUG` | 无 | 生产部署 |
| `RelWithDebInfo` | `-O2 -g` | 部分 | 性能分析 |

### 2.3 配置示例

#### Debug 构建

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_ROS2=ON \
    -DDOWNLOAD_ONNXRUNTIME=ON \
    -DBUILD_TESTS=ON
```

#### Release 构建

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_ROS2=ON \
    -DDOWNLOAD_ONNXRUNTIME=OFF \
    -DBUILD_TESTS=OFF
```

---

## 3. 编译流程

### 3.1 标准编译

```bash
# 1. 进入项目目录
cd aurora-edge-runtime

# 2. 创建构建目录
mkdir -p build && cd build

# 3. 配置
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_ROS2=ON \
    -DDOWNLOAD_ONNXRUNTIME=ON

# 4. 编译
make -j$(nproc)

# 5. 安装（可选）
sudo make install
```

### 3.2 清理构建

```bash
# 清理构建产物
make clean

# 清理所有（包括 CMake 缓存）
make clean-all
rm -rf CMakeCache.txt CMakeFiles
```

### 3.3 增量编译

```bash
# CMake 会自动检测文件变化
# 只重新编译修改过的文件

# 强制重新编译特定模块
make -j$(nproc) dcp_core
```

---

## 4. 交叉编译

### 4.1 ARM64 交叉编译

```bash
# 安装交叉编译工具链
sudo apt install -y \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu

# 配置 CMake
cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_BUILD_TYPE=Release
```

### 4.2 Jetson 交叉编译

```bash
# 使用 Jetson 工具链
export PATH=/opt/nvidia-toolchain/bin:$PATH

cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
```

---

## 5. 常见问题

### Q: 找不到 ROS2？

A: 确保 ROS2 环境已加载：
```bash
source /opt/ros/humble/setup.bash
```

### Q: 链接错误？

A: 检查依赖库是否已安装：
```bash
sudo apt install libboost-all-dev
```

### Q: ONNX Runtime 下载失败？

A: 手动下载：
```bash
mkdir -p 3rdparty/onnxruntime
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-linux-x64-1.16.3.tgz
tar -xzf onnxruntime-linux-x64-1.16.3.tgz -C 3rdparty/
```

---

## 编译检查清单

编译前检查：

- [ ] ROS2 Humble 已安装
- [ ] 环境变量已配置
- [ ] 依赖库已安装
- [ ] 磁盘空间充足 (>5GB)

编译后验证：

- [ ] 可执行文件生成
- [ ] 测试全部通过
- [ ] 可以正常启动
