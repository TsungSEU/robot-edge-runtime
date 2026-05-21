**Breadcrumbs:** [Docs](../../README.md) / [Hardware](../index.md) / [Robot Platforms](index.md) / Integration

# 平台集成指南

本文档提供 Aurora-Edge-Runtime 系统与机器人平台集成的详细指南。

## 集成概述

Aurora-Edge-Runtime 通过 ROS2 接口与机器人平台集成，支持两种运行模式：

| 模式 | 机器人类型 | 状态空间 | 动作空间 |
|------|------------|----------|----------|
| `humanoid` | 双足人形机器人 | 43维 | 3 维连续速度命令 |
| `auto` | 轮式机器人 | 25维 | 4 离散动作 |

## 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      Aurora-Edge-Runtime                       │
│                                                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │ 传感器接口   │  │   RL 规划器  │  │  采集执行器  │        │
│  │ (ROS2 Topic) │  │ (ONNX 模型)  │  │(环形缓冲)    │        │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘        │
│         │                  │                  │                │
└─────────┼──────────────────┼──────────────────┼────────────────┘
          │                  │                  │
          ▼                  ▼                  ▼
┌─────────────────────────────────────────────────────────────────┐
│                      机器人平台 HAL 层                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │  传感器驱动  │  │  电机驱动器  │  │  通信总线    │        │
│  └──────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────────┘
          │                  │                  │
          ▼                  ▼                  ▼
┌─────────────────────────────────────────────────────────────────┐
│                      硬件层                                     │
│  IMU  编码器  电机   力传感器   深度相机   CAN   SPI    I2C     │
└─────────────────────────────────────────────────────────────────┘
```

## 集成步骤

### 第一步: 硬件准备

#### 1.1 计算平台

确认计算平台满足最低要求：

| 检查项 | 最低 | 推荐 |
|--------|------|------|
| CPU | 4核 @ 1.5GHz | 8核 @ 2.0GHz |
| 内存 | 4GB | 16GB |
| 存储 | 50GB SSD | 256GB NVMe |

#### 1.2 传感器

确认以下传感器已安装并可用：

- [ ] IMU (9轴推荐)
- [ ] 关节编码器 (12-bit 以上)
- [ ] 足端传感器 (人形机器人可选)

### 第二步: 软件安装

#### 2.1 安装 ROS2

```bash
# 设置语言
sudo apt update && sudo apt install locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

# 添加 ROS2 APT 仓库
sudo apt install software-properties-common
sudo add-apt-repository universe
sudo apt update && sudo apt install curl -y
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.asc

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.asc] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

# 安装 ROS2 Humble
sudo apt update
sudo apt install ros-humble-desktop
```

#### 2.2 安装 Aurora-Edge-Runtime

```bash
# 克隆项目
git clone https://gitlab.orderseek.ai/icr11/dataengine/data-infra/Aurora/aurora-edge-runtime.git
cd aurora-edge-runtime

# 安装依赖
sudo apt install -y \
    libboost-all-dev \
    libeigen3-dev \
    libssl-dev \
    python3-colcon-common-extensions

# 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8

# 安装
sudo make install
```

### 第三步: 传感器驱动

#### 3.1 IMU 驱动

创建 `config/imu.yaml`:

```yaml
imu:
  type: ICM-42688-P  # 或 MPU6050, MPU9250
  interface: SPI
  device: /dev/spidev0.0
  rate: 200  # Hz

  # ROS2 参数
  frame_id: robot_imu
  topic: /robot/imu

  # 校准参数
  accelerometer:
    noise_density: 0.01
    random_walk: 0.0
  gyroscope:
    noise_density: 0.005
    random_walk: 0.0
```

启动 IMU 节点:

```bash
ros2 run imu_driver imu_publisher --ros-args --params-file config/imu.yaml
```

#### 3.2 编码器驱动

```bash
# CAN 编码器
ros2 run encoder_driver can_encoder_publisher --ros-args \
  --params-file config/encoders.yaml

