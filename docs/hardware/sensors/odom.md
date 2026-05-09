# 里程计配置指南

本文档描述 Aurora-Edge-Runtime 系统中里程计的配置和使用方法。

## 里程计概述

里程计 (Odometry) 提供：

- **位置估计** - 机器人在世界坐标系中的位置 (x, y, z)
- **姿态估计** - 机器人的姿态 (roll, pitch, yaw)
- **速度估计** - 线速度和角速度
- **协方差** - 用于后续滤波融合

## 配置类型

### 人形机器人里程计

基于关节编码器和 IMU 的融合估计：

```
输入:
  - 关节编码器 (x12) → 正运动学 → 足端位置
  - IMU → 姿态角速度
  - 足端接触检测 → 支撑脚判断

算法:
  - EKF (扩展卡尔曼滤波)
  - 或 UKF (无损卡尔曼滤波)
```

### 轮式机器人里程计

基于轮速编码器和 IMU 的融合估计：

```
输入:
  - 左轮编码器 → 左轮速度
  - 右轮编码器 → 右轮速度
  - IMU → 姿态角速度

算法:
  - 差速模型
  - 与 IMU 融合校正
```

## 人形机器人里程计配置

### 正运动学模型

```yaml
# config/humanoid_odom.yaml
humanoid_odometry:
  # 腿部几何参数
  leg_geometry:
    upper_leg_length: 0.35  # m (上腿长)
    lower_leg_length: 0.35  # m (下腿长)
    hip_width: 0.10         # m (髋宽)
    foot_height: 0.05       # m (足高)

  # 支撑脚检测
  foot_contact:
    force_threshold: 50     # N (力传感器阈值)
    use_imu: true           # 使用 IMU 辅助判断
    use_z_axis: true        # 使用 Z 轴加速度

  # EKF 参数
  ekf:
    frequency: 50.0         # Hz

    # 状态向量 [x, y, z, roll, pitch, yaw, vx, vy, vz, vr, vp, vyaw]
    state_vector: 12

    # 过程噪声协方差
    process_noise_covariance: [0.01, 0.01, 0.01, 0.01, 0.01, 0.01,
                             0.1, 0.1, 0.1, 0.1, 0.1, 0.1]

    # 测量噪声协方差
    measure_noise_covariance: [0.1, 0.1, 0.1]
```

### ROS2 节点配置

```python
# launch/humanoid_odom.launch.py
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # 里程计节点
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_node',
            parameters=[{
                'config_file': 'config/humanoid_odom.yaml',
                'freq': 50.0,
                'sensor_timeout': 0.1,
                'odom_frame': 'odom',
                'base_link_frame': 'robot_base_link',
                'world_frame': 'world'
            }]
        ),
    ])
```

## 轮式机器人里程计配置

### 差速模型

```yaml
# config/diff_drive_odom.yaml
diff_drive_odometry:
  # 轮参数
  wheel_separation: 0.50  # m (轮距)
  wheel_radius: 0.10      # m (轮半径)

  # 编码器参数
  encoder_resolution: 512  # PPR (每转脉冲数)
  gear_ratio: 1.0          # 减速比

  # 速度限制
  max_linear_speed: 1.0    # m/s
  max_angular_speed: 2.0   # rad/s

  # 协方差
  pose_covariance_diagonal: [0.001, 0.001, 0.0, 0.0, 0.0, 0.001]
  twist_covariance_diagonal: [0.001, 0.0, 0.0, 0.0, 0.0, 0.001]
```

### ROS2 控制器

```xml
<ros2_control name="DiffDriveController">
  <hardware>
    <plugin>diff_drive_controller/DiffDriveController</plugin>
    <param name="left_wheel_names">["left_wheel_joint"]</param>
    <param name="right_wheel_names">["right_wheel_joint"]</param>
    <param name="wheel_separation">0.50</param>
    <param name="wheel_radius">0.10</param>
  </hardware>
</ros2_control>
```

## 里程计融合

### robot_localization 配置

