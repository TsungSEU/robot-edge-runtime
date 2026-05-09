# 轮式机器人平台

本文档描述 Aurora-Edge-Runtime 系统支持的轮式机器人硬件平台规格和集成指南。

## 平台概述

Aurora-Edge-Runtime 的 `auto` 模式专为轮式自主移动机器人设计，适用于自动驾驶车辆、AMR (自主移动机器人) 等平台。

## 参考平台

### Google RT-1 (Robotics Transformer 1)

```
开发者: Google DeepMind
平台: Saycan 移动操作机器人
特点:
  - 基于 Transformer 的策略模型
  - 视觉-动作直接映射
  - 130k 真实世界演示数据
```

### 轮式 AMR 平台

| 型号 | 制造商 | 载重 | 续航 |
|------|--------|------|------|
| **MiR100** | Mobile Industrial Robots | 100kg | 10h |
| **Fetch** | Fetch Robotics | 15kg | 4h |
| **Husky** | Clearpath Robotics | 75kg | 6h |

## 机械规格

### 车体参数

| 参数 | 推荐值 | 小型 | 中型 | 大型 |
|------|--------|------|------|------|
| **尺寸 (L×W×H)** | 60×50×30 cm | 40×35×20 | 60×50×30 | 100×80×50 |
| **重量** | 20-50kg | 10kg | 30kg | 80kg |
| **载荷** | > 30kg | 10kg | 50kg | 200kg |
| **离地间隙** | > 5cm | 3cm | 5cm | 10cm |

### 轮系配置

#### 差速驱动

| 参数 | 推荐值 |
|------|--------|
| 驱动轮数量 | 2 或 4 |
| 轮径 | 15cm - 20cm |
| 轮距 | 40cm - 60cm |
| 最大速度 | 1.5 m/s |

#### 阿克曼转向

| 参数 | 推荐值 |
|------|--------|
| 前轮 | 转向轮 |
| 后轮 | 驱动轮 |
| 转向角 | ±30° |
| 最小转弯半径 | 0.5m |
| 最大速度 | 2.0 m/s |

## 驱动系统

### 电机配置

| 参数 | 差速驱动 | 阿克曼转向 |
|------|----------|------------|
| 电机类型 | 有刷/无刷直流电机 | 无刷直流电机 |
| 功率 | 50W - 100W × 4 | 100W - 200W × 2 |
| 额定电压 | 24V | 24V 或 48V |
| 减速比 | 1:20 - 1:50 | 1:10 - 1:30 |
| 驱动器 | H桥/ESC | ESC |

### 推荐电机

| 型号 | 功率 | 电压 | 应用 |
|------|------|------|------|
| **DJI M3508** | 70W | 24V | 差速 |
| **DJI M2006** | 50W | 24V | 轻载 |
| **Maxon EC-4pole** | 200W | 48V | 阿克曼 |
| **NeveRest** | 50W | 12V | 教育 |

### 编码器

| 参数 | 推荐值 |
|------|--------|
| 类型 | 增量式光电编码器 |
| 分辨率 | ≥ 512 PPR |
| 接口 | AB 相位或正交 |
| 位置 | 电机后轴或轮毂 |

## 传感器配置

### 必需传感器

#### IMU

| 参数 | 推荐值 |
|------|--------|
| 位置 | 车体中心 |
| 安装 | 水平, X轴向前 |
| 采样率 | ≥ 100Hz |
| 接口 | SPI 或 I2C |

#### 轮速编码器

| 参数 | 推荐值 |
|------|--------|
| 分辨率 | ≥ 512 PPR |
| 采样率 | ≥ 50Hz |
| 接口 | GPIO 或 CAN |

### 可选传感器

| 传感器 | 用途 | 推荐规格 |
|--------|------|----------|
| **2D 激光雷达** | 建图与导航 | 270° FOV, 10Hz |
| **深度相机** | 避障 | RGB-D, 30fps |
| **GPS** | 绝对定位 | CEP < 5m |
| **超声波** | 近距离避障 | 0.05-3m |

## 运动控制

### 差速控制

```
v_linear = (v_left + v_right) / 2
v_angular = (v_right - v_left) / wheel_base
```

### 阿克曼控制

```
steering_angle = atan(wheel_base * v_angular / v_linear)
wheel_speeds = 根据转向角计算内外轮差速
```

## 电气系统

### 电源配置

