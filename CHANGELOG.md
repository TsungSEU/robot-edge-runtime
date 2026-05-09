# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.10.3] - 2026-05-08

**去 robot_sim 硬依赖、推理奖励权重对齐训练配置**

### Features

* **application_runner:** INIT_SYNC 支持 velocity_cmd 模式 fallback，robot_sim 服务不可用时自动切换为 odom 定位 + velocity 控制
  - `action_type == "velocity_cmd"` 时 service 不可用仅打印 WARN 继续
  - `action_type == "path_tracking"` 时仍要求 robot_sim 必须可用
  - 任务区域设置 fallback 到 `position_tracker_->getCurrentPosition()` (from `/robot/odom`)

### Changes

* **reward:** 推理奖励权重同步 `rl_planning_train/config/nav_data_training.yaml`
  - w_approach 3.0→15.0, w_goal 100.0→30.0, w_data_value 5.0→3.0, w_value_guide 2.0→0.5
  - w_coverage 1.0→0.5, w_speed 0.5→1.0, w_time 0.02→0.05
* **humanoid_reward:** 移除 `HumanoidRewardConfig` 硬编码默认值，所有参数从 YAML 加载，key 缺失时打印 WARN
* **aer script:** 路径修正 (ops/dev → project root)、`aer logs` 默认显示 aer 日志无需指定 service

## [1.10.2] - 2026-05-08

**子模块提取、可执行文件重命名 dcp→aer、日志优化**

### Refactor

* **submodule:** 将 `src/rl_planner_infer/` 提取为独立 GitLab 仓库 `rl_planning_infer`，以 git submodule 方式引用
  - 推送到 `gitlab.t3caic.com:icr11/dataengine/data-infra/Aurora/rl_planning_infer.git`
  - 本地路径 `src/rl_planning_infer/` → `src/rl_planning_infer/`（匹配远程仓库名）
  - 更新所有 `#include` 路径、CMakeLists.txt glob、测试配置

### Changes

* **rename:** 可执行文件从 `dcp` 重命名为 `aer`，影响范围：
  - CMake `EDGE_APP`、ROS2 节点名、日志路径（`/tmp/aer.log`）、配置文件默认路径
  - ops 脚本（`aer-start.sh`、`install-aer-service.sh`、`ops/dev/aer`）
  - 性能测试脚本 `runtime_performance_test.sh`
* **docs:** README 快速开始新增 `--recursive` 克隆说明 + 子模块初始化步骤；项目结构展示子模块；全部文档 dcp→aer、rl_planner_infer→rl_planning_infer 引用更新

### Bug Fixes

* **robot_controller:** 速度跟踪 warning 增加 2 秒节流，消除 50ms 控制周期内的日志洪泛（原 ~7100 条/次运行 → ~50 条）
* **data_manager:** 空数据点 WARN 降级为 DEBUG，新增 `hasDataPoints()` 守卫防止循环边界空状态警告

## [1.10.1] - 2026-05-07

**Edge-LivelyBot ROS2 通信增强：闭环速度反馈、topic 修复、QoS 诊断**

### Features

* **robot_controller:** 新增 `/robot/cmd_vel` 闭环速度反馈订阅（`VelocityFeedback` 结构体 + mutex 保护），支持实际执行速度与命令速度误差检测，误差超过 0.2m/s 触发 warning 日志

### Bug Fixes

* **gait_trigger:** 修复 `/joint_states` topic 名称错误，改为 `/robot/joint_states`，直接匹配 LivelyBot 发布的 topic
* **qos_callbacks:** 修复 ROS2 Humble API 兼容性（`set_on_new_qos_event_callback` + `RCL_SUBSCRIPTION_REQUESTED_DEADLINE_MISSED`），原代码从未成功编译

### Changes

* **qos:** 在 `/robot/velocity_cmd`（liveliness）、`/robot/cmd_vel`、`/robot/odom`、`/robot/joint_states`（deadline）安装 QoS 诊断回调，关键 topic 断连时输出告警
* **docs:** integration-flow.md 整体架构章节重写，新增 5 类 ROS2 消息详解表 + 消息时序图

## [1.10.0] - 2026-04-30

**gzip 压缩支持、录制时长扩展与后录数据缺失修复**

### Features

* **file_compress:** 新增 `CompressionFormat` 枚举（`Lz4`/`Gzip`），`CompressFiles` 和 `CompressSingleFileToLz4` 支持可选 gzip 格式，使用 zlib `deflateInit2` 实现，LZ4 保持默认向后兼容
* **app_config:** `dataStorage` 新增 `compressionFormat` 配置项（`"lz4"` 或 `"gzip"`，默认 `"lz4"`），DCP 运行时自动选择压缩格式和文件后缀（`.tar.lz4` 或 `.tar.gz`）
* **recording:** 三条采集策略录制时长统一调整为前录 45s + 后录 15s = 60s

### Bug Fixes

* **ros2bag_recorder:** 修复 `TriggerRecord` 后向数据缺失问题——单线程 worker 串行处理导致排队任务的后录窗口过期。新增动态等待时间计算，根据 `trigger_timestamp` 与当前时间差值缩减后录等待，并清空前次残留的后向 buffer
* **app_config:** 上传 `filenameRegex` 增加 `tar\.gz` 匹配，确保 gzip 格式文件可正常上传

### Build

* **cmake:** `Find3rdparty.cmake` 新增 `find_package(ZLIB REQUIRED)`，链接 `ZLIB::ZLIB`

## [1.9.0] - 2026-04-30

**合规模块：地信偏转、图像脱敏与元数据清单**

新增数据合规模块（compliance），支持地信坐标偏转、图像全帧脱敏、录制元数据清单自动生成。通过策略配置 `enableMasking` 启用，作为 Observer 中间层插入数据采集管道，不修改已有录制流程。清单文件随 bag 数据一并打包压缩上传。

### Features

* **compliance_filter:** Observer 中间层，按 topic 类型分发地信偏转和图像脱敏处理，异常时透传原始数据不中断录制
* **geospatial_obfuscator:** 确定性坐标偏转，session 内固定偏移量，支持配置偏移半径
* **image_desensitizer:** 轻量级两遍分离式 Box Blur 全帧模糊，无 OpenCV 依赖，640×480 RGB 约 2-3ms/帧
* **metadata_manifest:** 录制元数据清单自动生成（JSON），包含设备信息、时间窗口、触发位置、合规状态、sensor 列表、SHA-256 校验
* **strategy_config:** 新增 `MaskingConfig` 结构体（`geospatialOffsetRadius`、`imageBlurKernelSize`、`imageDetectionMode`），向后兼容旧配置
* **audit_logger:** 新增 `COMPLIANCE_GEO_OBFUSCATION_APPLIED`、`COMPLIANCE_IMAGE_DESENSITIZED`、`COMPLIANCE_MANIFEST_GENERATED` 审计事件

### Changes

* **data_collection_executor:** `RecordTask` 新增 `trigger_id`、`business_type`、`trigger_x/y` 字段，用于清单生成
* **data_collection_executor:** `recordWorkerLoop` 在 TriggerRecord 与 compress 之间插入清单生成步骤
* **data_collection_executor:** `reloadConfig` 支持 compliance_filter 的热重载
* **robot_data_collection.json:** 为视觉采集策略添加 `maskingConfig` 配置块

## [1.8.1] - 2026-04-30

**配置外部化与环境变量统一**

将硬编码的规划参数（grid_resolution、max_planning_steps、planning_dt 等）外部化到 YAML 配置文件；环境变量从 `DCP_*` 统一为 `AER_*` 前缀；日志级别降噪（AD_INFO→AD_DEBUG）；删除已废弃的 Zhiyuan 集成测试。

### Features

* **runtime_config:** 从 YAML 读取 `grid_resolution`、`upload_threshold`、`trail_publish_interval_sec`，替代硬编码默认值
* **humanoid_planner:** `max_planning_steps` 和 `planning_dt` 从 YAML 配置读取（`max_planning_steps`、`policy_hz`），替代硬编码常量
* **path_visualizer:** 轨迹发布间隔改为构造函数参数，支持 `setTrailPublishInterval()` 动态调整
* **planner_factory:** 环境变量统一为 `AER_MODE`、`AER_MODEL_PATH`、`AER_CONFIG_PATH`（原 `DCP_*`）

### Changes

* **planner_weights:** 调整奖励权重 `w_time` 0.1→0.02、`collision_penalty` -10→-2；`grid_resolution` 1.0→0.5 匹配 LivelyBot 环境
* **logging:** AutoPlanner、HumanoidPlanner、PlannerFactory 大量 AD_INFO 降级为 AD_DEBUG，多行日志合并为单行
* **setup.bash:** 新增 `AER_CONFIG_PATH` 环境变量，默认模型更新为 `nav_data_exp030.onnx`
* **collection_feedback:** 构造函数接受可配置的 `upload_threshold` 参数

### Removed

* **tests:** 删除已禁用的 Zhiyuan 集成测试目录（5 文件，~1500 行）

## [1.8.0] - 2026-04-25

**IPlanner 接口抽象与 DataCollectionPlanner 解耦重构**