```yaml
# config/ekf.yaml
ekf_filter_node:
  ros__parameters:
    # 频率
    frequency: 50.0

    # 传感器超时
    sensor_timeout: 0.1

    # 帧ID
    odom_frame: odom
    base_link_frame: robot_base_link
    world_frame: world

    # 输入
    odom0: /wheel/odometry
    odom0_config: [true,  true,  false,
                   false, false, true,
                   true,  true,  false,
                   false, false, true,
                   false, false]
    odom0_queue_size: 10

    imu0: /robot/imu
    imu0_config: [false, false, false,
                   false, false, true,
                   false, false, false,
                   true,  true,  false,
                   false, false]
    imu0_queue_size: 10

    # 初始状态
    initial_state: [0.0, 0.0, 0.0,
                     0.0, 0.0, 0.0,
                     0.0, 0.0, 0.0,
                     0.0, 0.0, 0.0]
```

## 数据输出

### Message 格式

```cpp
// nav_msgs/Odometry
Header header
string child_frame_id
geometry_msgs/PoseWithCovariance pose
geometry_msgs/TwistWithCovariance twist
```

### 示例输出

```
header:
  stamp: 1710000000123456789
  frame_id: "odom"
child_frame_id: "robot_base_link"

pose:
  pose:
    position:
      x: 1.23
      y: 4.56
      z: 0.70
    orientation:
      x: 0.001
      y: -0.002
      z: 0.999
      w: 0.042

twist:
  twist:
    linear:
      x: 0.3   # 前进速度
      y: 0.0
      z: 0.0
    angular:
      x: 0.0
      y: 0.0
      z: 0.05  # 转向速度
```

## 性能指标

### 精度要求

| 参数 | 人形机器人 | 轮式机器人 |
|------|------------|------------|
| 位置精度 | ±5cm | ±2cm |
| 姿态精度 | ±2° | ±1° |
| 速度精度 | ±0.05m/s | ±0.02m/s |
| 角速度精度 | ±0.1°/s | ±0.05°/s |

### 性能指标

| 指标 | 最低 | 推荐 |
|------|------|------|
| 更新频率 | 50Hz | 100Hz |
| 延迟 | < 20ms | < 10ms |
| 漂移率 | < 5%/min | < 1%/min |

## 调试

### 可视化

```bash
# 启动 RViz2
ros2 run rviz2 rviz2

# 添加显示
# 1. Odometry 显示
# 2. TF 显示
# 3. RobotModel 显示
```

### 检查 TF 树

```bash
# 查看 TF 树
ros2 run tf2_tools view_frames

# 查看 TF 变换
ros2 run tf2_ros tf2_echo odom robot_base_link
```

## 故障排查

### 问题 1: 里程计跳变

**症状**: 位置突然跳变

**原因**:
- 编码器噪声
- 打滑检测失效
- 支撑脚判断错误

**解决方案**:
```yaml
# 启用打滑检测
slip_detection:
  enabled: true
  threshold: 0.1  # 速度差异阈值

# 启用异常值过滤
outlier_rejection:
  enabled: true
  threshold: 0.5  # 位置跳变阈值
```

### 问题 2: 快速漂移

**症状**: 位置快速偏离

**原因**:
- 轮径不准
- 编码器分辨率不足
- 打滑严重

**解决方案**:
```bash
# 重新校准轮径
ros2 param set /diff_drive_controller/wheel_radius 0.10

# 调整协方差
ros2 param set /ekf_node/initial_estimate_covariance 0.01
```

### 问题 3: 方向漂移

**症状**: Yaw 角持续漂移

**原因**:
- IMU 陀螺仪漂移
- 轮子直径不一致

**解决方案**:
```yaml
# 启用 IMU 校正
imu_correction:
  enabled: true
  continuous: true
  alpha: 0.98  # 校正系数
```

## 参考资源

- [ROS2 robot_localization](https://docs.ros.org/en/humble/packages/robot_localization/)
- [diff_drive_controller](https://control.ros.org/doc/diff_drive_controller/)
