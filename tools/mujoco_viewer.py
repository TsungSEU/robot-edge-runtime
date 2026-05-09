#!/usr/bin/env python3
# MuJoCo Viewer for Aurora Bipedal Robot
# Visualizes the robot in MuJoCo by subscribing to ROS2 joint_states
#
# IMPORTANT: This script must be run with system Python, NOT Anaconda Python!
# Usage:
#   source /opt/ros/humble/setup.bash
#   python3 tools/mujoco_viewer.py

import os
import sys
import time
import numpy as np
import threading

# Check if we're using Anaconda Python (which has libstdc++ compatibility issues)
if 'anaconda' in sys.executable.lower() or 'conda' in sys.executable.lower():
    print("WARNING: You are using Anaconda Python:", sys.executable)
    print("This may cause GLIBCXX version errors with ROS2 libraries.")
    print("")
    print("SOLUTION: Use system Python instead:")
    print("  1. Deactivate conda: conda deactivate")
    print("  2. Source ROS2: source /opt/ros/humble/setup.bash")
    print("  3. Run: python3 tools/mujoco_viewer.py")
    print("")
    response = input("Continue anyway? (y/N): ")
    if response.lower() != 'y':
        sys.exit(1)

try:
    import mujoco
    import mujoco.viewer
except ImportError:
    print("Installing MuJoCo...")
    os.system("pip install mujoco")
    import mujoco
    import mujoco.viewer

try:
    import rclpy
    from rclpy.node import Node
    from sensor_msgs.msg import JointState
    from nav_msgs.msg import Odometry
except ImportError as e:
    print(f"ERROR importing ROS2: {e}")
    print("")
    print("Make sure ROS2 Humble is installed and sourced:")
    print("  source /opt/ros/humble/setup.bash")
    print("")
    print("If you see GLIBCXX version errors, you are using Anaconda Python.")
    print("Use system Python instead (see instructions above).")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    print("")
    if 'GLIBCXX' in str(e) or 'libstdc++' in str(e):
        print("This is a libstdc++ version compatibility issue.")
        print("You are likely using Anaconda Python with ROS2 libraries.")
        print("")
        print("SOLUTION: Use system Python:")
        print("  1. Deactivate conda: conda deactivate")
        print("  2. Source ROS2: source /opt/ros/humble/setup.bash")
        print("  3. Run: python3 tools/mujoco_viewer.py")
    sys.exit(1)


