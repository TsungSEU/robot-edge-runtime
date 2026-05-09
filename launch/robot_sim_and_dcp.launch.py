#!/usr/bin/env python3
"""
Launch file for running Robot Simulator and DCP as separate processes

This launch file starts:
1. robot_sim - The robot simulator node
2. dcp - The data collection planner node

The DCP node is delayed by 2 seconds to ensure the robot simulator
services are available when DCP starts.
"""

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import TimerAction, ExecuteProcess, RegisterEventHandler
from launch.event_handlers import OnProcessExit

def generate_launch_description():
    # Robot simulator node
    robot_sim_node = Node(
        package='aurora_edge',
        executable='robot_sim',
        name='robot_simulator',
        output='screen',
        parameters=[],
        remappings=[
            ('/odom', '/robot/odom'),
            ('/joint_states', '/robot/joint_states'),
        ]
    )

    # DCP node (delayed by 2 seconds to ensure services are available)
    dcp_node = Node(
        package='aurora_edge',
        executable='dcp',
        name='data_collection_planner',
        output='screen',
        parameters=[],
    )

    # Delay DCP launch by 2 seconds
    delayed_dcp_node = TimerAction(
        period=2.0,
        actions=[dcp_node]
    )

    return LaunchDescription([
        robot_sim_node,
        delayed_dcp_node,
    ])
