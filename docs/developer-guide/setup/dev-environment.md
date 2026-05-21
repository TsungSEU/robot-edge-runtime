**Breadcrumbs:** [Docs](../../README.md) / [Developer Guide](../index.md) / [Setup](index.md) / Dev Environment

# 开发环境搭建

本文档介绍如何搭建 Aurora-Edge-Runtime 的开发环境。

## 目录

1. [系统要求](#1-系统要求)
2. [基础环境安装](#2-基础环境安装)
3. [开发工具配置](#3-开发工具配置)
4. [IDE 配置](#4-ide-配置)
5. [验证环境](#5-验证环境)

---

## 1. 系统要求

### 1.1 操作系统

| 系统 | 版本 | 状态 |
|-----|------|------|
| Ubuntu | 20.04 LTS | ✅ 推荐 |
| Ubuntu | 22.04 LTS | ✅ 支持 |
| ROS2 | Humble | ✅ 必需 |

### 1.2 硬件要求

| 组件 | 最低 | 推荐 |
|-----|------|------|
| CPU | 4核 @ 2.0GHz | 8核 @ 3.0GHz |
| 内存 | 8GB | 16GB |
| 存储 | 100GB SSD | 500GB NVMe |

---

## 2. 基础环境安装

### 2.1 安装 ROS2 Humble

```bash
# 添加 ROS2 APT 仓库
sudo apt update
sudo apt install software-properties-common
sudo add-apt-repository universe
sudo add-apt-repository restricted
sudo add-apt-repository multiverse

# 添加 ROS2 GPG 密钥
sudo apt update && sudo apt install curl
curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.asc

# 添加 ROS2 仓库
sudo sh -c 'echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.asc] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" > /etc/apt/sources.list.d/ros2.list'

# 安装 ROS2 Humble
sudo apt update
sudo apt install -y ros-humble-desktop python3-rosdep

# 初始化 rosdep
sudo rosdep init
rosdep update
```

### 2.2 安装构建工具

```bash
# 安装构建依赖
sudo apt install -y \
    build-essential \
    cmake \
    git \
    wget \
    python3-pip \
    python3-venv

# 安装 colcon (ROS2 构建工具)
sudo apt install -y \
    python3-colcon-common-extensions \
    python3-colcon-mixin-release \
    python3-colcon-mixin-vcs

# 安装额外 Python 包
pip3 install \
    pytest \
    pytest-cov \
    black \
    isort \
    mypy \
    flake8
```

### 2.3 安装项目依赖

```bash
# 安装 Boost
sudo apt install -y libboost-all-dev

# 安装 ONNX Runtime（可选，系统会自动下载）
# 或手动安装
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.0/onnxruntime-linux-x64-1.16.0.tgz
tar -xzf onnxruntime-linux-x64-1.16.0.tgz
sudo mv onnxruntime-linux-x64-1.16.0 /opt/onnxruntime
```

---

## 3. 开发工具配置

### 3.1 配置环境变量

创建 `~/aurora_env.sh`:

```bash
#!/bin/bash
# Aurora-Edge-Runtime 开发环境

# ROS2 环境
source /opt/ros/humble/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export ROS_DOMAIN_ID=0

# 项目路径
export AURORA_ROOT=/path/to/aurora-edge-runtime
export AURORA_BUILD=$AURORA_ROOT/build
export AURORA_INSTALL=$AURORA_ROOT/install
export AURORA_MODE=humanoid

# 路径配置
export PATH=$AURORA_INSTALL/bin:$PATH
export LD_LIBRARY_PATH=$AURORA_INSTALL/lib:$LD_LIBRARY_PATH
export PYTHONPATH=$AURORA_INSTALL/lib/python3.8/site-packages:$PYTHONPATH

# 开发工具
export CMAKE_BUILD_PARALLEL_LEVEL=8
export MAKEFLAGS="-j8"

# 日志配置
export DCP_LOG_LEVEL=0  # DEBUG1
export RCUTILS_LOG_LEVEL=DEBUG
```

```bash
# 加载环境
source ~/aurora_env.sh
```

### 3.2 配置 Git

```bash
# 配置用户信息
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"

# 配置行尾
git config --global core.autocrlf input

# 配置合并工具
git config --global merge.tool vimdiff
```

### 3.3 配置 clang-format

创建 `.clang-format`:

```yaml
BasedOnStyle: Google
Language: Cpp
Standard: c++17
IndentWidth: 2
ColumnLimit: 100
```

---

## 4. IDE 配置

### 4.1 VS Code

推荐插件：

```json
{
  "recommendations": [
    "ms-vscode.cpptools",
    "ms-vscode.cmake-tools",
    "twxs.cmake",
    "ros-robotics.ros",
    "ms-python.python",
    "eamodio.gitlens",
    "yzhang.markdown-all-in-one"
  ]
}
```

工作区配置：

```json
{
  "cmake.configureArgs": [
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DENABLE_ROS2=ON",
    "-DDOWNLOAD_ONNXRUNTIME=ON"
  ],
  "ros.distro": "humble"
}
```

### 4.2 CLion

1. 打开项目
2. 设置 CMake options
3. 配置环境变量
4. 设置代码风格

### 4.3 Vim/Neovim

配置 `~/.vimrc`:

```vim
" C++
autocmd FileType cpp setlocal shiftwidth=2
autocmd FileType cpp setlocal tabstop=2
autocmd FileType cpp setlocal expandtab

" 语法高亮
syntax on
filetype plugin on
```

---

## 5. 验证环境

### 5.1 检查工具链

```bash
# 检查 CMake
cmake --version  # 应该 >= 3.22

# 检查 GCC
gcc --version    # 应该 >= 9.0

# 检查 Python
python3 --version  # 应该 >= 3.8

# 检查 ROS2
ros2 --version
```

### 5.2 编译项目

```bash
cd aurora-edge-runtime

# 创建构建目录
mkdir -p build && cd build

# 配置
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_ROS2=ON \
    -DDOWNLOAD_ONNXRUNTIME=ON \
    -DBUILD_TESTS=ON

# 编译
make -j$(nproc)

# 运行测试
ctest --output-on-failure
```

### 5.3 验证运行

```bash
# 设置环境变量
cd /path/to/aurora-edge-runtime
source resource/scripts/setup.bash

# 方式 1: 单进程运行（内置仿真器）
./build/src/aer

# 方式 2: 双进程运行
# 终端1 - 启动仿真器
./build/src/robot_sim
# 终端2 - 启动数据采集
./build/src/aer
```

---

## 开发工作流

### 创建功能分支

```bash
git checkout -b feature/your-feature
```

### 进行开发

```bash
# 编辑代码
vim src/your_file.cpp

# 编译
cd build && make -j$(nproc)

# 测试
cd build && ctest
```

### 提交代码

```bash
# 格式化代码
cd build
make format

# 提交
git add .
git commit -m "feat: add your feature"
```

### 代码审查

```bash
# 推送到远程
git push origin feature/your-feature

# 创建 Pull Request
# 在 GitHub 上操作
```

---

## 常见问题

### Q: ROS2 导入失败？

A: 确保加载了 ROS2 环境：
```bash
source /opt/ros/humble/setup.bash
```

### Q: 找不到 Boost？

A: 安装 Boost 开发包：
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

## 延伸阅读

- [编译指南](building.md)
- [测试指南](testing.md)
- [IDE配置](ide-setup.md)
- [贡献指南](../contributing/contributing.md)