class MuJoCoViewerNode(Node):
    """ROS2 node that subscribes to joint_states and updates MuJoCo simulation"""

    # Joint names matching Aurora's convention (without _joint suffix)
    # Order: [left_leg..., right_leg...]
    AURORA_JOINT_NAMES = [
        "left_hip_yaw", "left_hip_roll", "left_hip_pitch",
        "left_knee_pitch", "left_ankle_pitch", "left_ankle_roll",
        "right_hip_yaw", "right_hip_roll", "right_hip_pitch",
        "right_knee_pitch", "right_ankle_pitch", "right_ankle_roll"
    ]

    # MuJoCo XML uses _joint suffix
    # We need to map Aurora names to MuJoCo joint indices
    def __init__(self, model_path):
        super().__init__('mujoco_viewer')

        # Load MuJoCo model
        self.model = mujoco.MjModel.from_xml_path(model_path)
        self.data = mujoco.MjData(self.model)
        self.viewer = None

        # Get MuJoCo joint names
        self.mujoco_joint_names = []
        for i in range(self.model.njnt):
            name = mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_JOINT, i)
            if name:
                self.mujoco_joint_names.append(name)

        # Get MuJoCo actuator names
        self.actuator_names = []
        self.actuator_to_joint = {}  # Maps actuator index to joint index
        for i in range(self.model.nu):
            name = mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_ACTUATOR, i)
            if name:
                self.actuator_names.append(name)
                # Get the joint this actuator controls
                joint_id = self.model.actuator_trnid[i, 0]  # trnid contains transmission IDs
                self.actuator_to_joint[i] = joint_id

        # Build mapping from Aurora joint names to MuJoCo joint indices
        self.joint_name_to_idx = {}
        for i, mj_name in enumerate(self.mujoco_joint_names):
            # MuJoCo uses "left_hip_yaw_joint", Aurora uses "left_hip_yaw"
            aurora_name = mj_name.replace("_joint", "")
            self.joint_name_to_idx[aurora_name] = i

        # Build mapping from Aurora joint names to actuator indices
        # (Actuators are ordered in Aurora's convention: yaw, roll, pitch, knee, ankle_pitch, ankle_roll)
        self.joint_name_to_actuator = {}
        for act_idx, act_name in enumerate(self.actuator_names):
            # Actuator name is like "left_hip_yaw_motor"
            # Remove "_motor" suffix to get Aurora joint name
            aurora_name = act_name.replace("_motor", "")
            self.joint_name_to_actuator[aurora_name] = act_idx

        # Joint positions cache (indexed by MuJoCo joint index)
        self.target_positions = np.zeros(self.model.nq)

        # Actuator targets (indexed by actuator index, matches Aurora's order)
        self.actuator_targets = np.zeros(self.model.nu)

        # Default standing pose (if no ROS2 messages received)
        # All zeros = neutral standing pose for this robot
        self.default_pose = np.zeros(self.model.nu)

        # Track if we've received ROS2 messages
        self.received_messages = False
        self.last_message_time = 0

        # Create subscribers
        self.joint_sub = self.create_subscription(
            JointState,
            '/joint_states',
            self.joint_callback,
            10
        )
        self.odom_sub = self.create_subscription(
            Odometry,
            '/robot/odom',
            self.odom_callback,
            10
        )

        # Lock for thread-safe data access
        self.lock = threading.Lock()

        self.running = False

        print(f"MuJoCo model loaded: {model_path}")
        print(f"MuJoCo Joints: {self.mujoco_joint_names}")
        print(f"Actuators: {self.actuator_names}")
        print(f"Aurora->MuJoCo joint mapping: {self.joint_name_to_idx}")
        print(f"Aurora->Actuator mapping: {self.joint_name_to_actuator}")
        print(f"DoF: {self.model.nq}, Actuators: {self.model.nu}")

    def joint_callback(self, msg):
        """Handle joint state messages"""
        with self.lock:
            # Mark that we've received messages
            self.received_messages = True
            self.last_message_time = time.time()

            # Map Aurora joint names to MuJoCo joint indices and actuator indices
            for name, pos in zip(msg.name, msg.position):
                # Update joint position target (indexed by MuJoCo joint index)
                if name in self.joint_name_to_idx:
                    joint_idx = self.joint_name_to_idx[name]
                    if joint_idx < self.model.nq:
                        self.target_positions[joint_idx] = pos

                # Update actuator target (indexed by actuator index, matches Aurora's order)
                if name in self.joint_name_to_actuator:
                    act_idx = self.joint_name_to_actuator[name]
                    if act_idx < self.model.nu:
                        self.actuator_targets[act_idx] = pos

            # Note: Queue operations removed - not needed for visualization
            # The joint positions are stored in self.target_positions and self.actuator_targets

    def odom_callback(self, msg):
        """Handle odometry messages"""
        with self.lock:
            # Update base position from odometry
            # This assumes the robot is in a known starting pose
            q = msg.pose.pose.orientation
            p = msg.pose.pose.position

            # Convert quaternion to euler angles (simplified)
            # For proper implementation, use tf2 or a quaternion library
            yaw = np.arctan2(2.0 * (q.w * q.z + q.x * q.y),
                             1.0 - 2.0 * (q.y * q.y + q.z * q.z))

            # Update base position in MuJoCo data
            # Index 0-6: position (x,y,z), quaternion (w,x,y,z)
            self.data.qpos[0:3] = [p.x, p.y, p.z]
            self.data.qpos[3:7] = [q.w, q.x, q.y, q.z]

            # Note: Queue operation removed - not needed for visualization
            # The odometry data is stored directly in self.data.qpos

    def get_joint_positions(self):
        """Get current joint positions"""
        with self.lock:
            return self.joint_positions.copy()

    def start_viewer(self):
        """Launch MuJoCo viewer"""
        self.viewer = mujoco.viewer.launch_passive(
            self.model, self.data,
            show_left_ui=True,   # Show left UI panel
            show_right_ui=True   # Show right UI panel
        )
        # Set camera for better viewing angle
        self.viewer.cam.distance = 2.5
        self.viewer.cam.elevation = -10
        self.viewer.cam.azimuth = 90
        self.viewer.cam.lookat[:] = [0, 0, 0.8]  # Look at robot waist

        self.running = True

    def update(self):
        """Update MuJoCo simulation and viewer"""
        if self.viewer is None:
            return False

        with self.lock:
            # Use default pose if no messages received recently
            time_since_message = time.time() - self.last_message_time
            use_default = not self.received_messages or time_since_message > 1.0

            if use_default:
                # Use default standing pose
                targets = self.default_pose
            else:
                # Use received actuator targets
                targets = self.actuator_targets

            # Apply PD control to move joints to target positions
            # Optimized gains for stability
            kp = 100.0   # Position gain (increased for better tracking)
            kd = 10.0    # Velocity gain (increased for damping)

            # Control each actuator
            for i in range(self.model.nu):
                # Get the joint index this actuator controls
                joint_idx = self.actuator_to_joint.get(i, -1)

                if joint_idx < 0:
                    continue

                # Only control joints that we should control
                # Skip achilles tendon joints (they're passive)
                joint_name = self.mujoco_joint_names[joint_idx] if joint_idx < len(self.mujoco_joint_names) else ""

                # Skip passive achilles joints - they're controlled by equality constraints
                if 'achilles' in joint_name.lower():
                    continue

                # Skip floating base (index 0)
                if joint_idx == 0:
                    continue

                target = targets[i] if i < len(targets) else 0.0
                current = self.data.qpos[joint_idx]
                velocity = self.data.qvel[joint_idx]

                error = target - current
                velocity_error = -velocity

                # PD control with better error handling
                control_output = kp * error + kd * velocity_error

                # Clamp control input to actuator limits
                # Most actuators have range [-140, 140], some have [-42, 42]
                actuator_range = 140.0 if 'ankle' not in self.actuator_names[i].lower() else 42.0
                self.data.ctrl[i] = np.clip(control_output, -actuator_range, actuator_range)

        # Step physics with multiple substeps for stability
        # This improves simulation stability
        nsubsteps = 5
        for _ in range(nsubsteps):
            mujoco.mj_step(self.model, self.data, nstep=1)

        # Sync viewer
        self.viewer.sync()

        return True

    def stop(self):
        """Stop the viewer"""
        self.running = False
        if self.viewer:
            self.viewer.close()


