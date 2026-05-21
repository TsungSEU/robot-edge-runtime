**Breadcrumbs:** [Docs](../../README.md) / [Hardware](../index.md) / [Sensors](index.md) / Imu

# IMU 配置指南

本文档描述 Aurora-Edge-Runtime 系统中惯性测量单元 (IMU) 的配置和使用方法。

## IMU 概述

IMU (Inertial Measurement Unit) 是 Aurora-Edge-Runtime 的核心传感器，用于：

- **姿态估计** - 提供机器人姿态信息 (roll, pitch, yaw)
- **角速度测量** - 用于步态检测和运动分析
- **加速度测量** - 用于运动状态判断
- **里程计融合** - 与编码器融合提供精确位置估计

## 推荐型号

| 型号 | 轴数 | 接口 | 采样率 | 精度 | 价格 |
|------|------|------|--------|------|------|
| **ICM-42688-P** | 9轴 | SPI | 1kHz | ±0.01° | $ |
| **ICM-20948** | 9轴 | SPI/I2C | 1kHz | ±0.05° | $ |
| **BMI088** | 6轴 | SPI/I2C | 1kHz | ±0.01° | $ |
| **MPU9250** | 9轴 | I2C/SPI | 1kHz | ±0.1° | $ |
| **MPU6050** | 6轴 | I2C | 1kHz | ±0.1° | $ |

## 硬件连接

### I2C 接口

```
┌─────────────────────────────────────────────────────────┐
│                    Jetson Orin / Pi                       │
│  ┌─────────┐                                           │
│  │  I2C-1  │ (SDA, SCL)                                │
│  └────┬────┘                                           │
│       │                                                │
│       ▼                                                │
│  ┌─────────┐                                           │
│  │  IMU    │                                           │
│  │ MPU6050 │                                           │
│  │         │                                           │
│  │   INT   │ ──────────────────────────────────► GPIO  │
│  └─────────┘                                           │
└─────────────────────────────────────────────────────────┘
```

### SPI 接口 (推荐)

```
┌─────────────────────────────────────────────────────────┐
│                    Jetson Orin / Pi                       │
│  ┌─────────┐                                           │
│  │  SPI-0  │ (MOSI, MISO, SCLK, CS)                     │
│  └────┬────┘                                           │
│       │                                                │
│       ▼                                                │
│  ┌─────────┐                                           │
│  │  IMU    │                                           │
│  │ICM-20948│                                           │
│  │         │                                           │
│  │   INT   │ ──────────────────────────────────► GPIO  │
│  └─────────┘                                           │
└─────────────────────────────────────────────────────────┘
```

## 引脚配置

### Raspberry Pi 4

| 引脚 | 功能 | IMU 引脚 |
|------|------|----------|
| GPIO 2 | SDA (I2C-1) | SDA |
| GPIO 3 | SCL (I2C-1) | SCL |
| GPIO 9 | MISO (SPI-0) | MISO |
| GPIO 10 | MOSI (SPI-0) | MOSI |
| GPIO 11 | SCLK (SPI-0) | SCLK |
| GPIO 8 | CE0 (SPI-0) | CS |

### Jetson Orin

| 引脚 | 功能 | IMU 引脚 |
|------|------|----------|
| GPIO 18 | SPI1_MOSI | MOSI |
| GPIO 19 | SPI1_MISO | MISO |
| GPIO 20 | SPI1_SCK | SCK |
| GPIO 21 | SPI1_CS0 | CS |
| GPIO 28 | SPI1_MISO_CS1 | CS1 |

## ROS2 配置

### 1. 创建配置文件

创建 `config/imu.yaml`:

```yaml
# IMU 配置
imu:
  # 传感器类型
  type: ICM-42688-P  # 或 MPU6050, MPU9250, ICM-20948

  # 硬件接口
  interface: SPI  # SPI 或 I2C
  device: /dev/spidev0.0  # 或 /dev/i2c-1
  cs_pin: 8  # GPIO 片选引脚 (SPI)
  interrupt_pin: 23  # 中断引脚

  # 采样参数
  sample_rate: 200  # Hz
  ahrs: true  # 姿态解算
  fifo: true  # 使用 FIFO 缓冲

  # 校准参数
  accel_calib:
    x_offset: 0.0
    y_offset: 0.0
    z_offset: 0.0
    x_scale: 1.0
    y_scale: 1.0
    z_scale: 1.0

  gyro_calib:
    x_offset: 0.0
    y_offset: 0.0
    z_offset: 0.0
    x_scale: 1.0
    y_scale: 1.0
    z_scale: 1.0

  # ROS2 输出
  frame_id: robot_imu
  topic: /robot/imu
  publish_rate: 50  # Hz
```

### 2. 创建 URDF 描述

