# 第一次运行

本文档指导您完成 Aurora-Edge-Runtime (AER) 的第一次运行。

## 目录

1. [启动前准备](#1-启动前准备)
2. [开发环境运行](#2-开发环境运行)
3. [生产环境部署](#3-生产环境部署)
4. [验证运行状态](#4-验证运行状态)
5. [下一步](#5-下一步)

---

## 1. 启动前准备

### 1.1 环境检查

```bash
# 检查 ROS2 环境
source /opt/ros/humble/setup.bash
ros2 --version

# 检查 RMW 实现
echo $RMW_IMPLEMENTATION
# 应该输出: rmw_fastrtps_cpp

# 如果没有，设置环境变量
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

### 1.2 配置检查

```bash
# 检查配置文件
ls -l config/planner_weights.yaml
ls -l config/app_config.json

# 检查模型文件
ls -l models/humanoid_ppo.onnx
# 如果不存在，下载模型
wget https://github.com/your-org/releases/download/v1.1.8/humanoid_ppo.onnx -P models/
```

### 1.3 创建数据目录

```bash
# 创建数据目录
mkdir -p /data/aer/{bags,enc,masked}

# 设置权限
chmod 755 /data/aer
```

---

## 2. 开发环境运行

开发环境使用 `aer` 命令进行快速开发和测试。

### 2.1 安装 aer 命令

```bash
# 进入项目目录
cd aurora-edge-runtime

# 安装 aer 命令
cd ops/dev
./install-aer.sh

# 如果提示 PATH 问题，添加到 PATH
export PATH="$HOME/.local/bin:$PATH"

# 验证安装
aer --help
```

### 2.2 启动机器人仿真器（可选）

**方式 1: 手动启动**

```bash
# 打开终端1 - 启动仿真器
cd aurora-edge-runtime
source resource/scripts/setup.bash
./build/src/robot_sim
```

**方式 2: 后台启动**

```bash
# 后台启动
cd aurora-edge-runtime
source resource/scripts/setup.bash
./build/src/robot_sim &

# 查看日志
tail -f /tmp/robot_sim.log
```

仿真器会发布以下话题：
- `/robot/odom` - 里程计数据
- `/robot/joint_states` - 关节状态
- `/robot/imu` - IMU数据

### 2.3 启动 AER 服务

```bash
# 启动 AER 服务
aer start

# 查看状态
aer status

# 查看日志
aer logs aer
```

**指定模式启动：**

```bash
# 自动驾驶模式
aer start --mode auto

# 人形机器人模式（默认）
aer start --mode humanoid

# 使用环境变量
AER_MODE=auto aer start
```

### 2.4 停止服务

```bash
# 停止 AER 服务
aer stop

# 停止 robot_sim
pkill robot_sim
```

---

## 3. 生产环境部署

生产环境使用 systemd 服务进行部署。

### 3.1 安装 systemd 服务

```bash
# 进入项目目录
cd aurora-edge-runtime

# 安装服务
cd ops/prod
sudo ./install-aer-service.sh install
```

### 3.2 配置服务

编辑配置文件：

```bash
# 编辑配置
vim ops/prod/aer.conf
```

**主要配置项：**

```bash
# 运行模式
AER_MODE=humanoid

# 模型路径
AER_MODEL_PATH=/path/to/models/humanoid_ppo.onnx

# 配置文件路径
AER_CONFIG_PATH=/path/to/config/planner_weights.yaml

# 日志级别
AER_LOG_LEVEL=2

# CPU 绑核
AER_CPU_AFFINITY=0

# 可视化
AER_ENABLE_VISUALIZATION=false

# S3 上传
AER_ENABLE_UPLOAD=true
AER_S3_BUCKET=your-bucket
```

### 3.3 启动服务

```bash
# 启动服务
sudo systemctl start aer

# 查看状态
sudo systemctl status aer

# 设置开机自启
sudo systemctl enable aer

# 查看日志
sudo journalctl -u aer -f
```

### 3.4 管理服务

```bash
# 停止服务
sudo systemctl stop aer

# 重启服务
sudo systemctl restart aer

# 重新加载配置
sudo systemctl reload aer

# 禁用开机自启
sudo systemctl disable aer
```

---

## 4. 验证运行状态

### 4.1 检查节点

```bash
# 开发环境
aer status

# 生产环境
sudo systemctl status aer

# 检查 ROS2 节点
ros2 node list
# 应该看到:
# /aer
# /robot_simulator (如果使用仿真器)
```

### 4.2 检查采集的数据

```bash
# 查看采集的 Rosbag 文件
ls -lh /data/aer/bags/

# 查看采集记录
cat /data/aer/bags/upload_record.json | python3 -m json.tool
```

### 4.3 可视化（可选）

```bash
# 启动 RViz2
ros2 launch aurora_edge_runtime robot_demo_visualization.launch.py
```

RViz2 中应该显示：
- 机器人模型
- 计划路径（绿色线条）
- 已采集路径（红色线条）
- 采集点标记（蓝色圆点）

### 4.4 查看日志

**开发环境：**

```bash
# 实时日志
aer logs aer

# 查看文件
tail -f /tmp/aer.log

# 查看最近的错误
grep ERROR /tmp/aer.log | tail -20
```

**生产环境：**

```bash
# systemd 日志
sudo journalctl -u aer -f

# 最近 100 行
sudo journalctl -u aer -n 100

# 查看错误
sudo journalctl -u aer -p err -n 20
```

---

## 5. 下一步

运行成功后，您可以：

### 5.1 调整配置

- [规划器配置](../user-guide/configuration/planner-config.md) - 调整 RL 参数
- [采集配置](../user-guide/configuration/collection-config.md) - 调整采集策略

### 5.2 启用可视化

**开发环境：**

```bash
# 使用命令行参数
aer start --mode humanoid

# 或查看配置
cat ops/prod/aer.conf | grep VISUALIZATION
```

**生产环境：**

```bash
# 编辑配置
vim ops/prod/aer.conf

# 设置
AER_ENABLE_VISUALIZATION=true

# 重启服务
sudo systemctl reload aer
```

### 5.3 配置数据上传

```bash
# 编辑 AWS 配置
vim config/app_config.json

# 设置 S3 参数
{
  "dataUpload": {
    "aws": {
      "enabled": true,
      "bucketName": "your-bucket",
      "endpointUrl": "your-endpoint"
    }
  }
}
```

或编辑 `ops/prod/aer.conf`：

```bash
AER_ENABLE_UPLOAD=true
AER_S3_BUCKET=your-bucket
```

### 5.4 生产部署

- [Docker 部署](../operations/deployment/docker.md)
- [边缘设备部署](../operations/deployment/edge-device.md)

### 5.5 深入了解

- `ops/README.md` - Ops 目录详细说明
- `ops/QUICKREF.md` - 快速参考
- `docs/AER_COMMAND.md` - aer 命令详细文档
- `docs/SERVICE_ARCHITECTURE.md` - 服务架构说明

---

## 常见问题

### Q: aer 命令找不到？

A: 确保已安装 aer 命令：

```bash
cd ops/dev
./install-aer.sh

# 添加到 PATH
export PATH="$HOME/.local/bin:$PATH"

# 验证
aer --help
```

### Q: 看不到 `/robot/odom` 话题？

A: 确保仿真器已启动：

```bash
# 检查仿真器进程
ps aux | grep robot_sim

# 检查话题
ros2 topic list | grep odom

# 重启仿真器
pkill robot_sim
./build/src/robot_sim &
```

### Q: 提示找不到模型文件？

A: 确保 ONNX 模型存在：

```bash
ls -l models/humanoid_ppo.onnx

# 如果不存在，下载
wget https://github.com/your-org/releases/download/v1.1.8/humanoid_ppo.onnx -P models/
```

### Q: 服务无法启动？

A: 检查可执行文件和权限：

```bash
# 检查可执行文件
ls -la build/src/aer

# 如果不存在，编译项目
mkdir -p build && cd build
cmake ..
make -j8

# 检查权限
chmod +x build/src/aer
```

### Q: PID 文件权限错误？

A: 清理 PID 文件：

```bash
# 删除 PID 文件
rm -f .pids/*.pid

# 重新启动
aer start
```

### Q: 采集没有触发？

A: 检查步态触发条件：

```bash
# 查看触发器日志（开发环境）
grep "GaitTrigger" /tmp/aer.log | tail -20

# 查看触发器日志（生产环境）
sudo journalctl -u aer | grep "GaitTrigger" | tail -20

# 检查步态参数
grep "min_step_distance" config/planner_weights.yaml
```

---

## 相关文档

- `ops/README.md` - Ops 目录详细说明
- `ops/QUICKREF.md` - 快速参考
- `docs/AER_QUICKSTART.md` - AER 快速入门
- `docs/AER_STARTUP_FLOW.md` - 启动流程详解
