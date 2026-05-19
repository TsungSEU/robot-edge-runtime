**Breadcrumbs:** [Docs](../README.md) / [Hardware](index.md) / Index

# 硬件文档

本章节面向硬件工程师和系统集成商，提供 Aurora-Edge-Runtime 的硬件需求、兼容性和集成指南。

## 目录

### 硬件需求

| 文档 | 说明 |
|------|------|
| [最低配置](requirements/minimum.md) | 最低硬件配置要求 |
| [推荐配置](requirements/recommended.md) | 推荐硬件配置规格 |
| [兼容性列表](requirements/compatibility.md) | 已验证兼容硬件列表 |

### 机器人平台

| 文档 | 说明 |
|------|------|
| [人形机器人](robot-platforms/humanoid.md) | 双足人形机器人平台规格 |
| [轮式机器人](robot-platforms/wheeled.md) | 轮式移动机器人平台规格 |
| [平台集成](robot-platforms/integration.md) | 硬件平台集成指南 |

### 传感器

| 文档 | 说明 |
|------|------|
| [IMU配置](sensors/imu.md) | 惯性测量单元配置指南 |
| [里程计](sensors/odom.md) | 里程计配置指南 |
| [相机](sensors/camera.md) | 相机配置（可选） |

---

## 硬件要求概览

### 计算平台

| 组件 | 最低配置 | 推荐配置 |
|-----|---------|---------|
| **CPU** | 4核 ARM64/x86_64 @ 1.5GHz | 8核 ARM64/x86_64 @ 2.0GHz |
| **指令集** | NEON/SSE4.2 | AVX2/NEON |
| **内存** | 4GB | 16GB |
| **存储** | 50GB SSD | 256GB NVMe SSD |
| **网络** | 1Mbps 上行 | 10Mbps 上行 |

### 推荐平台

#### 边缘计算设备

| 设备 | CPU | 内存 | 适用场景 |
|-----|-----|------|---------|
| NVIDIA Jetson Orin NX | ARM64 8核 @ 2.0GHz | 16GB | 人形机器人 (推荐) |
| NVIDIA Jetson AGX Orin | ARM64 12核 @ 2.2GHz | 64GB | 高性能应用 |
| NVIDIA Jetson Nano | ARM64 4核 @ 1.4GHz | 4GB | 轮式机器人/开发 |
| Raspberry Pi 5 | ARM64 4核 @ 2.4GHz | 8GB | 开发测试 |
| Intel NUC 12 Pro | x86_64 10核 @ 3.3GHz | 32GB | 研发/仿真 |

### 传感器要求

#### 必需传感器

| 传感器 | 精度 | 频率 | 接口 |
|-------|------|------|------|
| IMU (9轴) | ±0.05° | ≥ 50Hz | SPI/I2C |
| 关节编码器 | 12-bit | ≥ 50Hz | SPI/CAN |
| 足端力传感器 (人形) | ±10% | ≥ 50Hz | ADC |

#### 可选传感器

| 传感器 | 用途 | 接口 |
|--------|------|------|
| 深度相机 | 视觉数据采集 | USB3/Ethernet |
| 激光雷达 | 环境感知 | Ethernet |
| GPS | 绝对定位 | UART/USB |

---

## 参考平台

### 智元机器人 灵犀 X2 (AgiBot X2 Ultra)

```
制造商: 智元机器人 (Zhiyuan Robotics)
身高: 1.31m
体重: 39kg
自由度: 30 DOF
  - 颈部: 1 DOF
  - 手臂: 14 DOF (每臂 7)
  - 腰部: 3 DOF
  - 腿部: 12 DOF (每腿 6)
续航: ~2小时 (@ 0.5 m/s)
最大速度: 1.8 m/s (日常 ≤ 0.8 m/s)
负载: 3kg (特定姿态), ≤1kg (全空间)
关节扭矩: 120 N·m (峰值)
算力: Orin NX 16GB 157 TOPS
传感器: RGB×4 + RGB-D + 3D激光雷达 + 触摸
通信: WiFi + 蓝牙 + 4G/5G
特点: 高度集成、全身协调控制、AimDK开发支持
```

### Google RT-2 (Robotics Transformer 2)

```
开发者: Google DeepMind
特点: VLA (视觉-语言-动作) 模型
应用: 通用机器人操控
传感器: 深度相机 + 触觉
```

### Tesla Optimus

```
身高: 1.73m
体重: 56kg
自由度: 28+ DOF
特点: FSD 技术下放、电机驱动
应用: 通用人形机器人
```

---

## 电气规范

| 参数 | 规格 |
|-----|------|
| 输入电压 | 12V - 48V DC |
| 功耗 | 30W - 1kW (取决于配置) |
| 工作温度 | 0°C ~ +45°C |
| 存储温度 | -20°C ~ +60°C |
| 防护等级 | IP20 (室内) / IP54 (可选) |

---

## 接口规范

### ROS2 话题接口

| 话题 | 类型 | 频率 | 说明 |
|-----|------|------|------|
| `/robot/odom` | nav_msgs/Odometry | 50Hz | 里程计数据 |
| `/robot/joint_states` | sensor_msgs/JointState | 50Hz | 关节状态 |
| `/robot/imu` | sensor_msgs/Imu | 50Hz | IMU数据 |
| `/robot/cmd_vel` | geometry_msgs/Twist | 50Hz | 速度命令 |

### 电气接口

| 接口 | 数量 | 用途 |
|------|------|------|
| CAN-FD | 2 | 电机控制、编码器反馈 |
| SPI | 1-2 | IMU、编码器 |
| I2C | 1 | 扩展传感器 |
| UART | 1 | GPS、调试 |
| USB 3.0 | 2-4 | 相机、WiFi |

---

## 快速检查清单

### 硬件检查

- [ ] 计算平台满足最低配置
- [ ] IMU 已安装并测试
- [ ] 编码器已安装并测试
- [ ] 电源供应稳定
- [ ] 散热良好

### 软件检查

- [ ] ROS2 Humble 已安装
- [ ] Aurora-Edge-Runtime 已编译
- [ ] 传感器驱动已安装
- [ ] 里程计发布正常
- [ ] IMU 数据正常

---

## 集成支持

### 认证硬件

以下硬件已通过 Aurora-Edge-Runtime 认证：

- ✅ NVIDIA Jetson Orin/Nano
- ✅ Raspberry Pi 4/5
- ✅ Intel NUC 11/12
- ✅ ICM-42688-P IMU
- ✅ MPU9250/MPU6050 IMU
- ✅ AS5600 编码器
- ✅ T-Motor 系列电机

### 认证流程

如需将您的硬件添加到认证列表：

1. 提交硬件规格表
2. 提供测试样品（可选）
3. 通过兼容性测试
4. 颁发认证证书

### 技术支持

- 📧 邮箱: hardware@example.com
- 📞 电话: +86-xxx-xxxx-xxxx
- 💬 工单: https://support.example.com
- 📖 文档: https://docs.aurora-runtime.com

---

## 更新日志

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| v1.1.3 | 2026-03-07 | 添加智元灵犀 X2 (AgiBot X2 Ultra) 参考规格 |
| v1.1.2 | 2026-03-07 | 添加 Google RT-2 参考规格 |
| v1.1.0 | 2026-03-04 | 添加人形机器人支持 |
| v1.0.0 | 2026-02-25 | 初始版本
