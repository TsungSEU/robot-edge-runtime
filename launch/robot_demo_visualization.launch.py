#!/usr/bin/env python3
"""
机器人数据采集演示可视化启动脚本
Robot Data Collection Demo Visualization Launch Script

用于启动 rviz2 可视化界面，显示机器人运动轨迹
"""

from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    # 获取 rviz 配置文件路径
    config_dir = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'config'
    )
    rviz_config = os.path.join(config_dir, 'robot_demo.rviz')

    return LaunchDescription([
        # RViz2 可视化节点
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='screen',
            arguments=['-d', rviz_config],
            parameters=[{
                'use_sim_time': False
            }],
            remappings=[
                # 如果需要，可以重映射话题
                ('/odom', '/robot/odom'),
                ('/joint_states', '/robot/joint_states'),
            ]
        ),
    ])
