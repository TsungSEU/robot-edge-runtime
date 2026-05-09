# 人形机器人平台

本文档描述 Aurora-Edge-Runtime 系统支持的人形机器人硬件平台规格和集成指南。

## 平台概述

Aurora-Edge-Runtime 的 `humanoid` 模式专为双足人形机器人设计，支持基于步态的智能数据采集。参考平台包括：

- **智元机器人 灵犀 X2** (AgiBot X2 Ultra) - 高度集成人形机器人平台（推荐）
- **Google RT-2** - 通用机器人 Transformer 平台
- **Tesla Optimus** - 特斯拉人形机器人
- **Figure 01** - Figure AI 人形机器人

## 机械规格

### 机身参数

| 参数 | 推荐规格 | AgiBot X2 | Tesla Optimus |
|------|----------|-----------|---------------|
| **身高** | 1.2m - 1.7m | 1.31m | 1.73m |
| **体重** | < 55kg | 39kg | 56kg |
| **臂展** | 0.6m - 0.8m | 558mm | N/A |
| **上半身比例** | 45% | N/A | 50% |

### 下肢规格

| 参数 | 推荐规格 | 说明 |
|------|----------|------|
| **腿长** | 70cm ± 5cm | 上腿 + 下腿 |
| **上腿长** | 35cm | 髋关节到膝关节 |
| **下腿长** | 35cm | 膝关节到踝关节 |
| **髋宽** | 10cm ± 2cm | 影响稳定性 |
| **足长** | 24cm - 28cm | 影响步长 |
| **足宽** | 12cm - 15cm | 影响稳定性 |

### 自由度分布

| 部位 | DOF | 关节配置 |
|------|-----|----------|
| **单腿** | 6 | 髋偏航(1) + 髋滚转(1) + 髋俯仰(1) + 膝俯仰(1) + 踝俯仰(1) + 踝滚转(1) |
| **双腿总计** | 12 | - |
| **全身** | 20-24 | 包含手臂和头部 |

## 关节执行器

### 推荐电机规格

| 关节 | 推荐扭矩 | 峰值扭矩 | 转速 | 说明 |
|------|----------|----------|------|------|
| 髋偏航 | 23Nm | 40Nm | 50rpm | 高扭矩需求 |
| 髋滚转 | 23Nm | 40Nm | 50rpm | 高扭矩需求 |
| 髋俯仰 | 23Nm | 40Nm | 50rpm | 高扭矩需求 |
| 膝俯仰 | 23Nm | 40Nm | 50rpm | 高扭矩需求 |
| 踝俯仰 | 11Nm | 20Nm | 50rpm | 标准扭矩 |
| 踝滚转 | 11Nm | 20Nm | 50rpm | 标准扭矩 |

### 电机驱动配置

```
电压: 24V - 48V DC
电流: 持续 20A, 峰值 40A
总线: CAN 2.0 / CAN FD
控制频率: 1kHz
反馈: 14-bit 绝对编码器 + 电流反馈
```

## 步态参数

### 基础步态

| 参数 | 推荐值 | 范围 | 说明 |
|------|--------|------|------|
| **步长** | 0.25m | 0.15m - 0.40m | 行走效率 |
| **步高** | 0.05m | 0.02m - 0.10m | 跨越障碍 |
| **步频** | 1.25 Hz | 0.8 - 2.0 Hz | 步/秒 |
| **步周期** | 0.8s | 0.5s - 1.25s | 单步持续时间 |
| **行走速度** | 0.3 m/s | 0.1 - 0.5 m/s | 前进速度 |

### 步态相位

```
相位 0.0 - 0.4π: 右脚支撑, 左脚摆动
相位 0.4π - 0.6π: 双脚支撑 (稳定期)
相位 0.6π - 1.4π: 左脚支撑, 右脚摆动
相位 1.4π - 1.6π: 双脚支撑 (稳定期)
相位 1.6π - 2π: 右脚支撑, 左脚摆动
```

## 传感器配置

### IMU 安装

| 参数 | 推荐配置 |
|------|----------|
| **位置** | 躯干中心，接近质心 |
| **方向** | X向前, Y向左, Z向上 |
| **安装** | 减震安装，避免振动干扰 |

### 编码器配置

| 位置 | 编码器类型 | 分辨率 | 精度 |
|------|------------|--------|------|
| 关节 | 14-bit 绝对值 | 16384 步/圈 | ±0.02° |
| 足端 | 接近传感器 | 1mm | ±1mm |

### 足端传感器

| 传感器 | 用途 | 推荐型号 |
|--------|------|----------|
| 力传感器 | 接触力检测 | SingleTap S-40 |
| 压力阵列 | 压力分布 | Tekscan F-Scan |
| 接近传感器 | 着地检测 | VL6180X |

## 运动学参数

### DH 参数

参考坐标系定义：

```
Link    α    a    d    θ
-------------------------------------
CoM     0    0    0    θ_base
Hip     0    0    d1   θ_hip_yaw
Hip     -π/2  0    0    θ_hip_roll
Hip     0    a1   0    θ_hip_pitch + π/2
Knee    0    a2   0    θ_knee_pitch
Ankle   0    a3   0    θ_ankle_pitch + π/2
Ankle   π/2   0    0    θ_ankle_roll
Foot    0    0    d2   0
```

