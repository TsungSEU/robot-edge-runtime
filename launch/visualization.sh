#!/bin/bash
# 机器人数据采集可视化启动脚本
# Robot Data Collection Visualization Launch Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_DIR="$SCRIPT_DIR/config"

# 检查 rviz2 配置文件是否存在
if [ ! -f "$CONFIG_DIR/robot_demo_vis.rviz" ]; then
    echo "错误: RViz 配置文件不存在: $CONFIG_DIR/robot_demo_vis.rviz"
    echo "请先编译项目生成配置文件"
    exit 1
fi

# 检查 ROS 环境
if [ -z "$ROS_DISTRO" ]; then
    echo "错误: ROS 环境未设置"
    echo "请先运行: source /opt/ros/humble/setup.bash"
    exit 1
fi

# 设置 RMW 环境变量
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

echo "======================================"
echo "  机器人数据采集可视化"
echo "  Robot Data Collection Visualization"
echo "======================================"
echo ""
echo "RViz 配置: $CONFIG_DIR/robot_demo_vis.rviz"
echo "URDF 文件: $CONFIG_DIR/bipedal_robot.urdf"
echo "RMW: $RMW_IMPLEMENTATION"
echo ""
echo "显示内容:"
echo "  - Grid (网格)"
echo "  - TF (坐标变换树)"
echo "  - RobotModel (机器人3D模型)"
echo "  - Odometry (运动轨迹 - 橙色箭头)"
echo "  - JointStates (关节状态 - 紫色箭头)"
echo "  - Origin (原点)"
echo ""
echo "使用方法:"
echo "  终端 1: ./build/src/dcp"
echo "  终端 2: ./launch/visualization.sh"
echo ""
echo "要停止: 按 Ctrl+C"
echo ""
echo "======================================"

# 检查 URDF 文件是否存在
URDF_FILE="$CONFIG_DIR/bipedal_robot.urdf"
if [ ! -f "$URDF_FILE" ]; then
    echo "错误: URDF 文件不存在: $URDF_FILE"
    exit 1
fi

echo "启动 robot_state_publisher..."
ros2 run robot_state_publisher robot_state_publisher --ros-args \
    -p robot_description:="$(cat "$URDF_FILE")" \
    -p use_tf_static:=true \
    --remap /joint_states:=/robot/joint_states &
RSP_PID=$!
echo "      PID: $RSP_PID"

# 等待 robot_state_publisher 初始化
sleep 2

# 启动 rviz2
rviz2 -d "$CONFIG_DIR/robot_demo_vis.rviz"

# 清理：停止 robot_state_publisher
kill $RSP_PID 2>/dev/null