# 或 SPI/I2C 编码器
ros2 run encoder_driver spi_encoder_publisher --ros-args \
  --params-file config/encoders.yaml
```

### 第四步: 里程计配置

#### 4.1 人形机器人里程计

```yaml
# config/humanoid_odom.yaml
odometry:
  type: EKF
  # 状态输入
  inputs:
    - joint_states  # 关节编码器
    - imu           # IMU 角速度
  # 输出
  outputs:
    - odom          # 里程计
  # 参数
  frequency: 50.0
  frame_id: odom
  child_frame_id: robot_base_link
```

#### 4.2 轮式机器人里程计

```yaml
# config/wheel_odom.yaml
diff_drive_controller:
  type: diff_drive_controller/DiffDriveController
  left_wheel_names: ["left_wheel_joint"]
  right_wheel_names: ["right_wheel_joint"]
  wheel_separation: 0.50
  wheel_radius: 0.10
  publish_rate: 50.0
```

### 第五步: 启动系统

#### 5.1 创建启动文件

创建 `launch/robot_bringup.launch.py`:

```python
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # IMU 驱动
        Node(
            package='imu_driver',
            executable='imu_publisher',
            name='imu_node',
            parameters=[{'config_file': 'config/imu.yaml'}]
        ),
        # 编码器驱动
        Node(
            package='encoder_driver',
            executable='encoder_publisher',
            name='encoder_node',
            parameters=[{'config_file': 'config/encoders.yaml'}]
        ),
        # 里程计
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_node',
            parameters=[{'config_file': 'config/ekf.yaml'}]
        ),
        # Aurora-Edge-Runtime
        Node(
            package='dcp',
            executable='dcp_node',
            name='dcp_node',
            parameters=[
                {'mode': 'humanoid'},  # 或 'auto'
                {'model_path': 'models/humanoid_ppo.onnx'},
                {'config_path': 'config/planner_weights.yaml'}
            ]
        ),
    ])
```

#### 5.2 启动命令

```bash
# 方式 1: 使用 launch 文件
ros2 launch aurora_edge_runtime robot_bringup.launch.py

# 方式 2: 直接运行
./src/dcp --mode humanoid
```

## 接口验证

### 验证传感器输出

```bash
# 检查 IMU
ros2 topic echo /robot/imu

# 检查里程计
ros2 topic echo /robot/odom

# 检查关节状态
ros2 topic echo /robot/joint_states
```

### 验证频率

```bash
# 检查话题频率
ros2 topic hz /robot/imu
ros2 topic hz /robot/odom
ros2 topic hz /robot/joint_states
```

预期频率:
- `/robot/imu`: 50Hz
- `/robot/odom`: 50Hz
- `/robot/joint_states`: 50-100Hz

## 故障排查

### 问题 1: 传感器数据缺失

```bash
# 检查节点是否运行
ros2 node list

# 检查话题是否存在
ros2 topic list

# 检查节点日志
ros2 node log <node_name>
```

### 问题 2: 数据频率过低

```bash
# 检查 CPU 占用
htop

# 调整发布频率
ros2 param set /imu_node publish_rate 100
```

### 问题 3: 里程计漂移

```bash
# 重新校准 IMU
ros2 service call /imu_node/calibrate

# 调整 EKF 参数
ros2 param set /ekf_node/imu_queue_size 10
```

## 认证检查清单

完成集成后，请使用此清单验证：

- [ ] 所有传感器正常工作
- [ ] 里程计输出稳定
- [ ] 话题频率满足要求
- [ ] Aurora-Edge-Runtime 正常启动
- [ ] 路径规划正常工作
- [ ] 数据采集触发正常

## 支持联系

如需集成支持，请联系：

- 📧 技术支持: support@example.com
- 📞 电话: +86-xxx-xxxx-xxxx
- 💬 工单: https://support.example.com
- 📖 文档: https://docs.aurora-runtime.com