| 子系统 | 电压 | 电流 | 功耗 |
|--------|------|------|------|
| 计算 | 12V | 2A | 24W |
| 电机 (4个) | 24V | 10A | 240W |
| 传感器 | 5V | 0.5A | 2.5W |
| **总计** | - | - | **~270W** |

### 电池配置

| 参数 | 推荐值 |
|------|--------|
| 类型 | Li-Ion 或 Li-Po |
| 标称电压 | 24V (6S-7S) |
| 容量 | 10Ah - 20Ah |
| 续航 | 3-6 小时 (典型工作) |
| 充电 | 1-2 小时 |

## ROS2 Topics

### 标准 Topics

| Topic | 类型 | 频率 | 方向 | 说明 |
|-------|------|------|------|------|
| `/robot/odom` | nav_msgs/Odometry | 50Hz | pub | 里程计 |
| `/robot/cmd_vel` | geometry_msgs/Twist | 50Hz | sub | 速度命令 |
| `/robot/imu` | sensor_msgs/Imu | 50Hz | pub | IMU 数据 |
| `/robot/joint_states` | sensor_msgs/JointState | 50Hz | pub | 关节状态 |

### 驱动 Topics

| Topic | 类型 | 频率 | 说明 |
|-------|------|------|------|
| `/wheel/left/cmd` | std_msgs/Float64 | 50Hz | 左轮命令 |
| `/wheel/right/cmd` | std_msgs/Float64 | 50Hz | 右轮命令 |
| `/wheel/encoder` | sensor_msgs/JointState | 50Hz | 编码器反馈 |

## 里程计配置

### 融合配置

```
输入:
  - 轮速编码器 (x4)
  - IMU 加速度计
  - IMU 陀螺仪

输出:
  - robot_base_link 姿态
  - 速度 (线速度 + 角速度)
  - 位置 (x, y, yaw)

算法:
  - EKF (扩展卡尔曼滤波)
  - 或 robot_localization 包
```

### 配置文件

```yaml
diff_drive_controller:
  type: diff_drive_controller/DiffDriveController
  left_wheel_names: ['left_wheel_joint']
  right_wheel_names: ['right_wheel_joint']

  # 轮参数
  wheel_separation: 0.50  # m
  wheel_radius: 0.10      # m

  # 编码器参数
  encoder_resolution: 512  # PPR

  # 里程计参数
  pose_covariance_diagonal: [0.001, 0.001, 0.0, 0.0, 0.0, 0.001]
  twist_covariance_diagonal: [0.001, 0.0, 0.0, 0.0, 0.0, 0.001]

  # 发布频率
  publish_rate: 50.0
```

## 集成步骤

### 1. 硬件连接

```
┌────────────────────────────────────────────────────┐
│                   轮式机器人平台                      │
│                                                      │
│  ┌─────┐         ┌───────┐         ┌─────┐        │
│  │左轮 │────────▶│ ECU   │◀────────│右轮 │        │
│  └─────┘  CAN   └───────┘   CAN   └─────┘        │
│                      │                            │
│              ┌───────┴───────┐                   │
│              │   Jetson Nano  │                   │
│              │   或 Orin NX   │                   │
│              └───────────────┘                   │
└────────────────────────────────────────────────────┘
```

### 2. URDF 配置

```xml
<robot name="wheeled_robot">
  <!-- 轮体 -->
  <link name="base_link">
    <visual>
      <geometry>
        <box size="0.5 0.4 0.2"/>
      </geometry>
    </visual>
  </link>

  <!-- 左轮 -->
  <link name="left_wheel">
    <visual>
      <geometry>
        <cylinder radius="0.1" length="0.05"/>
      </geometry>
    </visual>
  </link>

  <!-- 右轮 -->
  <link name="right_wheel">
    <visual>
      <geometry>
        <cylinder radius="0.1" length="0.05"/>
      </geometry>
    </visual>
  </link>
</robot>
```

### 3. 启动系统

```bash
# 1. 启动机器人驱动
ros2 launch robot_bringup diff_drive.launch.py

# 2. 启动 Aurora-Edge-Runtime (auto 模式)
./src/dcp --mode auto

# 3. 启动可视化
ros2 launch aurora_edge_runtime visualization.launch.py
```

## 参考资源

- [Google DeepMind Saycan](https://www.deepmind.com/research/saycan)
- [ROS2 Diff Drive Controller](https://control.ros.org/doc/diff_drive_controller/)
- [Micro ROS 机器人](https://micro.ros.org/)
