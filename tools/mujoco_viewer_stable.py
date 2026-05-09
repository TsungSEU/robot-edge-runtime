#!/usr/bin/env python3
"""
Stable MuJoCo Viewer for Aurora Bipedal Robot
Uses direct position control (kinematics) instead of physics simulation
This provides stable visualization without physics instability
"""

import os
import sys
import time
import numpy as np
import threading

# Check if we're using Anaconda Python
if 'anaconda' in sys.executable.lower() or 'conda' in sys.executable.lower():
    print("WARNING: You are using Anaconda Python:", sys.executable)
    print("This may cause GLIBCXX version errors with ROS2 libraries.")
    print("")
    print("SOLUTION: Use system Python instead:")
    print("  1. Deactivate conda: conda deactivate")
    print("  2. Source ROS2: source /opt/ros/humble/setup.bash")
    print("  3. Run: python3 tools/mujoco_viewer_stable.py")
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
    sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    if 'GLIBCXX' in str(e) or 'libstdc++' in str(e):
        print("This is a libstdc++ version compatibility issue.")
        print("Use system Python instead.")
    sys.exit(1)


class StableMuJoCoViewerNode(Node):
    """ROS2 node for stable MuJoCo visualization using direct position control"""

    AURORA_JOINT_NAMES = [
        "left_hip_yaw", "left_hip_roll", "left_hip_pitch",
        "left_knee_pitch", "left_ankle_pitch", "left_ankle_roll",
        "right_hip_yaw", "right_hip_roll", "right_hip_pitch",
        "right_knee_pitch", "right_ankle_pitch", "right_ankle_roll"
    ]

    def __init__(self, model_path):
        super().__init__('stable_mujoco_viewer')

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
        self.actuator_to_joint = {}
        for i in range(self.model.nu):
            name = mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_ACTUATOR, i)
            if name:
                self.actuator_names.append(name)
                joint_id = self.model.actuator_trnid[i, 0]
                self.actuator_to_joint[i] = joint_id

        # Build mapping from Aurora joint names to MuJoCo joint indices
        self.joint_name_to_idx = {}
        for i, mj_name in enumerate(self.mujoco_joint_names):
            # Remove _joint suffix
            aurora_name = mj_name.replace("_joint", "")

            # Add the base name
            self.joint_name_to_idx[aurora_name] = i

            # Special case: MuJoCo uses "left_knee" but DCP uses "left_knee_pitch"
            # Add _pitch variant for knee and ankle joints
            if 'knee' in aurora_name and '_pitch' not in aurora_name:
                self.joint_name_to_idx[aurora_name + '_pitch'] = i
            elif 'ankle' in aurora_name and 'pitch' in aurora_name:
                # Add mapping for just "ankle_pitch" without left/right prefix for compatibility
                pass  # Already handled by base name

        # Build mapping from Aurora joint names to actuator indices
        self.joint_name_to_actuator = {}
        for act_idx, act_name in enumerate(self.actuator_names):
            aurora_name = act_name.replace("_motor", "")
            self.joint_name_to_actuator[aurora_name] = act_idx

        # Joint positions cache
        self.target_positions = np.zeros(self.model.nq)
        self.actuator_targets = np.zeros(self.model.nu)

        # Default standing pose
        self.default_pose = np.zeros(self.model.nu)

        # Track message reception
        self.received_messages = False
        self.last_message_time = 0

        # Initialize base height and default standing pose
        # Diagnostic confirmed: when qpos=0, feet are at z=-0.907
        # So base_link should be at z=0.907 for feet to be on ground
        self.initial_base_z = 0.907  # Confirmed by diagnostic script

        print(f"MuJoCo model loaded: {model_path}")
        print(f"DoF: {self.model.nq}, Actuators: {self.model.nu}")

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

        # Lock for thread safety
        self.lock = threading.Lock()
        self.running = False

        # Set correct initial base height and pose
        print("\nInitializing robot pose...")

        # Reset to neutral standing pose
        # Important: Set joint positions first, then adjust base height
        self.data.qpos[:] = 0.0
        self.data.qvel[:] = 0.0

        # Compute kinematics to get foot positions with neutral joints
        mujoco.mj_kinematics(self.model, self.data)

        # Find where the feet are with qpos=0
        min_foot_z = 999.0
        foot_geom_names = ["left_foot", "right_foot"]
        for geom_name in foot_geom_names:
            geom_id = mujoco.mj_name2id(self.model, mujoco.mjtObj.mjOBJ_GEOM, geom_name)
            if geom_id >= 0:
                pos = self.data.geom_xpos[geom_id]
                size = self.model.geom_size[geom_id]
                bottom_z = pos[2] - size[2]
                min_foot_z = min(min_foot_z, bottom_z)

        print(f"  Initial foot bottom z (with qpos=0, base_z=0): {min_foot_z:.6f}m")

        # The feet should be at z=0 when standing
        # If min_foot_z is negative (underground), we need to raise base by -min_foot_z
        # The base_link qpos[2] is the z-position of the floating base
        self.data.qpos[2] = abs(min_foot_z)  # Set base so feet are at ground level
        self.initial_base_z = self.data.qpos[2]

        # Recompute kinematics with correct base height
        mujoco.mj_kinematics(self.model, self.data)

        # Verify feet are now on ground
        min_foot_z_after = 999.0
        for geom_name in foot_geom_names:
            geom_id = mujoco.mj_name2id(self.model, mujoco.mjtObj.mjOBJ_GEOM, geom_name)
            if geom_id >= 0:
                pos = self.data.geom_xpos[geom_id]
                size = self.model.geom_size[geom_id]
                bottom_z = pos[2] - size[2]
                min_foot_z_after = min(min_foot_z_after, bottom_z)

        print(f"  ✓ Base z-position set to: {self.data.qpos[2]:.6f}m")
        print(f"  ✓ Foot bottom z after adjustment: {min_foot_z_after:.6f}m")
        print(f"  Using DIRECT POSITION CONTROL (stable mode)")
        print(f"  Joint mapping: {len(self.joint_name_to_idx)} joints")
        print(f"  Actuator mapping: {len(self.joint_name_to_actuator)} actuators")
        print()

    def joint_callback(self, msg):
        """Handle joint state messages"""
        with self.lock:
            self.received_messages = True
            self.last_message_time = time.time()

            # DCP to MuJoCo actuator mapping
            # DCP order: yaw, roll, pitch, knee, ankle_pitch, ankle_roll (per leg)
            # MuJoCo order: pitch, roll, yaw, knee, ankle_pitch, ankle_roll (per leg)
            DCP_TO_MUJOCO = [
                2,   # DCP[0]  left_hip_yaw         -> Act[2]  hip_yaw_left
                1,   # DCP[1]  left_hip_roll        -> Act[1]  hip_roll_left
                0,   # DCP[2]  left_hip_pitch       -> Act[0]  hip_pitch_left
                3,   # DCP[3]  left_knee_pitch      -> Act[3]  knee_left
                4,   # DCP[4]  left_ankle_pitch     -> Act[4]  ankle_pitch_left
                5,   # DCP[5]  left_ankle_roll      -> Act[5]  ankle_roll_left
                10,  # DCP[6]  right_hip_yaw        -> Act[10] hip_yaw_right
                9,   # DCP[7]  right_hip_roll       -> Act[9]  hip_roll_right
                8,   # DCP[8]  right_hip_pitch      -> Act[8]  hip_pitch_right
                11,  # DCP[9]  right_knee_pitch     -> Act[11] knee_right
                12,  # DCP[10] right_ankle_pitch    -> Act[12] ankle_pitch_right
                13,  # DCP[11] right_ankle_roll     -> Act[13] ankle_roll_right
            ]

            # Map DCP joint positions to MuJoCo actuators
            for i, pos in enumerate(msg.position):
                if i < len(DCP_TO_MUJOCO):
                    mujoco_act_idx = DCP_TO_MUJOCO[i]
                    if mujoco_act_idx < self.model.nu:
                        self.actuator_targets[mujoco_act_idx] = pos

            # Also map to joint positions for direct control
            for name, pos in zip(msg.name, msg.position):
                if name in self.joint_name_to_idx:
                    joint_idx = self.joint_name_to_idx[name]
                    if joint_idx < self.model.nq:
                        self.target_positions[joint_idx] = pos

    def odom_callback(self, msg):
        """Handle odometry messages"""
        with self.lock:
            q = msg.pose.pose.orientation
            p = msg.pose.pose.position

            # Update base position directly from odometry
            # Use a minimum height threshold to prevent sinking
            min_height = self.initial_base_z * 0.9  # Allow some natural height variation
            self.data.qpos[0] = p.x
            self.data.qpos[1] = p.y
            self.data.qpos[2] = max(p.z, min_height)
            self.data.qpos[3:7] = [q.w, q.x, q.y, q.z]

    def start_viewer(self):
        """Launch MuJoCo viewer"""
        self.viewer = mujoco.viewer.launch_passive(
            self.model, self.data,
            show_left_ui=True,
            show_right_ui=True
        )

        # Set camera for good viewing angle
        self.viewer.cam.distance = 2.5
        self.viewer.cam.elevation = -10
        self.viewer.cam.azimuth = 90
        self.viewer.cam.lookat[:] = [0, 0, 0.8]

        self.running = True
        print("Viewer started with UI panels enabled")

    def update(self):
        """Update visualization using direct position control (STABLE)"""
        if self.viewer is None:
            return False

        with self.lock:
            # Check if we have recent messages
            time_since_message = time.time() - self.last_message_time
            use_default = not self.received_messages or time_since_message > 1.0

            if use_default:
                # Use default standing pose
                targets = self.default_pose
            else:
                # Use received targets
                targets = self.actuator_targets

            # DIRECT POSITION CONTROL - No physics simulation!
            # This is much more stable than PD control
            for i in range(self.model.nu):
                joint_idx = self.actuator_to_joint.get(i, -1)
                if joint_idx < 0:
                    continue

                # Skip floating base
                if joint_idx == 0:
                    continue

                # Skip achilles and passive joints (they're controlled by equality constraints)
                joint_name = self.mujoco_joint_names[joint_idx] if joint_idx < len(self.mujoco_joint_names) else ""
                if 'achilles' in joint_name.lower() or 'rod' in joint_name.lower() or 'waist' in joint_name.lower():
                    continue

                # Directly set joint position (kinematics only)
                if i < len(targets):
                    # Smooth interpolation for visual stability
                    target = targets[i]
                    current = self.data.qpos[joint_idx]

                    # Use higher alpha for more responsive tracking
                    alpha = 0.6  # Increased for better response
                    self.data.qpos[joint_idx] = current + alpha * (target - current)

                    # Zero velocities for stability
                    self.data.qvel[joint_idx] = 0.0

            # Compute forward kinematics to update body positions
            mujoco.mj_kinematics(self.model, self.data)

            # Sync viewer
            self.viewer.sync()

        return True

    def stop(self):
        """Stop the viewer"""
        self.running = False
        if self.viewer:
            self.viewer.close()