其中：
- `d1 = 35cm` (髋高)
- `a1 = 0cm` (髋偏移)
- `a2 = 35cm` (上腿长)
- `a3 = 35cm` (下腿长)
- `d2 = -5cm` (踝偏移)

### 工作空间

| 参数 | 值 |
|------|-----|
| 前后范围 | ±0.5m |
| 左右范围 | ±0.3m |
| 高度范围 | 0.6m - 1.2m |
| 最大抬腿高度 | 0.2m |

## 参考平台: 智元 灵犀 X2 (AgiBot X2 Ultra)

### 硬件规格

```
制造商: 智元机器人 (Zhiyuan Robotics)
型号: AgiBot X2 Ultra (旗舰版)

物理参数:
  身高: 1.31m
  体重: 39kg
  尺寸: 1310(H) × 460(W) × 210(L) mm
  单臂展: 558mm (不含末端执行器)
  环境温度: -10℃ ~ 40℃

自由度配置 (30 DOF):
  颈部: 1 DOF
  单手臂: 7 DOF (共 14 DOF)
  腰部: 3 DOF
  单腿: 6 DOF (共 12 DOF)

性能参数:
  关节峰值扭矩: 120 N·m
  最大速度: 1.8 m/s
  日常使用速度: ≤ 0.8 m/s
  最大负载: 3kg (特定姿态)
  全空间负载: ≤1kg (不含末端执行器)

能源动力:
  电池能量: 约 421 Wh
  续航时间: 0.5 m/s 连续行走约 2 小时
  补能方式: 直充/换电/自动充电(可选)
  充电时间: ≤ 1.5 小时
  充电输入: 100-220V AC
  充电器输出: 54.6V 10A

智控参数:
  基础算力板: RK3588s + RK3588
  高算力板: Orin NX 16GB 157 TOPS
  二次开发: 支持 AimDK

硬件接口:
  USB Host: Type-A×2, Type-C×2
  以太网: RJ45×2
  供电接口: 12V/3A×1, 48V/5A×1

通信方式:
  WiFi、蓝牙、4G/5G 模块

交互模块:
  语音: 麦克风阵列、迷你无线麦克风、扬声器
  显示: 交互屏、灯效
  触摸: 头部触摸传感器

感知系统:
  RGB 摄像头: 交互RGB、前视双目RGB、后视RGB
  RGB-D 摄像头: 具备
  3D 激光雷达: 具备
```

### 自由度分布

| 部位 | DOF | 说明 |
|------|-----|------|
| 颈部 | 1 | 头部俯仰/偏航 |
| 单臂 | 7 | 肩3 + 肘1 + 腕3 |
| 双臂 | 14 | - |
| 腰部 | 3 | 腰部多自由度 |
| 单腿 | 6 | 髋3 + 膝1 + 踝2 |
| 双腿 | 12 | - |
| **总计** | **30** | - |

### 传感器配置

```
视觉系统:
  - 交互 RGB 摄像头
  - 前视双目 RGB 摄像头
  - 后视 RGB 摄像头
  - RGB-D 深度相机
  - 3D 激光雷达

交互感知:
  - 头部触摸传感器
  - 麦克风阵列
  - 迷你无线麦克风
  - 扬声器

本体感知:
  - 关节编码器
  - IMU
  - (可选) 足端力传感器
```

### 与 Aurora-Edge-Runtime 集成

```
Aurora-Edge-Runtime         AgiBot X2
        │                        │
        ├── ROS2 Topics ─────────┤
        │   /robot/odom          │
        │   /robot/joint_states  │
        │   /robot/imu           │
        │                        │
        ├── 传感器数据 ──────────┤
        │   RGB/RGB-D 相机       │
        │   3D 激光雷达          │
        │   触摸传感器           │
        │                        │
        └── 控制指令 ────────────┤
            步态规划             │
            数据采集触发         │
```

### 配置文件

创建 `config/agibot_x2.yaml`:

```yaml
robot:
  type: agibot_x2
  manufacturer: zhiyuan_robotics
  dof: 30

  # 身体参数
  body:
    height: 1.31              # m
    weight: 39                # kg
    arm_span: 0.558           # m

  # 自由度分布
  dof_distribution:
    neck: 1
    waist: 3
    arm_per_side: 7           # 每臂 7 DOF
    leg_per_side: 6           # 每腿 6 DOF

  # 腿部几何
  leg_geometry:
    upper_leg_length: 0.35    # m (估算)
    lower_leg_length: 0.35    # m (估算)
    hip_width: 0.10           # m (估算)

  # 步态参数
  gait:
    step_length: 0.25         # m (可调)
    step_height: 0.05         # m (可调)
    step_duration: 0.8        # s (可调)
    walk_speed: 0.5           # m/s (最大)

  # 性能参数
  performance:
    max_speed: 1.8            # m/s
    daily_speed: 0.8          # m/s
    peak_torque: 120          # N·m
    max_payload: 3            # kg (特定姿态)
    full_space_payload: 1     # kg

  # 传感器
  sensors:
    cameras:
      - type: rgb
        location: interactive
      - type: rgb_stereo
        location: front
      - type: rgb
        location: rear
      - type: rgb_d
        location: front
    lidar:
      type: 3d
    imu:
      type: builtin
    touch:
      - head

  # 计算
  compute:
    primary: RK3588s + RK3588
    accelerator: Orin NX 16GB 157 TOPS
    development_sdk: AimDK

  # 通信
  communication:
    wifi: true
    bluetooth: true
    lte: true
    ethernet_ports: 2

  # 电源
  power:
    battery_capacity: 421     # Wh
    runtime: 2                 # h @ 0.5 m/s
    charge_time: 1.5           # h
    charger_output: "54.6V 10A"

  # 接口
  interfaces:
    usb_type_a: 2
    usb_type_c: 2
    ethernet_rj45: 2
    power_12v: 1
    power_48v: 1
```

---

## 参考平台: Google RT-2

### 硬件规格

```
开发者: Google DeepMind
类型: 通用人形机器人平台
特点:
  - 基于 Robotics Transformer 2
  - 视觉-语言-动作 (VLA) 模型
  - 500+ 万个真实机器人训练样本
  - 支持复杂操作和推理
```

### 与 Aurora-Edge-Runtime 集成

```
 Aurora-Edge-Runtime       RT-2 模型
        │                    │
        ├── 传感器数据 ────────┤
        ├── 状态反馈 ──────────┤
        └── 采集策略 ──────────┤
                │             │
                ▼             ▼
            数据采集触发 ────┼──> 策略执行
```

## 电气系统

### 电源分配

| 子系统 | 电压 | 电流 | 功耗 |
|--------|------|------|------|
| 计算 | 12V | 3A | 36W |
| 电机 (12个) | 48V | 20A | 960W |
| 传感器 | 5V | 1A | 5W |
| 通信 | 5V | 0.5A | 2.5W |
| **总计** | - | - | **~1kW** |

### 电池配置

| 参数 | 推荐值 |
|------|--------|
| 类型 | Li-Ion 18650 或 Li-Po |
| 标称电压 | 48V (13S) |
| 容量 | 20Ah - 40Ah |
| 能量 | ~1kWh - 2kWh |
| 续航 | 1-2 小时 (典型工作) |
| 充电时间 | 1-2 小时 |

## 通信架构

### 内部总线

```
┌─────────────────────────────────────────────────────────┐
│                     CAN-FD Bus                          │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐    │
│  │ 电机1   │  │ 电机2   │  │ 电机3   │  │ 电机4   │    │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘    │
│  ...  ...  ...  ...  ...  ...  ...  ...  ...  ...      │
└─────────────────────────────────────────────────────────┘
                          ▲
                          │
                    ┌─────────┐
                    │   ECU   │
                    │(Orin NX)│
                    └─────────┘
```

### ROS2 Topics

| Topic | 类型 | 频率 | 说明 |
|-------|------|------|------|
| `/robot/odom` | nav_msgs/Odometry | 50Hz | 里程计 |
| `/robot/joint_states` | sensor_msgs/JointState | 100Hz | 关节状态 |
| `/robot/imu` | sensor_msgs/Imu | 50Hz | IMU 数据 |
| `/robot/cmd_vel` | geometry_msgs/Twist | 50Hz | 速度命令 |

## 集成步骤

### 1. 硬件连接

```
电机 → CAN Bus → ECU (Jetson Orin)
          ↓
      传感器 → SPI/I2C/GPIO
          ↓
      ECU 处理 → ROS2 Topics
```

### 2. 配置文件

创建 `config/humanoid_robot.yaml`:

```yaml
robot:
  type: humanoid
  dof: 12

  # 腿部几何
  leg_geometry:
    upper_leg_length: 0.35  # m
    lower_leg_length: 0.35  # m
    hip_width: 0.10         # m
    foot_height: 0.05       # m

  # 步态参数
  gait:
    step_length: 0.25       # m
    step_height: 0.05       # m
    step_duration: 0.8      # s
    walk_speed: 0.3         # m/s

  # 传感器
  sensors:
    imu:
      type: ICM-42688-P
      interface: SPI
      rate: 200
    encoders:
      resolution: 14        # bit
      interface: CAN
```

### 3. 启动系统

```bash
# 1. 启动机器人
ros2 launch robot_bringup robot_driver.launch.py

# 2. 启动 Aurora-Edge-Runtime
./src/dcp --mode humanoid

# 3. 启动可视化
ros2 launch aurora_edge_runtime visualization.launch.py
```

## 参考资源

- [智元机器人官网](https://www.zhiyuan-robot.com/)
- [Google DeepMind Robotics](https://www.deepmind.com/research/team/robotics)
- [Tesla Optimus](https://www.tesla.com/optimus)
- [T-Motor 电机选型指南](https://www.tmotor.com)