引入 `IPlanner` 统一规划器接口，支持多态调用与工厂创建；将 DataCollectionPlanner 从 God Class 分解为薄协调器，职责委托给 `RobotPositionTracker`、`RobotController` 等子系统。修复环形缓冲区录制时长不正确的 bug。

### Features

* **i_planner:** 新增 `IPlanner` 抽象接口，定义 `planMission()`、`updateWithNewData()`、`reportCoverageMetrics()`、`getAverageReward()` 等多态方法
  - `AutoPlanner` 和 `HumanoidPlanner` 均实现 `IPlanner` 接口
  - `PlannerFactory` 基于 `IPlanner` 创建规划器实例
* **sector_computer:** 从 `HumanoidPlanner` 中提取 `SectorComputer` 工具类，解耦扇区计算（data_value 8方向、obstacle 4方向）
* **runtime_config:** 新增 `RuntimeConfig` 统一配置结构体，替代多参数构造函数

### Bug Fixes

* **recording_duration:** 修复异步录制时 forward 环形缓冲区仅捕获 ~5s 数据（应为 10s）的 bug
  - 根因：`handleTriggerService` 将录制任务入队后，后台 worker 执行 `TriggerRecord` 时环形缓冲区已旋转
  - 修复：在 executor 线程立即调用 `snapshotForwardBuffers()` 快照 forward 数据，通过 `RecordTask::saved_forward_buffers` 传递给后台 worker
  - 新增 `TriggerRecord` 三参数重载接收预快照数据，两参数重载保留向后兼容
  - `TimestampedData` 从类内私有提升到 `aurora::collector` 命名空间以支持跨类型使用
* **write_ringbuffer:** 新增 `[DIAG]` 日志输出实际时间戳范围，修复 `bag_info_` 时间范围被 topic 交叉写入覆盖的问题

### Refactor

* **data_collection_planner:** 从 God Class（~1600 行）分解为薄协调器（~400 行）
  - 里程计跟踪 → `RobotPositionTracker`
  - 机器人控制 → `RobotController`（统一 path_tracking / velocity_cmd 模式）
  - 主循环逻辑委托给子系统，`DataCollectionPlanner` 仅负责初始化和协调
* **main.cpp:** 精简 ~600 行，移除内联业务逻辑
* **planner_factory:** 接口统一为 `IPlanner`，构造函数签名简化

### Changes

* **tests:** `test_dcp_integration.cpp` 适配新 API（`RuntimeConfig` 构造、`planMission()`、`executeMission()`、`getStats()`）

## [1.7.0] - 2026-04-17

**RL Planner 架构重构：双模式规划器与目录重组**

将 4 模式架构（AUTO / HUMANOID / RULE / NAV_DATA）简化为 2 模式（AUTO / HUMANOID），Humanoid 统一使用 43-dim 状态空间与 3-dim 连续速度动作。源码目录从 `navigation_planner` 重命名为 `rl_planner_infer`，子目录简化命名。

### Refactor

* **architecture:** PlannerMode 枚举精简为 AUTO=0, HUMANOID=1，移除 RULE 和 NAV_DATA
  - 删除 `PlannerType` 枚举和 `PlannerModeConfig` 结构体
  - 移除 `RulePlanner` 及所有 RULE 模式代码
  - NavData* 系列类全部重命名为 Humanoid*（NavDataPPOAgent → HumanoidPPOAgent 等）
* **humanoid_planner:** 移除 75-dim 双模式分支，HumanoidPlanner 统一为 43-dim 单一规划路径
  - 移除旧成员：`ppo_agent_`(75-dim)、`ContinuousPPOConfig`、`PlannerState`(75-dim)
  - 重命名 nav_data 成员：`nav_data_agent_` → `ppo_agent_`，`selectNavDataAction()` → `selectAction()` 等
  - `planNavData()` 逻辑提升为 `planUnified()` 主路径
* **state_traits:** 新建 `HumanoidStateTraits`（43-dim），替换旧 75-dim 版本
  - 43 维特征布局：base velocity(6) + position(2) + heading(2) + goal(5) + data_value(8) + obstacle(4) + collection(4) + env(2) + gait(1) + action_history(8) + budget(1)
* **state_converter:** 重写为 43-dim 转换，委托给 `HumanoidState::fromStateInfo()`

### Changes

* **directory:** `src/navigation_planner/` → `src/rl_planner_infer/`，子目录简化：
  - `common/` → `config/`，`planner/` → `core/`，`rl_policy/` → `agents/`
  - `safety_layer/` → `safety/`，`state/` → `observation/`，`value_map/` → `maps/`
* **onnx_inference_engine:** HumanoidInferenceEngine 从 `<75,3>` 改为 `<43,3>`，移除 HumanoidJointInferenceEngine 和 NavDataInferenceEngine
* **config:** `planner_weights.yaml` humanoid 配置替换为 nav_data 的 43-dim 参数（10 分量奖励函数、速度范围、控制频率等）
* **main:** 移除 `nav_data` 模式选项，仅保留 auto/humanoid

### Removed

* **old_humanoid:** 删除 75-dim HumanoidPPOAgent、HumanoidAction(18-DOF)、HumanoidRewardCalculator、HumanoidStateTraits
* **rule_planner:** 删除 RulePlanner 及工厂注册
* **11 个文件**被删除，~1700 行旧代码移除

---

## [1.6.0] - 2026-04-14

**RL Planner 推理优化与可视化增强**

### Features

* **planner:** 闭环速度跟踪导航 — 重写 `executeWithVelocityCommands()` 为 waypoint-by-waypoint 闭环跟踪模式，替代开环 action 播放
  - 50ms 控制周期、P 控制角速度、自适应前进速度
  - waypoint 超时跳过机制（15s）
  - cooldown 阻塞时等待重试（1.5s），大幅提升采集率（1 → 2-3 点/路径）
* **planner:** 使用实际 odom 位置作为规划起点，同步 humanoid_state 避免起点漂移
* **planner:** 新增 `getCurrentRobotYaw()` 获取实时朝向用于航向控制
* **humanoid_planner:** 直线插值路径 fallback — ONNX 模型不可用时生成等间距直线 waypoint 路径
* **humanoid_planner:** PPO 目标导航修正 — 补偿 observation 缺少 goal_heading 的问题，按比例修正 turn_rate 和 forward_velocity
* **humanoid_planner:** 记录 ONNX 模型加载状态（`model_loaded_`），区分模型可用与 fallback

### Changes

* **humanoid_state:** 启用预留的 4 维 observation 槽位用于目标方向信息（goal_heading, goal_relative_bearing）
* **config:** `cooldownDurationSec` 从 10s 降至 1s，解决整条路径仅 1 次采集的问题
* **config:** `min_collection_interval` 从 1.0s 降至 0.5s，匹配 waypoint 间距时间
* **main:** 默认 action_type 改为 `path_tracking`

### Visualization

* **path_visualizer:** 新增实时机器人轨迹 `/robot/trail` (nav_msgs/Path)，5Hz 增量发布，最大 2000 点
* **path_visualizer:** 新增采集点标记 `/collection_points_vis` (青色球体)，每次采集后实时更新
* **path_visualizer:** 修复规划路径颜色（黄色 → 绿色），线宽从 0.2 降至 0.15
* **path_visualizer:** frame_id 从 `map` 改为 `odom`，与 TF 树一致
* **rviz:** 新增 Path 和 Marker 显示入口，Odom 只显示当前位置（Keep=1）
* **rviz:** 修复 `robot_state_publisher` 的 `/joint_states` remap

### Performance

* **robot_sim:** 运动控制更新提前到位置更新之前，使用新朝向计算位移，提升跟踪精度
* **robot_sim:** VelocityLocomotionController 禁用 Ruckig、smoothing_time 降至 100ms，提升响应速度
* **qos:** TF 和 visualization publisher 从 BEST_EFFORT 改为 RELIABLE，解决 RViz QoS 不兼容问题

---

## [1.5.0] - 2026-04-14

**Observability : QoS Profiles, Structured Logging, Audit Trail & Health Monitoring**

This release introduces centralized ROS2 QoS management, structured JSON logging, an audit trail framework, and a system health monitor. A comprehensive test suite (11 unit + 2 integration tests) is also added with coverage support.

### Features

* **ros2:** add centralized QoS profile management
  - Create `qos_profiles.h` with 7 typed profiles: sensor_data, odometry, velocity_cmd, tf_transforms, static_data, visualization, channel_data
  - Each profile tuned with appropriate reliability, deadline, and liveliness settings
  - Replace hardcoded `rclcpp::QoS(10)` across all publishers and subscribers (6 files)

* **ros2:** add QoS event callback utilities
  - Create `qos_callbacks.h` with deadline-missed and liveliness-lost handlers
  - Provide template helpers `installDeadlineCallback` and `installLivelinessCallback`

* **ros2:** add system health monitoring node
  - Create `HealthMonitor` publishing `/robot/health` at 1Hz with JSON payload
  - Track state machine state, odometry/velocity liveliness, recording status, disk/memory usage, uptime

