# 安装指南

本文档介绍如何在不同平台上安装 Aurora-Edge-Runtime。

## 目录

1. [系统要求](#1-系统要求)
2. [Docker 安装](#2-docker-安装)
3. [源码编译](#3-源码编译)
4. [systemd 服务](#4-systemd-服务)
5. [验证安装](#5-验证安装)

---

## 1. 系统要求

### 1.1 硬件要求

| 组件 | 最低配置 | 推荐配置 |
|-----|---------|---------|
| CPU | 4核 ARM64/x86_64 @ 1.5GHz | 8核 ARM64/x86_64 @ 2.0GHz |
| 内存 | 4GB | 8GB |
| 存储 | 50GB 可用空间 | 200GB SSD |
| 网络 | 1Mbps 上行 | 10Mbps 上行 |

### 1.2 软件要求

| 软件 | 版本     | 必需 |
|-----|--------|------|
| Ubuntu | 22.04+ | ✅ |
| ROS2 | Humble | ✅ |
| CMake | 3.22+  | ✅ |
| GCC | 11.4+  | ✅ |
| Boost | 1.74+  | ✅ |
| Docker | 20.10+ | 推荐 |

### 1.3 支持的平台

| 平台 | 架构 | 状态 |
|-----|------|------|
| Ubuntu 20.04 | x86_64 | ✅ 完全支持 |
| Ubuntu 22.04 | x86_64 | ✅ 完全支持 |

---

## 2. Docker 安装

Docker 安装是最简单的方式，适合快速部署和隔离环境。

### 2.1 前置要求

```bash
# 安装 Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# 安装 Docker Compose
sudo curl -L "https://github.com/docker/compose/releases/download/v2.20.0/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose

# 添加用户到 docker 组
sudo usermod -aG docker $USER
newgrp docker
```

### 2.2 获取镜像

```bash
# 从 Docker Hub 拉取（推荐）
docker pull aurora/edge-runtime:v1.1.2

# 或从源码构建
git clone https://github.com/your-org/aurora-edge-runtime.git
cd aurora-edge-runtime
docker build -t aurora-edge-runtime:v1.1.2 .
```

### 2.3 运行容器

```bash
# 使用 docker-compose（推荐）
docker-compose up -d

# 或使用 docker run
docker run -d \
  --name aurora_edge_runtime \
  --network host \
  --restart unless-stopped \
  -v /data/aer:/data/aer \
  -v $(pwd)/config:/app/config:ro \
  -e AER_MODE=humanoid \
  aurora-edge-runtime:v1.1.2
```

### 2.4 管理容器

```bash
# 查看日志
docker logs -f aurora_edge_runtime

# 进入容器
docker exec -it aurora_edge_runtime /bin/bash

# 停止容器
docker stop aurora_edge_runtime

# 重启容器
docker restart aurora_edge_runtime

# 删除容器
docker rm -f aurora_edge_runtime
```

---

## 3. 源码编译

源码编译适合需要自定义修改或调试的开发者。

### 3.1 安装依赖

```bash
# 更新包列表
sudo apt update

# 安装基础工具
sudo apt install -y \
    build-essential \
    cmake \
    git \
    wget

# 安装 ROS2 Humble
sudo apt install -y \
    software-properties-common
sudo add-apt-repository universe
sudo add-apt-repository restricted
sudo add-apt-repository multiverse
sudo apt update

sudo apt install -y \
    ros-humble-desktop \
    ros-humble-rosbag2-storage-mcap \
    python3-colcon-common-extensions \
    python3-rosdep

# 安装额外依赖
sudo apt install -y \
    libboost-all-dev \
    libssl-dev \
    libcurl4-openssl-dev

# 初始化 rosdep
sudo rosdep init
rosdep update
```

### 3.2 获取源码

```bash
# 克隆仓库
git clone https://github.com/your-org/aurora-edge-runtime.git
cd aurora-edge-runtime

# 检出发布版本
git checkout v1.1.2
```

### 3.3 编译项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_ROS2=ON \
    -DDOWNLOAD_ONNXRUNTIME=ON

# 编译（使用所有核心）
make -j$(nproc)
```

**编译产物位置：**
- `build/src/aer` - 主数据采集程序
- `build/src/robot_sim` - 机器人模拟器
- `build/tests/test_dcp_integration` - 集成测试

**注意**：本项目不需要 `make install`，可直接使用 `build/src/` 下的可执行文件。

### 3.4 编译选项

| 选项 | 默认值 | 说明 |
|-----|-------|------|
| `CMAKE_BUILD_TYPE` | `Release` | Debug/Release/RelWithDebInfo |
| `ENABLE_ROS2` | `ON` | 启用 ROS2 支持 |
| `ENABLE_RSCL` | `OFF` | 启用 RSCL 支持 |
| `DOWNLOAD_ONNXRUNTIME` | `ON` | 自动下载 ONNX Runtime |
| `BUILD_TESTS` | `ON` | 构建测试 |

### 3.5 常见编译问题

**问题**: 找不到 ROS2
```bash
# 解决方案
source /opt/ros/humble/setup.bash
```

**问题**: Boost 版本不兼容
```bash
# 解决方案
sudo apt remove libboost-all-dev
sudo apt install libboost1.74-all-dev
```

**问题**: ONNX Runtime 下载失败
```bash
# 解决方案：手动下载
cd 3rdparty
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.0/onnxruntime-linux-x64-1.16.0.tgz
tar -xzf onnxruntime-linux-x64-1.16.0.tgz
mv onnxruntime-linux-x64-1.16.0 onnxruntime
```

---

## 4. systemd 服务

systemd 服务适合长期运行的系统。

### 4.1 安装服务

```bash
# 进入项目目录
cd aurora-edge-runtime

# 执行安装脚本
sudo ./ops/prod/install-aer-service.sh install

# 查看服务状态
sudo systemctl status aer
```

### 4.2 配置服务

编辑配置文件：
```bash
sudo vim /etc/default/aer
```

```bash
# 运行模式
AER_MODE=humanoid

# 日志级别
AER_LOG_LEVEL=2

# 最大采集周期
AER_MAX_CYCLES=0
```

### 4.3 管理服务

```bash
# 启动服务
sudo systemctl start aer

# 停止服务
sudo systemctl stop aer

# 重启服务
sudo systemctl restart aer

# 开机自启动
sudo systemctl enable aer

# 查看日志
sudo journalctl -u aer -f
```

---

## 5. 验证安装

### 5.1 检查可执行文件

```bash
# 检查可执行文件
ls -lh build/src/aer build/src/robot_sim
# 应该输出可执行文件及其大小

# 运行主程序查看版本
./build/src/aer --version
```

### 5.2 检查依赖

```bash
# 检查 ROS2
ros2 --version
# 应该输出: ros2 2.x.x

# 检查话题
ros2 topic list
```

### 5.3 运行测试

```bash
# 运行集成测试
cd build
./tests/test_dcp_integration

# 或使用 CTest
ctest --output-on-failure
```

### 5.4 验证运行

#### 方式 1: 单进程运行

```bash
cd aurora-edge-runtime
source resource/scripts/setup.bash
./build/src/aer
```

#### 方式 2: 双进程运行

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

#### 方式 3: 使用启动脚本

```bash
cd aurora-edge-runtime
source resource/scripts/setup.bash
./launch/launch_dual_process.sh
```

## 6. 卸载

### 6.1 卸载 Docker 版本

```bash
# 停止并删除容器
docker stop aurora_edge_runtime
docker rm aurora_edge_runtime

# 删除镜像（可选）
docker rmi aurora-edge-runtime:v1.1.2
```

### 6.2 卸载源码版本

```bash
# 停止服务
sudo systemctl stop aer
sudo systemctl disable aer

# 删除服务
sudo ./ops/prod/install-aer-service.sh uninstall

# 删除构建文件
cd build
sudo make uninstall
cd ..
rm -rf build
```

## 7. 下一步

安装完成后，请阅读：

- [第一次运行](first-run.md)
- [配置指南](../user-guide/configuration/planner-config.md)
- [用户指南](../user-guide/index.md)
