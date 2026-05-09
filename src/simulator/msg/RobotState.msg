# RobotState.msg
# 机器人整体状态消息（参考Unitree H1设计）
# 用于发布机器人的位置、速度、姿态和足端信息

# 包含头信息
builtin_interfaces/msg/Time stamp
uint32 error_code           # 错误码，0表示无错误

# 机器人位置和速度（世界坐标系）
geometry_msgs/Point position
geometry_msgs/Quaternion orientation
geometry_msgs/Vector3 linear_velocity
geometry_msgs/Vector3 angular_velocity

# 机器人身体高度
float32 body_height

# 足端位置（相对于身体坐标系）
# 12个关节对应的足端位置（6个左腿 + 6个右腿，实际只用2个）
# 数组索引对应：[0-5]左腿，[6-11]右腿
# 每个点包含x, y, z三个值，展开为36个float
float32[36] foot_position_body

# 足端速度（相对于身体坐标系）
float32[36] foot_velocity_body

# 步态状态
uint8 gait_type            # 步态类型
# 0: idle（静止站立）
# 1: trot（小跑）
# 2: run（奔跑）
# 3: climb_stair（爬楼梯）
# 4: forward_down_stair（下楼梯）
# 9: adjust（调整姿态）

float32 foot_raise_height  # 抬脚高度

# 足端力
int16[4] foot_force         # 4个足端力传感器读数

# 支撑相状态
bool[2] foot_contact      # [左脚, 右脚] 是否接触地面
uint8 support_phase        # 0:左脚支撑, 1:右脚支撑, 2:双脚支撑