* **logging:** add structured JSON logging (JSONL)
  - Create `StructuredLogWriter` singleton writing JSONL alongside text logs
  - Add `CorrelationId` for thread-local request tracking
  - Add `AD_INFO_S` / `AD_ERROR_S` / `AD_WARN_S` macros with JSON context
  - Initialize in `main.cpp` with `.jsonl` file extension

* **audit:** add audit trail framework
  - Create `AuditLogger` singleton with 9 event types (STATE_TRANSITION, RECORDING_START/STOP, TRIGGER_FIRED, CONFIG_RELOAD, ERROR_OCCURRED, QOS_VIOLATION, UPLOAD_START/COMPLETE)
  - JSONL output with sequence numbers, ISO 8601 timestamps, and correlation IDs
  - Wire state transitions to audit trail in `state_machine.cpp`

* **testing:** add comprehensive test suite
  - Add 11 unit tests: error_handler, ring_buffer, state_machine, foot_trajectory, safety_system, kinematics, data_collection_executor, data_encryption, ros2bag_recorder, file_compress
  - Add 2 integration tests: gait_trigger_chain, data_upload_chain
  - Add `tests/unit/` and `tests/integration/` subdirectories

### Build

* **cmake:** add code coverage support
  - Add `ENABLE_COVERAGE` option with `--coverage -O0 -g` flags
  - Add `enable_testing()` for CTest integration

* **setup:** update environment setup
  - Add `_aer_reset_env()` to clear stale environment variables on source
  - Update default model path to `humanoid_ppo_v3.onnx`

### Bug Fixes

* **error_handler:** add missing `<thread>` and `<filesystem>` includes

## [1.4.0] - 2026-04-10

**Production Readiness : Safety & Quality Foundations**

This release implements the production readiness roadmap, focusing on safety infrastructure, error handling, and security hardening. All state machine event handlers are now active, with callback-driven architecture and audit logging.

### Features

* **state_machine:** activate event-driven state machine
  - Uncomment and enable all 8 state event handlers (INITIALIZING, IDLE, PLANNING, NAVIGATING, DATA_COLLECTING, UPLOADING, ERROR, SHUTTING_DOWN)
  - Implement ActionCallbacks to bridge DCP methods to state machine transitions
  - Add audit logging for all state transitions (from/to/event/timestamp)
  - Implement degradation mode for non-critical safety violations
  - Remove hardcoded trigger logic (`current_waypoint_index_ % 5`) in favor of callback-based approach

* **error_handling:** add centralized error handler
  - Create `ErrorHandler` singleton with exponential backoff retry
  - Support fallback execution with degrade policy
  - Add disk-safe write handling (skip on disk full instead of crash)
  - Add configuration validation helpers (range check, NaN/infinity rejection)

* **main:** implement signal handling and graceful shutdown
  - Register SIGTERM/SIGINT handlers for clean shutdown
  - Register SIGSEGV/SIGABRT handlers with stack backtrace on crash
  - Use `sig_atomic_t` flag for thread-safe shutdown signaling
  - Properly close resources before exit

* **config:** add environment variable substitution
  - Support `${ENV_VAR}` syntax for environment variables
  - Support `${ENV_VAR:-default}` syntax with default values
  - Create `config/app_config.json.template` as reference
  - Add `config/app_config.json` to `.gitignore`

### Bug Fixes

* **main:** fix logging stage pairing on error paths
  - Call `LOG_INIT_STAGE_END` even when initialization fails
  - Ensure all `LOG_INIT_STAGE_BEGIN` have matching `END` calls

* **startup_logger:** fix stage begin logging newline
  - Change `std::flush` to `std::endl` so stage begin occupies its own line

* **state_machine:** fix SystemState namespace references
  - Update `TriggerManager` and `RuleTrigger` to use namespaced `SystemState`
  - Create local `TriggerState` enum for trigger-level state tracking

### Improvements

* **aws_uploader:** enhance network retry with exponential backoff
  - Add exponential backoff: 10s → 20s → 40s → 80s
  - Add max consecutive failures limit (5 attempts) to skip problematic files
  - Reset backoff on successful upload

* **mqtt:** improve MQTT reconnection
  - Add exponential backoff: 5s → 10s → 20s → 40s
  - Add error logging when max retries exhausted
  - Properly notify when reconnection fails

* **recorder:** add disk error handling
  - Add disk space check before opening bag (require 256MB available)
  - Add error handling for directory creation failures
  - Use `std::error_code` for filesystem operations

* **app_config:** add configuration validation
  - Add `safeInt()` helper for range-bounded integer parsing
  - Use default values when required fields are missing
  - Reject NaN/infinity values with warning and default fallback

### Security

* **config:** remove plaintext credentials from repository
  - Create template file with `${ENV_VAR}` placeholders
  - Update `.gitignore` to exclude real configuration files
  - Support runtime credential injection via environment

### Refactor

* **state_machine:** simplify state enumeration
  - Reduce from 13 states to 8 core states
  - Remove redundant states (TRIGGERED, UNTRIGGERED, DATA_COLLECTED, etc.)
  - Map TRIGGERED → DATA_COLLECTING, UNTRIGGERED → IDLE for backward compatibility

* **rule_trigger:** decouple trigger state from system state
  - Create `TriggerState` enum (IDLE, TRIGGERED, UNTRIGGERED)
  - Remove dependency on `SystemState` for trigger-level state tracking

## [1.3.1] - 2026-04-09

**Bug Fix Release: Recorder Statistics & Startup Logging**

This release fixes critical bugs in rosbag recording statistics and adds structured startup logging with version tracking.

### Bug Fixes

* **recorder:** fix bag recording statistics accumulation across recordings
  - Reset `bag_info_` struct in `Open()` to prevent message count/duration from accumulating
  - Enable `update_statistics()` call in `Write()` to properly track message counts
  - Fix duration calculation to use actual data timestamps (`end_timestamp - start_timestamp`) instead of system clock time
  - Report accurate message count: previously showed 4500 (2× actual) for 10+5s capture, now correctly shows 2250

### Features

* **build:** add Git commit hash and build timestamp tracking
  - Use `find_package(Git)` to extract short commit hash at build time
  - Add `MODULE_VERSION`, `GIT_COMMIT_HASH`, `BUILD_TIMESTAMP` compile definitions
  - Expose version info to application code for startup logging

* **main:** add structured startup logging with `StartupLogger`
  - Integrate `StartupLogger` for banner display and initialization stage tracking
  - Add `SensorConfigManager` for sensor configuration from JSON or auto-detection
  - Differentiate CLI mode (verbose) vs PROD mode (structured logging)
  - Display system info: module name, version, Git commit, build time, ROS2 version, sensors

### Changes

* **recorder:** reduce log verbosity for better signal-to-noise ratio
  - Change topic creation and bag open messages from `RCLCPP_INFO` to `RCLCPP_DEBUG`
  - Reduce verbose per-topic logging during ring buffer write

* **setup:** remove redundant banner and usage info
  - Simplify `setup.bash` to only set environment variables
  - Banner/display moved to `StartupLogger` in application

## [1.3.0] - 2026-04-08

**Feature Release: 3-DOF Velocity Command Mode & PPO Training Data Enhancement**

