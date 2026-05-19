**Breadcrumbs:** [Docs](../README.md) / [Getting Started](index.md) / Index

# 快速入门

欢迎来到 Aurora-Edge-Runtime 快速入门指南。本章节将帮助您在 15 分钟内完成系统安装并运行第一个数据采集任务。

## 目录

1. [系统要求](#1-系统要求)
2. [安装](#2-安装)
3. [配置](#3-配置)
4. [第一次运行](#4-第一次运行)
5. [验证安装](#5-验证安装)
6. [下一步](#6-下一步)

## 1. 系统要求

### 硬件要求

| 组件 | 最低配置 | 推荐配置 |
|-----|---------|---------|
| CPU | 4核 ARM64/x86_64 @ 1.5GHz | 8核 ARM64/x86_64 @ 2.0GHz |
| 内存 | 4GB | 8GB |
| 存储 | 50GB 可用空间 | 200GB SSD |

### 软件要求

- **操作系统**: Ubuntu 22.04+
- **ROS2**: Humble
- **CMake**: 3.22+
- **GCC**: 11.4+
- **Boost**: 1.74+

## 2. 安装

### 方式一: Docker 安装（推荐）

```bash
# 克隆仓库
git clone https://github.com/your-org/aurora-edge-runtime.git
cd aurora-edge-runtime

# 构建镜像
docker build -t aurora-edge-runtime:v1.1.2 .

# 运行容器
docker-compose up -d
```

### 方式二: 源码编译

```bash
# 安装依赖
sudo apt update
sudo apt install -y \
    ros-humble-desktop \
    python3-colcon-common-extensions \
    cmake build-essential libboost-all-dev

# 克隆仓库
git clone https://github.com/your-org/aurora-edge-runtime.git
cd aurora-edge-runtime

# 编译
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 3. 配置

### 基本配置

编辑 `config/planner_weights.yaml`:

```yaml
# 选择运行模式
planner_mode: "humanoid"  # auto | humanoid

# 调整日志级别
DCP_LOG_LEVEL: 2  # 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
```

### 云端配置（可选）

编辑 `config/app_config.json`:

```json
{
  "dataUpload": {
    "aws": {
      "enabled": true,
      "bucketName": "your-bucket-name",
      "endpointUrl": "your-endpoint-url"
    }
  }
}
```

## 4. 第一次运行

### 使用机器人仿真器

#### 方式 1: 单进程运行（内置仿真器）

```bash
cd aurora-edge-runtime
source resource/scripts/setup.bash
./build/src/aer
```

#### 方式 2: 双进程运行（分离仿真器和采集器）

```bash
# 启动仿真器（终端1）
cd aurora-edge-runtime
source resource/scripts/setup.bash
./build/src/robot_sim

# 启动数据采集（终端2）
cd aurora-edge-runtime
source resource/scripts/setup.bash
./build/src/aer
```

#### 方式 3: 使用启动脚本（推荐）

```bash
cd aurora-edge-runtime
source resource/scripts/setup.bash
./launch/launch_dual_process.sh
```

### 使用真实机器人

确保机器人已连接并发布必要的话题：

```bash
# 检查话题
ros2 topic list | grep -E "(odom|joint_states|imu)"

# 启动数据采集
cd aurora-edge-runtime
source resource/scripts/setup.bash
export AER_MODE=humanoid
./build/src/aer
```

## 5. 验证安装

### 检查节点

```bash
ros2 node list
# 应该看到:
# /aer
# /robot_simulator
```

### 检查话题

```bash
ros2 topic list
# 应该看到:
# /robot/odom
# /robot/joint_states
# /robot/cmd_vel
# /planned_path
# /collected_path
```

### 可视化

```bash
# 启动 RViz2
ros2 launch aurora_edge_runtime robot_demo_visualization.launch.py
```

## 6. 下一步

安装完成后，您可能想要：

- 📖 阅读完整 [用户指南](../user-guide/index.md)
- 🔧 了解 [配置选项](../user-guide/configuration/planner-config.md)
- 🏃 尝试 [快速示例](./quick-examples.md)
- 🐛 遇到问题？查看 [常见问题](../faq/index.md)

## 获取帮助

- 📧 邮件支持: support@example.com
- 💬 讨论: [GitHub Discussions](https://github.com/your-org/aurora-edge-runtime/discussions)
- 🐛 问题反馈: [GitHub Issues](https://github.com/your-org/aurora-edge-runtime/issues)