class AuroraMuJoCoBridge:
    """Bridge between Aurora ROS2 system and MuJoCo visualization"""

    def __init__(self, model_path=None):
        if model_path is None:
            # Default path relative to this script
            script_dir = os.path.dirname(os.path.abspath(__file__))
            model_path = os.path.join(script_dir, '../config/bipedal_robot_full.xml')

        if not os.path.exists(model_path):
            raise FileNotFoundError(f"Model file not found: {model_path}")

        # Initialize ROS2
        rclpy.init()
        self.node = MuJoCoViewerNode(model_path)

    def spin(self):
        """Main loop"""
        try:
            self.node.start_viewer()

            fps = 60  # Target 60 FPS for smooth visualization
            dt = 1.0 / fps

            print("MuJoCo viewer running...")
            print("Waiting for /joint_states messages...")
            print("")
            print("Controls:")
            print("  - Mouse: Rotate/Pan/Zoom camera")
            print("  - Tab: Cycle through viewing modes")
            print("  - Space: Pause simulation")
            print("  - Esc: Exit")
            print("")
            print("UI Panels (Left/Right) show:")
            print("  - Simulation parameters")
            print("  - Joint positions and velocities")
            print("  - Actuator controls")
            print("")

            frame_count = 0
            start_time = time.time()
            last_status_time = start_time

            while rclpy.ok() and self.node.running:
                loop_start = time.time()

                # Spin ROS2 to process callbacks
                rclpy.spin_once(self.node, timeout_sec=0.0001)

                # Update MuJoCo
                if not self.node.update():
                    break

                frame_count += 1
                current_time = time.time()

                # Print status every 2 seconds
                if current_time - last_status_time >= 2.0:
                    elapsed = current_time - start_time
                    fps_actual = frame_count / elapsed

                    if self.node.received_messages:
                        time_since_msg = current_time - self.node.last_message_time
                        status = f"Receiving ROS2 messages (last: {time_since_msg:.1f}s ago)"

                        # Print joint info for debugging
                        if frame_count % 250 == 0:  # Every ~5 seconds
                            # Print a few joint positions for verification
                            with self.node.lock:
                                print(f"  Sample joint positions: "
                                      f"L_hip={self.node.actuator_targets[0]:.2f}, "
                                      f"L_knee={self.node.actuator_targets[3]:.2f}, "
                                      f"R_hip={self.node.actuator_targets[6]:.2f}, "
                                      f"R_knee={self.node.actuator_targets[9]:.2f}")
                    else:
                        status = "Using default pose (no Aurora simulator)"

                    print(f"[{elapsed:5.1f}s, {fps_actual:5.1f} FPS] {status}")
                    last_status_time = current_time

                # Maintain target frame rate
                loop_time = time.time() - loop_start
                sleep_time = dt - loop_time
                if sleep_time > 0:
                    time.sleep(sleep_time)

        except KeyboardInterrupt:
            print("\nShutting down...")
        finally:
            self.node.stop()
            rclpy.shutdown()


def main():
    import argparse

    parser = argparse.ArgumentParser(description='Aurora Bipedal Robot MuJoCo Viewer')
    parser.add_argument('--model', '-m',
                      default=None,
                      help='Path to MuJoCo XML model file')
    parser.add_argument('--list-joints', action='store_true',
                      help='List available joints and exit')

    args = parser.parse_args()

    try:
        bridge = AuroraMuJoCoBridge(args.model)

        if args.list_joints:
            print("\nAvailable Joints:")
            for i, name in enumerate(bridge.node.mujoco_joint_names):
                print(f"  {i}: {name} (Aurora: {name.replace('_joint', '')})")
            print(f"\nTotal MuJoCo DoF: {bridge.node.model.nq}")
            print(f"Total Actuators: {bridge.node.model.nu}")
            return

        print("=" * 60)
        print("Aurora Bipedal Robot - MuJoCo Viewer")
        print("=" * 60)
        print("\nControls:")
        print("  - Mouse: Rotate/Pan/Zoom camera")
        print("  - Tab: Cycle through viewing modes")
        print("  - Space: Pause simulation")
        print("  - Esc: Exit")
        print("\nSubscribed Topics:")
        print("  - /joint_states: Joint positions")
        print("  - /robot/odom: Odometry")
        print("\n" + "=" * 60)

        bridge.spin()

    except FileNotFoundError as e:
        print(f"Error: {e}")
        print("\nPlease ensure the MuJoCo XML model file exists.")
        print("You can specify a custom model with --model <path>")
        print("Default: config/bipedal_robot_full.xml")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