class StableAuroraMuJoCoBridge:
    """Stable bridge using direct position control instead of physics"""

    def __init__(self, model_path=None):
        if model_path is None:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            model_path = os.path.join(script_dir, '../config/bipedal_robot_mujoco.xml')

        if not os.path.exists(model_path):
            raise FileNotFoundError(f"Model file not found: {model_path}")

        # Initialize ROS2
        rclpy.init()
        self.node = StableMuJoCoViewerNode(model_path)

    def spin(self):
        """Main loop"""
        try:
            self.node.start_viewer()

            fps = 60
            dt = 1.0 / fps

            print("\n" + "=" * 60)
            print("Stable MuJoCo Viewer (Direct Position Control)")
            print("=" * 60)
            print("\nFeatures:")
            print("  ✓ No physics simulation - perfectly stable")
            print("  ✓ Direct joint position control")
            print("  ✓ Smooth interpolation for visual quality")
            print("  ✓ UI panels enabled for inspection")
            print("\nControls:")
            print("  - Mouse: Rotate/Pan/Zoom camera")
            print("  - Tab: Cycle viewing modes")
            print("  - Esc: Exit")
            print("\n" + "=" * 60)
            print("\nWaiting for /joint_states messages...\n")

            frame_count = 0
            start_time = time.time()
            last_status_time = start_time

            while rclpy.ok() and self.node.running:
                loop_start = time.time()

                # Process ROS2 callbacks
                rclpy.spin_once(self.node, timeout_sec=0.0001)

                # Update visualization
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
                        status = f"Receiving ROS2 (last: {time_since_msg:.1f}s ago)"

                        # Debug info every 5 seconds
                        if frame_count % 300 == 0:
                            with self.node.lock:
                                print(f"  Joint targets: L_hip={self.node.actuator_targets[0]:.2f}, "
                                      f"L_knee={self.node.actuator_targets[3]:.2f}, "
                                      f"R_hip={self.node.actuator_targets[6]:.2f}, "
                                      f"R_knee={self.node.actuator_targets[9]:.2f}")
                    else:
                        status = "Using default pose (no DCP simulator)"

                    print(f"[{elapsed:5.1f}s, {fps_actual:5.1f} FPS] {status}")
                    last_status_time = current_time

                # Maintain frame rate
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

    parser = argparse.ArgumentParser(description='Stable Aurora Bipedal Robot MuJoCo Viewer')
    parser.add_argument('--model', '-m', default=None,
                      help='Path to MuJoCo XML model file')

    args = parser.parse_args()

    try:
        print("\n" + "=" * 60)
        print("Stable MuJoCo Viewer for Aurora Bipedal Robot")
        print("=" * 60)
        print("\nThis viewer uses DIRECT POSITION CONTROL")
        print("instead of physics simulation for maximum stability.")
        print("\nAdvantages:")
        print("  • No physics instability")
        print("  • Perfect tracking of joint positions")
        print("  • No robot falling or leg folding")
        print("  • Smooth visualization")
        print("\n" + "=" * 60)

        bridge = StableAuroraMuJoCoBridge(args.model)
        bridge.spin()

    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