This release switches the default humanoid action mode from 18-DOF joint offset to 3-DOF velocity command, significantly simplifying the control pipeline and improving training data quality. The experience metadata system is overhauled to store complete (s, a, r, s') tuples for effective cloud PPO training.

### Features

* **action:** switch default action mode to 3-DOF velocity command
  - Change default `ActionType` from `JOINT_OFFSET` to `VELOCITY_CMD`
  - Change default `action_dim` from 18 to 3 (forward, lateral, angular velocity)
  - Add `HumanoidJointInferenceEngine` alias for 18-DOF mode (advanced use)
  - Update `stringToActionType()` to default to `VELOCITY_CMD`
  - Configure via `planner_weights.yaml`: `action_type: "velocity_cmd"` (default) or `"joint_offset"`

* **planner:** implement velocity command execution pipeline
  - Add `executeWithVelocityCommands()` method for 3-DOF action execution
  - Add `/robot/velocity_cmd` publisher for real-time velocity commands
  - Add goal offset logic when start is near mission area center
  - Store planned trajectory for velocity mode execution
  - Rebuild `DataManager` on `setMissionArea()` for correct map parameters

* **simulator:** integrate velocity control as default in robot_sim
  - Add `/robot/velocity_cmd` subscription in `RobotSimulatorV2`
  - Initialize `VelocityLocomotionController` by default
  - Implement `velocityControlUpdate()` for velocity-driven motion
  - Dual mode support: velocity command (default) or path tracking (fallback)
  - Rename node from `robotmc` to `robot_mc`

* **feedback:** overhaul experience metadata for PPO training
  - Replace lightweight 20-byte `ExperienceMetadata` with full (s, a, r, s') tuple
  - Store complete state/action vectors with float16 quantization (~628 bytes/sample)
  - Add `quantize()`/`dequantize()` for float ↔ uint16 conversion
  - Support up to 78-dim state and 18-dim action vectors
  - Update `generateMetadata()` to accept explicit state/action/next_state vectors
  - Enable cloud PPO policy gradient updates with complete training data

* **main:** add velocity mode support in execution loop
  - Read `action_type` from YAML config at runtime
  - Implement velocity command mode with position verification
  - Add error statistics query (`GetErrorStatistics`) for monitoring

### Changes

* **reward:** rebalance reward function weights for velocity mode
  - `w_data`: 0.05 → 1.5 (significantly increase data collection incentive)
  - `w_navigation`: 5.0 → 1.5 (reduce navigation dominance)
  - `w_safety`: 0.3 → 1.5 (increase safety priority)
  - `w_stability`: 0.2 → 1.2 (maintain high stability priority)
  - Update default weights in both `HumanoidRewardConfig` and `planner_weights.yaml`

* **build:** enable tests and improve compiler warnings
  - Re-enable `BUILD_TESTS` option (was commented out)
  - Replace `-w` (suppress all warnings) with `-Wall -Wextra -Wpedantic`

### Configuration

* **planner_weights.yaml:** update humanoid section defaults
  - `action_dim`: 18 → 3
  - `action_type`: "joint_offset" → "velocity_cmd"
  - `enable_joint_offset`: true → false
  - `velocity.enabled`: false → true
  - Reward weights rebalanced (see above)

---

## [1.2.1] - 2026-04-03

**Bug Fix: ROS2 Callback Starvation Causing Odometry Staleness**

Fixes a critical issue where odometry data becomes stale (15-30 seconds old) after running for a while in dual-process mode (robot_sim + dcp). The stale odom broke RL-based path planning and data collection by causing waypoint tracking timeouts.

**Root Cause:** All ROS2 callbacks (odometry subscriber, trigger service handler, ChannelManager subscriptions) shared the default MutuallyExclusive callback group on the DCP node. The `MultiThreadedExecutor` could only dispatch one callback at a time. When `handleTriggerService()` blocked for 5+ seconds (TriggerRecord backward capture wait + ring buffer write + compress), odometry callbacks were queued but never dispatched, causing position data to go stale.

### Bug Fixes

* **executor:** fix ROS2 callback starvation causing odometry staleness
  - Isolate odometry subscriber to its own `MutuallyExclusive` callback group
  - Isolate trigger recording service to its own `MutuallyExclusive` callback group
  - Isolate ChannelManager subscriptions to their own `MutuallyExclusive` callback group
  - Increase `MultiThreadedExecutor` thread count from 2 to 4
  - Move `TriggerRecord()` (5s backward capture + ring buffer write) and `compress()` to background worker thread
  - Service handler now enqueues recording tasks and returns immediately (microseconds vs 5+ seconds)

* **planner:** add isolated callback group for odometry subscriber
  - Create dedicated callback group for `/robot/odom` subscription
  - Ensures odom callbacks are never blocked by service handlers or channel processing

* **channel:** add isolated callback group for ChannelManager subscriptions
  - Create dedicated callback group for all generic topic subscriptions
  - Prevents channel callbacks from blocking or being blocked by other operations

## [1.2.0] - 2026-03-23

**🎉 Major Release: Dual-Process Synchronization & Trigger Accuracy**

This release contains significant improvements to the dual-process operation (DCP + robot_sim), addressing critical issues with odometry synchronization and trigger collection accuracy. These fixes enable reliable long-running missions with precise data collection based on actual robot position rather than planned waypoints.

**Key Highlights:**
- ✅ Odometry age reduced from >5000ms to ~19ms (99.6% improvement)
- ✅ Triggers now use actual robot position instead of planned waypoints
- ✅ Path continuity prevents robot jumps between planning cycles
- ✅ Thread-safe position tracking infrastructure
- ✅ Improved CPU efficiency with condition variable synchronization

**Upgrade Recommendation:** **Highly Recommended** for all users running dual-process mode. This release resolves critical synchronization issues that could cause data collection failures and inaccurate training data.

### Bug Fixes

* **dual-process:** fix odometry staleness during dual-process operation
  - Add condition variable synchronization mechanism for waypoint waiting
  - Implement `waypoint_wait_time_` parameter (default: 3s) for DCP coordination
  - Fix robot_sim stopping after final waypoint and ceasing odometry updates
  - Reduce odometry age from >5000ms to ~19ms during steady state
  - Add proper state tracking for wait management with mutex protection
  - Improve thread safety for concurrent odometry callback access

* **trigger:** fix trigger collection using planned waypoint coordinates instead of actual robot position
  - Add robot position tracking to `TriggerManager` with thread-safe updates
  - Implement `updateRobotPosition(x, y)` method to feed odometry data
  - Modify `shouldTrigger()` to use actual robot odometry position
  - Wire up odometry callback in `DataCollectionPlanner` to update trigger manager
  - Fix step distance calculations using actual position instead of planned waypoints
  - Resolve "Trigger accepted at (0.00, 0.00), step distance: 6.21 m" inconsistency
  - Update trigger context and DataPoint creation to use actual robot position

* **main:** fix path continuity between planning cycles
  - Add robot's current position to beginning of each new path
  - Implement `/robot/get_position` service call before path planning
  - Prevent sudden robot jumps when new path starts at different location
  - Reduce "timeout waiting for waypoint" errors caused by path discontinuity

### Features

* **trigger:** add robot position tracking infrastructure
  - Add `robot_position_` member to `TriggerManager` with mutex protection
  - Add `getRobotPosition()` accessor for thread-safe position queries
  - Integrate position updates with odometry callback at 50Hz
  - Support real-time trigger evaluation based on actual robot location

* **executor:** expose trigger manager accessor for integration
  - Add `getTriggerManager()` method to `DataCollectionExecutor`
  - Enable external components to update trigger manager state
  - Maintain encapsulation while allowing controlled access

### Performance

* **synchronization:** improve dual-process coordination efficiency
  - Replace busy-wait loops with condition variable synchronization
  - Reduce CPU usage during waypoint waiting periods
  - Enable robot_sim to continue odometry updates while waiting for DCP
  - Improve responsiveness to shutdown requests during wait states

* **trigger:** enhance trigger accuracy with real-time position tracking
  - Eliminate position discrepancies between planned and actual robot locations
  - Improve step distance measurement accuracy
  - Enable valid training data generation for reinforcement learning

### Code Quality

* **thread-safety:** add mutex protection for shared state
  - Protect robot position updates in `TriggerManager` with mutex
  - Protect waypoint state in `robot_simulator_v2` with mutex
  - Ensure condition variable synchronization is thread-safe

* **refactor:** improve code organization and clarity
  - Separate position tracking from trigger evaluation logic
  - Clarify distinction between planned waypoints and actual robot position
  - Add better variable naming (`robot_position_` vs `waypoint_`)
  - Improve error handling for service call timeouts

### Testing

* **validation:** confirm odometry freshness improvements
  - Verify odometry age stays < 100ms throughout 90-second test
  - Confirm robot continues through all waypoints without stopping
  - Validate condition variable notification mechanism

* **validation:** confirm trigger position accuracy
  - Verify trigger coordinates match actual robot odometry position
  - Confirm step distance calculations are accurate
  - Eliminate false triggers at origin when robot has moved

### Documentation

* **planning:** add comprehensive planning documentation
  - Create `task_plan.md` with 4-phase development roadmap
  - Create `findings.md` with technical discoveries and root cause analysis
  - Create `progress.md` with session history and current status
  - Document odometry synchronization architecture
  - Document trigger position bug and solution

---

## [1.1.9] - 2026-03-19

### Breaking Changes

* **ops:** restructure into `dev/`, `prod/`, `common/` subdirectories
* **ops:** rename all `DCP_*` variables to `AER_*`
* **ops:** rename `dcp` service to `aer` service

### Features

* **ops:** add `aer` command-line tool for development environment (ops/dev/)
* **ops:** add systemd service management for production (ops/prod/)
* **ops:** add ops documentation (README.md, QUICKREF.md)

### Documentation

* **README:** simplify by 50%, link to ops documentation
* **docs/operations:** update for new ops structure
* **docs/getting-started:** update first-run guide

### Refactoring

* **ops:** standardize naming from DCP to AER across all components
* **ops:** improve aer command path resolution for symlinks

* **ops:** organize scripts by environment type
  - Development tools: `ops/dev/` (aer command, installation, cleanup)
  - Production tools: `ops/prod/` (systemd service, configuration, startup)
  - Common tools: `ops/common/` (commit hooks, release scripts)
  - Clear separation of concerns and usage scenarios

### Fixed

* **ops:** fix aer command path resolution when installed as symlink
  - Correctly identify project root directory when aer is symlinked
  - Prevent creation of PID files in wrong directory (e.g., /usr/local/bin/.pids)
  - Ensure all file paths are relative to actual project root

---

## [1.1.8] - 2026-03-17

### Breaking Changes

* **trigger:** remove backward compatibility - pure service-based architecture
  - **REMOVE**: `DataCollectionExecutor::triggerRecording(const TriggerContext&)` method
  - **MODIFY**: `DataCollectionPlanner` now uses ROS2 service client instead of direct method call
  - **ALL trigger sources must now use `/robot/trigger` service**
  - Migration guide: Replace direct `triggerRecording()` calls with ROS2 service calls
  - This change enables true loose coupling and event-driven architecture

### Features

* **trigger:** implement ROS2 service-based event-driven trigger mechanism
  - Add `TriggerRecording.srv` service definition for `/robot/trigger` endpoint
  - Add service server in `DataCollectionExecutor` with `handleTriggerService()` callback
  - Implement async service client in `GaitTrigger` for gait-based collection triggering
  - Implement async service client in `RuleTrigger` for rule-based collection triggering
  - Implement async service client in `DataCollectionPlanner` for waypoint-based triggering
  - Modify `RuleTrigger` to inherit from `rclcpp::Node` for ROS2 integration
  - Support cooldown period, disk space checks, and detailed error responses

### Architecture

* **trigger:** decouple trigger modules from recording module via ROS2 service
  - Enable true event-driven architecture where triggers automatically initiate recording
  - Loose coupling between trigger detection and recording execution
  - Unified service interface for all trigger sources (GaitTrigger, RuleTrigger)
  - Async non-blocking service calls for high-performance trigger detection
  - Extensible design allowing future triggers to use same service interface

### Build

* **cmake:** add TriggerRecording service to build system
  - Add `data_collection/srv/TriggerRecording.srv` to `rosidl_generate_interfaces()`
  - Update `tests/CMakeLists.txt` to link service typesupport libraries
  - Fix linking issues for test executables using service interfaces

---

## [1.1.7] - 2026-03-16

### Bug Fixes

* **dual-process:** fix robot_sim stopping after first data collection cycle
  - Add path continuity by prepending robot's current position to each new path
  - Implement `/robot/get_position` service call to get current location before planning
  - Fix "timeout waiting for waypoint" errors caused by discontinuous paths between cycles
  - Resolve odometry staleness issue where robot would stop moving after cycle 1

* **simulator:** add waypoint wait mechanism for DCP synchronization
  - Add `waypoint_wait_time_` parameter (default: 3s) for waiting at each waypoint
  - Add `last_waypoint_reach_time_` and `waiting_at_waypoint_` state tracking
  - Modify `pathTrackingControl()` to implement wait logic after waypoint arrival
  - Reset wait state in `setTargetPath()` when new path is set
  - Synchronize robot_sim movement speed with DCP data collection timing

### Changes

* **main:** update main execution loop for path continuity
  - Add robot position retrieval between path planning and path setting
  - Insert current robot position at beginning of collection path
  - Add logging for position retrieval and path adjustment

* **simulator:** enhance RobotSimulatorV2 with wait mechanism
  - Add waypoint wait mechanism to constructor initialization
  - Implement wait state management in path tracking control loop

---

## [1.1.6] - 2026-03-13

### Refactor

* **planner:** optimize DataCollectionPlanner::executeWithFeedback
  - Extract 6 helper methods: getCostMap(), calculateDistance(), createTriggerContext(),
    tryCollectAtWaypoint(), updateReachabilityForPath(), syncCollectionParamsFromConfig()
  - Add CollectionParams struct to replace hard-coded constants
  - Refactor executeWithFeedback() from 259 to ~90 lines
  - Optimize waypoint matching algorithm from O(n*m) to O(n+m)
  - Remove ~200 lines of duplicated/commented code
  - Update executeHumanoidWithFeedback() and onReplanTriggered() to use helper methods
  - All changes maintain backward compatibility

### Performance

* **planner:** improve waypoint matching efficiency
  - Replace nested loops with two-pointer algorithm
  - Reduce time complexity from O(n*m) to O(n+m)

---

## [1.1.5] - 2026-03-11

### Features

* **3rdparty:** add Ruckig real-time motion planning library
  - Add Ruckig as third-party dependency for jerk-constrained trajectory planning
  - Support velocity/acceleration/jerk limits for smooth robot motion
  - Integration with gait control system for improved foot trajectory generation

### Build

* **dependencies:** update 3rdparty module structure
  - Reorganize third-party dependencies for better modularity
  - Add Ruckig build configuration

---

## [1.1.4] - 2026-03-11

### Features

* **locomotion:** add Zhiyuan robot reference integration with advanced gait control
  - Add `RuckigTrajectoryAdapter` for real-time smooth trajectory planning (<1ms)
  - Add `VelocityLocomotionController` for velocity-command interface
  - Add `MotionModeManager` supporting PASSIVE, DAMPING, JOINT, STAND, LOCOMOTION modes
  - Add `SafetySystem` with emergency stop, joint limits, input timeout, watchdog
  - Add `AdaptiveGaitController` for velocity/turn/slope/terrain adaptation
  - Add `PresetMotionLibrary` with 50+ predefined motion primitives
  - Add `GroundContactModel` for realistic foot-ground interaction
  - Add `locomotion_config.yaml` with feature flags for progressive deployment

* **action:** extend action space with velocity command mode
  - Add `velocity` action type as alternative to `joint_offset`
  - Support 3-DOF velocity commands (forward, lateral, angular)
  - Add configurable velocity limits and clamping
  - Maintain backward compatibility with joint offset mode (default)

* **state:** extend observation space for velocity control mode
  - Add `extended_state_dim: 77` for velocity observations (75 + 3)
  - Add optional velocity, mode, and safety state observations
  - Configure via `enable_velocity_obs`, `enable_mode_obs`, `enable_safety_obs`

### Refactor

* **docs:** restructure documentation following Diátaxis Framework
  - Add `docs/README.md` as documentation navigation hub
  - Reorganize into: getting-started/, user-guide/, developer-guide/, operations/, testing/, product/, hardware/, api/, faq/
  - Remove legacy documentation: docs/IMPLEMENTATION_SUMMARY.md, docs/architecture_docs/, docs/modules/, docs/images/
  - Add comprehensive documentation index with completion status

* **testing:** add Zhiyuan integration test suite
  - Add `test_ruckig_adapter.cpp` for trajectory planning tests
  - Add `test_velocity_controller.cpp` for velocity control tests
  - Add `test_safety_system.cpp` for safety system tests
  - Add `test_system_integration.cpp` for end-to-end integration tests
  - All tests include performance benchmarks and coverage metrics

### Documentation

* **docs:** add comprehensive documentation structure
  - Add getting-started guides: installation, first-run
  - Add user-guide: concepts, configuration, operation
  - Add developer-guide: architecture, components, setup
  - Add operations, testing, product, hardware, api, faq sections
  - Document all new locomotion components in architecture/

---

## [1.1.3] - 2026-03-10

### Features

* **costmap:** add execution-aware reachability tracking for both auto and humanoid modes
  - Add `updateReachability()` to track planned vs actual position discrepancies
  - Add `getReachabilityScore()` to compute cell reachability [0-1]
  - Add `hasReliableReachability()` to check data reliability (min_samples threshold)
  - Add `getExecutionStats()` to retrieve attempt/success counts per cell
  - Add `getEffectiveCost()` combining data value and reachability for planning decisions
  - Support configurable reachability parameters: decay (0.95), min_samples (3), tolerance (0.5m)

* **auto:** extend state space with reachability feature
  - Increase state dimension from 24 to 25 (add 1 reachability feature)
  - Add reachability score as 25th state feature in PPO observations
  - Enable auto planner to consider historical execution success in planning
  - Configure reachability via: `common_reachability_decay`, `common_reachability_min_samples`, `common_reachability_position_tolerance`

* **humanoid:** integrate reachability into state and reward computation
  - Add `reachability_score` and `reachability_confidence` to `HumanoidStateInfo`
  - Include reachability as feature[74] in 74-dimensional state space
  - Add reachability-aware reward weights: `w_reachability`, `high_reachability_bonus`, `low_reachability_penalty`, `confidence_bonus`
  - Use `getEffectiveCost()` for collision detection considering reachability

* **feedback:** add execution feedback mechanism for reachability updates
  - Add `ExecutionFeedback` struct capturing planned position, actual position, success, stability
  - Add `reportExecutionFeedback()` in `HumanoidPlanner` for single-point feedback
  - Add `reportExecutionFeedbackBatch()` for efficient batch updates
  - Integrate reachability tracking in `DataCollectionPlanner::executeWithFeedback()`

### Bug Fixes

* **planner:** fix double initialization of HumanoidPlanner
  - Remove `initialize()` call from `HumanoidPlanner` constructor
  - Match pattern used by `AutoPlanner` where `initialize()` is called by `DataCollectionPlanner`
  - Eliminate duplicate ONNX model loading and configuration logs

* **build:** fix compilation errors in navigation planner
  - Fix typo in `planner_config_manager.cpp`: `ContinuousAutoPPOConfig` → `ContinuousPPOConfig`
  - Fix typo in `auto_route_optimize.cpp`: `static_cast<>` → `static_cast<int>`
  - Fix forward declaration conflict in `planner_base.hpp` for `Trajectory` type alias

---

## [1.1.2-rc.1] - 2026-03-10

### Refactor

* **rl_policy:** rename auto mode reward files for consistency
  - Rename `auto_reward_calculator.h/cpp` → `auto_reward.h/cpp`
  - Update header guard: `AUTO_REWARD_CALCULATOR_H` → `AUTO_REWARD_H`
  - Remove duplicate `computeReward` overload that conflicted with type aliases
  - Update all include references across the codebase
  - Maintain backward compatibility with type aliases

* **includes:** fix broken include references after file reorganization
  - Update `humanoid_planner.h`: remove broken `#include "../rl_policy/ppo_agent.h"`
  - Update `rule_planner.h`: use `auto_route_optimize.h` and `auto_reward.h`
  - Update `planner_base.hpp`: proper forward declaration for `Trajectory` type alias

---

## [1.1.2] - 2026-03-06

### Features

* **simulator:** implement realistic bipedal gait simulation for humanoid robot
  - Add proper leg geometry based on real humanoid robots (upper leg: 35cm, lower leg: 35cm, hip width: 10cm)
  - Implement swing/stance phase alternation with π phase difference between legs
  - Add smooth foot trajectory using sine-squared curve for natural step pattern
  - Implement analytical inverse kinematics using cosine theorem for joint angles
  - Update torso position based on support foot (no sliding)

* **collection:** implement gait-based data collection trigger
  - Add `GaitTriggerConfig` with step distance threshold (default: 15cm) and cooldown (default: 1s)
  - Add `GaitTrigger` node that monitors robot actual state (odometry, joint_states)
  - Implement trigger logic based on actual foot strikes, not planned waypoints
  - Add footprint history tracking (1000 points) with deduplication
  - Add collection trigger only in stable stance phase (middle 60% of support phase)

* **planner:** refactor collection execution to use gait-based triggers
  - Add step distance validation in `executeWithFeedback` (min 15cm between collections)
  - Add time interval validation (min 1 second between collections)
  - Replace waypoint-based collection with actual robot position-based collection
  - Add `collector::TriggerContext` integration for gait-aware data collection

### Bug Fixes

* **trigger:** fix duplicate collection at same waypoint
  - Replace `shouldTrigger()` that always returned true with proper validation
  - Add distance check from last collected position
  - Add time interval check from last collection
  - Add history deduplication to prevent over-sampling same area

* **trigger:** fix trigger manager initialization
  - Add proper constructor initialization for `last_trigger_position_` and `last_trigger_time_`
  - Add `shouldTriggerGaitBased()` implementation with proper validation logic

### Changes

* **simulator:** upgrade robot simulator from simple sine wave to realistic gait
  - Step length: 25cm, step height: 5cm, step duration: 0.8s, walk speed: 0.3m/s
  - Torso height: 0.7m (based on leg geometry)
  - Update rate: 50Hz
  - Publishes 12 joint states with proper inverse kinematics

* **collection:** validate gait-based collection system
  - Tested with 90 collection triggers across 501 planned waypoints
  - Step distances range from 0.28m (adjustment) to 1.36m (normal walking)
  - Collection intervals maintained at ~1 second
  - No duplicate collection points detected
  - Robot simulator moving properly with realistic gait patterns

---

## [1.1.1] - 2026-03-06

### Bug Fixes

* **onnx:** fix ONNX Runtime API compatibility for version 1.16.3
  - Update `GetOutputName()` to `GetOutputNameAllocated()` for ONNX Runtime 1.16+ compatibility
  - Upgrade ONNX Runtime from 1.10.0 to 1.16.3 for opset 17 support
  - Remove manual memory management (allocator.Free) with new allocated string API

### Changes

* **dependencies:** upgrade ONNX Runtime to 1.16.3
  - Add support for ONNX opset 17 (previously limited to opset 15)
  - Enable loading of newer ONNX models exported with opset 17

---

## [1.1.0] - 2026-03-04

### Features

* **config:** unify YAML configuration format for humanoid and auto modes
  - Add `planner_mode` selector for choosing planner type (humanoid/auto)
  - Add `common` section for shared parameters
  - Add `humanoid` section with nested configuration (state, action, ppo, reward)
  - Add `auto` section with nested configuration (path_planning, ppo, nav_planner, sampling, coverage, semantic_constraints, reward)
  - Remove old backward-compatible YAML format
  - Add `loadModeSpecificParameters()` function in `PlannerUtils`

* **humanoid:** add humanoid robot mode support with 74-dim state and 18-dim continuous action
  - Add `HumanoidPlanner` for bipedal robot navigation
  - Add `HumanoidPPOAgent` with continuous action space
  - Add `HumanoidState` with 74-dimensional state representation
  - Add `HumanoidAction` with 18-dimensional continuous action
  - Add `HumanoidRewardCalculator` with multi-objective reward function
  - Add `HumanoidDataValueModel` for multi-dimensional data value assessment
  - Add fall detection, gait phase control, and stability metrics

* **auto:** update auto mode to support new YAML configuration format
  - Update `rl_planner.cpp` to use `loadModeSpecificParameters("auto", ...)`
  - Update `rule_planner.cpp` to use new key format with backward compatibility
  - Update `ppo_agent.cpp` with `use_ppo` member variable
  - Add backward-compatible key lookup for both old and new key formats

* **visualization:** add collected path visualization support in RViz2
  - Add `publishCollectedPath()` method to display actual robot trajectory
  - Add `updateCollectedPath()` method for incremental path updates
  - Add `clearCollectedPathHistory()` method to reset collected path display
  - Add new topic `/collected_path_vis` for collected path markers (blue color)
  - Add collected path history tracking in `PathVisualizer`
  - Update RViz configuration (`robot_demo_vis.rviz`, `robot_demo.rviz`) with Marker displays
  - Set marker lifetime to 0 (permanent display) for better visualization

* **data_collection:** integrate collected path visualization
  - Add `publishCollectedVisualization()` in `DataCollectionPlanner`
  - Add `clearCollectedVisualization()` in `DataCollectionPlanner`
  - Update `executeWithFeedback()` to publish collected path after data collection
  - Update `executeHumanoidWithFeedback()` to track and publish collected waypoints

### Bug Fixes

* **config:** fix YAML parsing error for string values (e.g., `planner_mode: "humanoid"`)
  - Add `safeConvertToDouble()` lambda to catch `YAML::BadConversion` exceptions
  - Skip non-numeric scalar values during YAML parameter loading

* **onnx:** fix ONNX inference error for models without `action_log_std` output
  - Add dynamic model output detection in `HumanoidPPOAgent`
  - Use `init_log_std` as fallback when `action_log_std` not present in model
  - Support both 3-output (`action_mean`, `action_log_std`, `value`) and 2-output (`action_mean`, `value`) models

* **visualization:** fix marker lifetime causing paths to disappear quickly
  - Change `MARKER_LIFETIME_SEC` from 0.2s to 0s (permanent display)
  - Add separate lifetime constants for different marker types

* **config:** fix missing Marker displays in RViz configuration
  - Update `robot_demo.rviz` with complete Marker display configuration
  - Add "Collected Path", "Planning Path", "Planning Trajectory" Marker displays
  - Add proper namespace filtering for each Marker topic

---

## [1.0.0] - 2026-02-25

### Features

* **data_collection:** add edge-cloud collaborative data collection architecture
  - Add `DataCollectionExecutor` for trigger-based recording with ring buffer (15s pre + 5s post capture)
  - Add `CollectionFeedback` for lightweight reward calculation and experience metadata generation (~20B per sample)
  - Add `DataManager` for data point management and CostMap updates
  - Add `ConfigWatcher` for hot-reload of JSON configuration files
  - Add `PathVisualizer` for RViz2 path visualization (planning path, trajectory, boundaries)
  - Add robot-specific data collection configuration `robot_data_collection.json`

* **simulator:** add bipedal robot simulator for testing
  - Add `RobotSimulator` with 12-joint configuration (6 per leg)
  - Publish sensor messages at 50Hz (odometry, joint states, IMU, TF)
  - Support gait-based locomotion with step phase control
  - Include URDF model and RViz2 configuration

* **testing:** add integration test suite for configuration hot-reload
  - Add automated test scripts for config reload functionality
  - Add test utilities for verifying data collection behavior

* **documentation:** add project documentation
  - Add `CLAUDE.md` for AI assistant project guidance
  - Add `IMPLEMENTATION_SUMMARY.md` for architecture overview

### Refactor

* **cleanup:** remove deprecated modules
  - Remove `auth_manager` module (authentication no longer needed)
  - Remove `data_storage` (storage moved to executor)
  - Remove `rsclbag_recorder` (replaced by ros2bag_recorder)
  - Remove `humanoid_interfaces` ROS2 definitions (moved to external package)

---

## [0.4.0] - 2026-02-09

### Features

* **uploader:** add AWS S3 multipart upload support with retry mechanism ([40b5065](https://github.com/xucongTHU/ad_data_closed_loop/commit/40b5065))
  - Implement `FileSplitter` for large file chunking
  - Add retry logic with exponential backoff for failed uploads
  - Support configurable chunk size and concurrent upload threads
  - Improve upload progress tracking and error reporting

* **core:** add state machine scheduling functionality in data collection planner ([f4e861c](https://github.com/xucongTHU/ad_data_closed_loop/commit/f4e861c))
  - Add `DataCollectionAnalyzer` for analyzing collected data
  - Integrate state machine with 8 states for data collection workflow
  - Refactor `StateMachine` to support event-driven transitions
  - Add scheduler integration for coordinated execution

* **config:** add AWS S3 configuration support and refactor uploader module ([79f72e5](https://github.com/xucongTHU/ad_data_closed_loop/commit/79f72e5))
  - Add `AwsDataUploader` class with S3 integration
  - Add S3 endpoint, bucket, and credential configuration to `app_config.json`
  - Refactor `TriggerManager` with improved channel management
  - Add `DataUploaderInterface` for uploader abstraction

### Refactor

* **data_collection:** restructure data collection system architecture and optimize path planning ([6bbf77c](https://github.com/xucongTHU/ad_data_closed_loop/commit/6bbf77c))
  - Move common utilities to `src/common/` for better code organization
  - Add `MemoryPool` and `MemoryMonitor` for efficient memory management
  - Add `PlatformAdapter` for cross-platform compatibility
  - Add `SIMDUtils` for SIMD-accelerated operations
  - Refactor `RLPlanner` with improved inference pipeline
  - Refactor `PPOAgent` with cleaner state management
  - Add `RouteOptimizer` for path planning optimization
  - Remove deprecated `priority_scheduler` module
  - Add humanoid ROS2 interfaces for robot control
  - Remove `data_collection/package.xml` (no longer standalone package)

---

## [0.3.0] - 2025-12-10

### Bug Fixes

* **build:** refactor cmake config and update data collection components ([c153eb2](https://github.com/xucongTHU/ad_data_closed_loop/commit/c153eb2022fdc4a6278535e581e7c721b59b5195))

### Features

* **architecture:** add submodule and update project structure ([1fcdc5b](https://github.com/xucongTHU/ad_data_closed_loop/commit/1fcdc5b196d0f29e2d2fc882295e80444cd2c7b9))
* **build:** add yaml-cpp dependency management and build support ([7193f57](https://github.com/xucongTHU/ad_data_closed_loop/commit/7193f5780e4c708ab2850c00bd5d887728cc906c))
* **data-processor:** remove costmap_builder and feature_alignment modules ([b831e7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/b831e7a5a0e0ce4b6a9cfd64801849120ddd68b7))
* **logger:** migrate logging macros to AD_* format ([25e2577](https://github.com/xucongTHU/ad_data_closed_loop/commit/25e257759fdf8a468e324d3b53d65a509d3985db))
* **navigation_planner:** add vehicle–edge PPO inference pipeline with ONNX Runtime integration ([2f9d363](https://github.com/xucongTHU/ad_data_closed_loop/commit/2f9d36310384afe940b9c62f4a89c267e8136637))
* **navigation-planner:** add PPO-based path planning support ([b26ba05](https://github.com/xucongTHU/ad_data_closed_loop/commit/b26ba053de393ecf279dd9473015648d77be516b))
* **planner:** implement navigation planning core, RL route optimization, and planning utils ([5ff2fcb](https://github.com/xucongTHU/ad_data_closed_loop/commit/5ff2fcb82d8bf499bb3c26bcb35a1ed6d7f99f51))
* **scenario:** add implementation details for turn detection and standstill scenarios ([bd28ff4](https://github.com/xucongTHU/ad_data_closed_loop/commit/bd28ff4668c9a892df4cc55bf3540dcbdf9334be))
* **scenario:** implement efficiency lane change and update scenario triggers ([51fbe0a](https://github.com/xucongTHU/ad_data_closed_loop/commit/51fbe0a6817a0410b86d8250faeccfa99189fc38))
* **signal_smoother:** implement signal smoothing with EMA and SMA options ([d5787cd](https://github.com/xucongTHU/ad_data_closed_loop/commit/d5787cdd9e5feb77cd7e5e819c281f7ddff2776f))
* **state-machine:** integrate state machine for data collection and navigation coordination ([c09fb7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/c09fb7a89f962769ae068ffd8335eda7fdcdd90a))
* **training:** implement advanced PPO training with configurable network and environment ([864865d](https://github.com/xucongTHU/ad_data_closed_loop/commit/864865d2052ee08edd0ec5cbad52d27e9910d43c))
* **training:** implement extended state representation and benchmark improvements ([19d9918](https://github.com/xucongTHU/ad_data_closed_loop/commit/19d991842888e81e814007849d85d85c0ab126a8))
* **utils:** add microsecond timestamp utility function ([86f1030](https://github.com/xucongTHU/ad_data_closed_loop/commit/86f1030199780ce85d6ab834e18e3d9f96aac3ea))
* **yaml-cpp:** add the yaml-cpp shared library file. ([c0022f6](https://github.com/xucongTHU/ad_data_closed_loop/commit/c0022f6f72901c26566c43c7d46b8dc6bc98fe9e))

---

## [0.2.0] - 2025-12-09

### Features

* **architecture:** add submodule and update project structure ([1fcdc5b](https://github.com/xucongTHU/ad_data_closed_loop/commit/1fcdc5b196d0f29e2d2fc882295e80444cd2c7b9))
* **build:** add yaml-cpp dependency management and build support ([7193f57](https://github.com/xucongTHU/ad_data_closed_loop/commit/7193f5780e4c708ab2850c00bd5d887728cc906c))
* **data-processor:** remove costmap_builder and feature_alignment modules ([b831e7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/b831e7a5a0e0ce4b6a9cfd64801849120ddd68b7))
* **logger:** migrate logging macros to AD_* format ([25e2577](https://github.com/xucongTHU/ad_data_closed_loop/commit/25e257759fdf8a468e324d3b53d65a509d3985db))
* **navigation_planner:** add vehicle–edge PPO inference pipeline with ONNX Runtime integration ([2f9d363](https://github.com/xucongTHU/ad_data_closed_loop/commit/2f9d36310384afe940b9c62f4a89c267e8136637))
* **navigation-planner:** add PPO-based path planning support ([b26ba05](https://github.com/xucongTHU/ad_data_closed_loop/commit/b26ba053de393ecf279dd9473015648d77be516b))
* **planner:** implement navigation planning core, RL route optimization, and planning utils ([5ff2fcb](https://github.com/xucongTHU/ad_data_closed_loop/commit/5ff2fcb82d8bf499bb3c26bcb35a1ed6d7f99f51))
* **scenario:** add implementation details for turn detection and standstill scenarios ([bd28ff4](https://github.com/xucongTHU/ad_data_closed_loop/commit/bd28ff4668c9a892df4cc55bf3540dcbdf9334be))
* **scenario:** implement efficiency lane change and update scenario triggers ([51fbe0a](https://github.com/xucongTHU/ad_data_closed_loop/commit/51fbe0a6817a0410b86d8250faeccfa99189fc38))
* **signal_smoother:** implement signal smoothing with EMA and SMA options ([d5787cd](https://github.com/xucongTHU/ad_data_closed_loop/commit/d5787cdd9e5feb77cd7e5e819c281f7ddff2776f))
* **state-machine:** integrate state machine for data collection and navigation coordination ([c09fb7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/c09fb7a89f962769ae068ffd8335eda7fdcdd90a))
* **training:** implement advanced PPO training with configurable network and environment ([864865d](https://github.com/xucongTHU/ad_data_closed_loop/commit/864865d2052ee08edd0ec5cbad52d27e9910d43c))
* **training:** implement extended state representation and benchmark improvements ([19d9918](https://github.com/xucongTHU/ad_data_closed_loop/commit/19d991842888e81e814007849d85d85c0ab126a8))
* **utils:** add microsecond timestamp utility function ([86f1030](https://github.com/xucongTHU/ad_data_closed_loop/commit/86f1030199780ce85d6ab834e18e3d9f96aac3ea))
* **yaml-cpp:** add the yaml-cpp shared library file. ([c0022f6](https://github.com/xucongTHU/ad_data_closed_loop/commit/c0022f6f72901c26566c43c7d46b8dc6bc98fe9e))

---

## [0.1.0] - 2025-12-05

### Features

* **architecture:** add submodule and update project structure ([1fcdc5b](https://github.com/xucongTHU/ad_data_closed_loop/commit/1fcdc5b196d0f29e2d2fc882295e80444cd2c7b9))
* **build:** add yaml-cpp dependency management and build support ([7193f57](https://github.com/xucongTHU/ad_data_closed_loop/commit/7193f5780e4c708ab2850c00bd5d887728cc906c))
* **data-processor:** remove costmap_builder and feature_alignment modules ([b831e7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/b831e7a5a0e0ce4b6a9cfd64801849120ddd68b7))
* **logger:** migrate logging macros to AD_* format ([25e2577](https://github.com/xucongTHU/ad_data_closed_loop/commit/25e257759fdf8a468e324d3b53d65a509d3985db))
* **navigation_planner:** add vehicle–edge PPO inference pipeline with ONNX Runtime integration ([2f9d363](https://github.com/xucongTHU/ad_data_closed_loop/commit/2f9d36310384afe940b9c62f4a89c267e8136637))
* **navigation-planner:** add PPO-based path planning support ([b26ba05](https://github.com/xucongTHU/ad_data_closed_loop/commit/b26ba053de393ecf279dd9473015648d77be516b))
* **planner:** implement navigation planning core, RL route optimization, and planning utils ([5ff2fcb](https://github.com/xucongTHU/ad_data_closed_loop/commit/5ff2fcb82d8bf499bb3c26bcb35a1ed6d7f99f51))
* **scenario:** add implementation details for turn detection and standstill scenarios ([bd28ff4](https://github.com/xucongTHU/ad_data_closed_loop/commit/bd28ff4668c9a892df4cc55bf3540dcbdf9334be))
* **scenario:** implement efficiency lane change and update scenario triggers ([51fbe0a](https://github.com/xucongTHU/ad_data_closed_loop/commit/51fbe0a6817a0410b86d8250faeccfa99189fc38))
* **signal_smoother:** implement signal smoothing with EMA and SMA options ([d5787cd](https://github.com/xucongTHU/ad_data_closed_loop/commit/d5787cdd9e5feb77cd7e5e819c281f7ddff2776f))
* **state-machine:** integrate state machine for data collection and navigation coordination ([c09fb7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/c09fb7a89f962769ae068ffd8335eda7fdcdd90a))
* **training:** implement advanced PPO training with configurable network and environment ([864865d](https://github.com/xucongTHU/ad_data_closed_loop/commit/864865d2052ee08edd0ec5cbad52d27e9910d43c))
* **utils:** add microsecond timestamp utility function ([86f1030](https://github.com/xucongTHU/ad_data_closed_loop/commit/86f1030199780ce85d6ab834e18e3d9f96aac3ea))

---

## [0.1.0-rc.2] - 2025-12-04

### Features

* **architecture:** add submodule and update project structure ([1fcdc5b](https://github.com/xucongTHU/ad_data_closed_loop/commit/1fcdc5b196d0f29e2d2fc882295e80444cd2c7b9))
* **data-processor:** remove costmap_builder and feature_alignment modules ([b831e7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/b831e7a5a0e0ce4b6a9cfd64801849120ddd68b7))
* **logger:** migrate logging macros to AD_* format ([25e2577](https://github.com/xucongTHU/ad_data_closed_loop/commit/25e257759fdf8a468e324d3b53d65a509d3985db))
* **navigation-planner:** add PPO-based path planning support ([b26ba05](https://github.com/xucongTHU/ad_data_closed_loop/commit/b26ba053de393ecf279dd9473015648d77be516b))
* **planner:** implement navigation planning core, RL route optimization, and planning utils ([5ff2fcb](https://github.com/xucongTHU/ad_data_closed_loop/commit/5ff2fcb82d8bf499bb3c26bcb35a1ed6d7f99f51))
* **scenario:** add implementation details for turn detection and standstill scenarios ([bd28ff4](https://github.com/xucongTHU/ad_data_closed_loop/commit/bd28ff4668c9a892df4cc55bf3540dcbdf9334be))
* **state-machine:** integrate state machine for data collection and navigation coordination ([c09fb7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/c09fb7a89f962769ae068ffd8335eda7fdcdd90a))
* **training:** implement advanced PPO training with configurable network and environment ([864865d](https://github.com/xucongTHU/ad_data_closed_loop/commit/864865d2052ee08edd0ec5cbad52d27e9910d43c))
* **utils:** add microsecond timestamp utility function ([86f1030](https://github.com/xucongTHU/ad_data_closed_loop/commit/86f1030199780ce85d6ab834e18e3d9f96aac3ea))

---

## [0.1.0-rc.1] - 2025-12-03

### Features

* **architecture:** add submodule and update project structure ([1fcdc5b](https://github.com/xucongTHU/ad_data_closed_loop/commit/1fcdc5b196d0f29e2d2fc882295e80444cd2c7b9))
* **data-processor:** remove costmap_builder and feature_alignment modules ([b831e7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/b831e7a5a0e0ce4b6a9cfd64801849120ddd68b7))
* **logger:** migrate logging macros to AD_* format ([25e2577](https://github.com/xucongTHU/ad_data_closed_loop/commit/25e257759fdf8a468e324d3b53d65a509d3985db))
* **navigation-planner:** add PPO-based path planning support ([b26ba05](https://github.com/xucongTHU/ad_data_closed_loop/commit/b26ba053de393ecf279dd9473015648d77be516b))
* **planner:** implement navigation planning core, RL route optimization, and planning utils ([5ff2fcb](https://github.com/xucongTHU/ad_data_closed_loop/commit/5ff2fcb82d8bf499bb3c26bcb35a1ed6d7f99f51))
* **state-machine:** integrate state machine for data collection and navigation coordination ([c09fb7a](https://github.com/xucongTHU/ad_data_closed_loop/commit/c09fb7a89f962769ae068ffd8335eda7fdcdd90a))
* **training:** implement advanced PPO training with configurable network and environment ([864865d](https://github.com/xucongTHU/ad_data_closed_loop/commit/864865d2052ee08edd0ec5cbad52d27e9910d43c))
* **utils:** add microsecond timestamp utility function ([86f1030](https://github.com/xucongTHU/ad_data_closed_loop/commit/86f1030199780ce85d6ab834e18e3d9f96aac3ea))

---

## [0.1.0-alpha] - 2025-12-02

### Refactor

#### refactor(trigger): restructure trigger engine components
- Improve `TriggerManager` with thread-safe accessors and scheduler integration
- Migrate variable getters in `TriggerManager` to use `MessageProvider`

---

## (2025-12-01)

### Features

#### feat(logger): migrate logging macros to AD_* format
- Replace `LOG_*` macros with `AD_*` macros across multiple modules
- Update logger calls in `ChannelManager`, `MessageProvider`, `DataStorage`,
  `RsclRecorder`, and `StateMachine` components
- Change log levels and formatting for consistency

#### feat(utils): add microsecond timestamp utility function
- Implement `GetCurrentTimestampUs()` in `utils.cpp`
- Add function declaration in `utils.h`
- Provide helper for microsecond precision time measurement

#### feat(state-machine): integrate state machine for data collection and navigation coordination
- Added `state_machine` module with 8 states and 12 transition events
- Integrated `DataCollectionPlanner`, `NavPlannerNode`, and `DataStorage` components
- Updated `main.cpp` to use state machine driven workflow
- Enhanced logging with `AD_INFO/AD_ERROR` macros
- Extended `CMakeLists.txt` to include new source files
- Improved data collection execution with real module integration
- Added upload functionality and coverage reporting
- Refactored namespaces and include paths for better modularity

---

## [0.0.3] - 2025-11-07

### Chore

#### chore(vscode): update VS Code settings for better C++ development experience
- Added file associations for C++ standard library headers
- Configured C++ IntelliSense, formatting, and inlay hints settings
- Updated `cmake.sourceDirectory` path for correct project root detection

---

## [0.0.2] - 2025-11-05

### Docs / Architecture

#### docs(architecture): update project structure and documentation files

#### feat(data-processor): remove costmap_builder and feature_alignment modules
- Removed `costmap_builder.py` and `feature_alignment.py` modules
- Deprecated:
  - Costmap generation and data density analysis
  - Cross-modality feature alignment and ICP-based registration
  - Visualization and configuration utilities tied to these modules
- Simplifies data processing pipeline, removing dependencies on
  `scipy`, `matplotlib`, and `opencv-python`

#### refactor(submodule): relocate data_collection from infra → src
- Moved `infra/data_collection` to `src/data_collection` for clearer project hierarchy
- Aligns submodule structure with standard source tree conventions
- Updated CMake and dependency paths accordingly

---

## [0.0.1] - 2025-11-04

### Features

#### feat(planner): implement navigation planning core, RL route optimization, and planning utils
- Add `NavPlannerNode`: costmap integration, route planner, sampling optimizer,
  semantic map & constraint checking, dynamic config reload, data-point collection
  and statistics
- Implement planner weights loading from YAML and document planner configuration parameters
- Add path planning & validation features including coverage metrics,
  path resampling, smoothing, length calculation, pose interpolation and angle normalization
- Integrate RL support: `RoutePlanner` (A* placeholder), sampling-based cost adjustments
  using data-density stats, `RewardCalculator` with shaped rewards, exploration bonus,
  and redundancy penalty
- Provide comprehensive planning utilities: coordinate transforms,
  distance functions, YAML param load/save, file I/O, and configurable logging
- Chore: update docs and README — fix `ad_shadowmode` URL, document new costmap/semantics/RL
  file structure, and update data_processor docs to include new components

---

## [0.0.0] - 2025-10-23

### Docs / Architecture

#### docs: add Synaptix AI Proprietary License

#### feat(architecture): add submodule and update project structure
- Add `infra/data_collection` submodule from external repository
- Add navigation planner and data processing modules structure
- Add config, docker, docs, infra, training, ops, tests, tools component directories
- Update `README.md` with detailed project architecture diagram

---

## [0.0.0] - 2025-10-21

### Docs

#### docs: add ad data_closed_loop new features documents

---

## [0.0.0] - 2025-10-17

### Initial Commit

---
