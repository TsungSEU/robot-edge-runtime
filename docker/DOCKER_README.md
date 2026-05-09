# Docker环境说明

## 概述

本项目提供了一个预配置的Docker环境，包含Ubuntu 22.04系统以及预装的CMake 3.20.4和Protobuf 3.6.1，用于构建和运行自动驾驶数据闭环系统。

## 环境包含

- Ubuntu 22.04
- CMake 3.20.4
- Protobuf 3.6.1
- 基本构建工具（build-essential, git, wget等）

## 使用方法

### 方法1：使用Docker命令

1. 构建镜像：
   ```bash
   docker build -t ad_data_closed_loop_env .
   ```

2. 运行容器：
   ```bash
   docker run -it --name ad_data_closed_loop_container -v $(pwd):/workspaces/ad_data_closed_loop ad_data_closed_loop_env
   ```

### 方法2：使用Docker Compose（推荐）

1. 构建并启动服务：
   ```bash
   docker-compose up -d
   ```

2. 进入容器：
   ```bash
   docker-compose exec ad_data_closed_loop_dev /bin/bash
   ```

3. 停止服务：
   ```bash
   docker-compose down
   ```

## 在容器中编译项目

进入容器后，可以按照以下Step编译项目：

```bash
cd /workspaces/ad_data_closed_loop/src
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 验证安装

可以通过以下命令验证关键组件是否正确安装：

```bash
cmake --version
protoc --version
```

## 自定义

如果需要添加更多依赖或修改环境，可以直接编辑[Dockerfile](file:///workspaces/ad_data_closed_loop/docker/Dockerfile)并重新构建镜像。