```xml
<robot name="robot">
  <!-- IMU Link -->
  <link name="imu_link">
    <visual>
      <geometry>
        <box size="0.03 0.03 0.01"/>
      </geometry>
      <material name="blue">
        <color rgba="0 0 1 1"/>
      </material>
    </visual>
    <inertial>
      <mass value="0.001"/>
      <origin xyz="0 0 0"/>
      <inertia ixx="1e-06" ixy="0" ixz="0" iyy="1e-06" iyz="0" izz="1e-06"/>
    </inertial>
  </link>

  <!-- IMU Joint -->
  <joint name="imu_joint" type="fixed">
    <parent link="torso_link"/>
    <child link="imu_link"/>
    <origin xyz="0 0 0.1" rpy="0 0 0"/>
  </joint>

  <!-- IMU Sensor -->
  <gazebo reference="imu_link">
    <sensor name="imu_sensor" type="imu">
      <plugin filename="libgazebo_ros_imu_sensor.so" name="imu_plugin">
        <topic_name>/robot/imu</topic_name>
        <body_name>imu_link</body_name>
        <update_rate>50</update_rate>
      </plugin>
      <always_on>true</always_on>
      <update_rate>50</update_rate>
      <visualize>true</visualize>
    </sensor>
  </gazebo>
</robot>
```

## 数据输出

### Message 格式

```cpp
// sensor_msgs/Imu
Header header
geometry_msgs/Quaternion orientation         # 姿态四元数
float64[9] orientation_covariance           # 姿态协方差
geometry_msgs/Vector3 angular_velocity       # 角速度 (rad/s)
float64[9] angular_velocity_covariance      # 角速度协方差
geometry_msgs/Vector3 linear_acceleration   # 线加速度 (m/s²)
float64[9] linear_acceleration_covariance  # 线加速度协方差
```

### 示例输出

```
header:
  stamp: 1710000000123456789
  frame_id: "robot_imu"
orientation:
  x: 0.001
  y: -0.002
  z: 0.999
  w: 0.042
angular_velocity:
  x: 0.0012
  y: 0.0008
  z: -0.0003
linear_acceleration:
  x: 0.02
  y: -0.05
  z: 9.81
```

## 校准

### 加速度计校准

```bash
# 启动校准程序
ros2 run imu_tools imu_calibrate

# 按照提示将 IMU 放置在 6 个方向:
# 1. 上 (Z+)
# 2. 下 (Z-)
# 3. 前 (X+)
# 4. 后 (X-)
# 5. 右 (Y+)
# 6. 左 (Y-)
```

### 陀螺仪校准

```bash
# 启动校准程序
ros2 run imu_tools gyro_calibrate

# 保持 IMU 静止 10 秒
```

### 保存校准参数

```bash
# 校准完成后，参数保存到
~/.ros/imu_calibration.yaml

# 复制到配置目录
cp ~/.ros/imu_calibration.yaml config/
```

## 常见问题

### 问题 1: 数据漂移

**症状**: 姿态角缓慢漂移

**原因**:
- 陀螺仪零偏未校准
- 温度变化
- 震动干扰

**解决方案**:
```yaml
# 启用温度补偿
imu:
  temp_comp: true
  temp_coeff: 0.01  # 温度系数

# 定期重新校准
ros2 service call /imu_node/reset_calibration
```

### 问题 2: 噪声过大

**症状**: 角速度/加速度数据抖动

**原因**:
- 采样率过低
- 滤波参数不当
- 机械振动

**解决方案**:
```yaml
# 增加采样率
sample_rate: 200  # Hz

# 启用低通滤波
lpf:
  enabled: true
  cutoff_freq: 30  # Hz
```

### 问题 3: 频率不稳定

**症状**: 发布频率低于预期

**原因**:
- CPU 占用过高
- 总线带宽不足
- 中断延迟

**解决方案**:
```bash
# 提高 CPU 优先级
sudo renice -5 -p $(pgrep -f imu_node)

# 使用 SPI 而非 I2C
interface: SPI
```

## 性能指标

| 指标 | 最低 | 推荐 |
|------|------|------|
| 更新频率 | 50Hz | 100Hz |
| 延迟 | < 10ms | < 5ms |
| 精度 (角度) | ±0.5° | ±0.05° |
| 精度 (角速度) | ±1°/s | ±0.1°/s |
| 精度 (加速度) | ±0.1g | ±0.01g |

## 参考资源

- [ICM-42688-P 数据手册](https://www.invensense.com/wp-content/uploads/2022/11/DS-000166-v1.0-ICM-42688-P.pdf)
- [MPU-6050 数据手册](https://store.invensense.com/datasheets/invensense/MPU-6050-Datasheet1.pdf)
- [ROS2 IMU 插件](https://github.com/ROS2/imu_tools)
