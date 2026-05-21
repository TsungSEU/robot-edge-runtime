# AGENTS.md

本文件适用于本仓库根目录及其所有子目录。后续 agent 在本仓库内修改代码时，除非用户或更高优先级指令明确覆盖，必须遵守以下约定。

## 背景：本轮审查问题与实施结论

本仓库的数据采集合规链路曾被审查指出：不要继续只增强 legacy `ImageDesensitizer` 与 `GeospatialObfuscator`，而应迁移到 **Compliance Pipeline V2**。核心目标如下：

- 图像隐私：从“全帧 blur”升级为“ROI 检测 + 不可逆遮挡 + fallback 策略”。
- 空间隐私：从“固定 x/y 偏移”升级为“多消息一致空间隐私变换 + GPS 分级脱敏 + manifest 可审计”。
- 运行时行为：从“失败后 raw forward”升级为“策略驱动、默认 fail-closed、可审计”。
- Manifest：从只记录开关升级为记录 policy、topic/frame 级统计、失败动作、raw-forward 计数，并对 VIN/device_id 做 token/hash。

当前实现已新增 `src/data_collection/compliance_v2/`，并通过 `src/data_collection/compliance/compliance_filter.*` 优先委托到 V2。Legacy 模块只能作为兼容 fallback，不应作为新功能的主要扩展点。

## Compliance Pipeline V2 模块边界

新增或修改合规能力时优先在以下目录内演进：

```text
src/data_collection/compliance_v2/
├── policy/    # policy model、parser、topic registry
├── visual/    # visual processor、PII detector、ROI redactor、codec/fallback policy
├── spatial/   # local frame transform、GPS privacy、pointcloud/tf/pose/path transform
├── runtime/   # compliance filter v2、decision、metrics、worker pool
└── manifest/  # privacy manifest v2、audit writer
```

### policy 层

- 不要新增基于 topic substring 的核心合规判断；应通过 topic + message_type + policy 精确匹配。
- 新策略字段应优先加到 `compliance_v2/policy/compliance_policy.h`，并在 `compliance_policy_parser.cpp` 中从 strategy/config 兼容解析。
- 默认高敏 topic 必须 `FailMode::FailClosed`。
- 只有显式配置 `FailMode::RawForwardExplicit` 且 `raw_forward_allowed=true` 时，才允许 raw forward。

### visual 层

- 不要恢复或新增可逆/弱隐私的全帧 blur 作为主策略。
- 主路径应是：decode/normalize -> PII detector -> ROI redaction -> encode/serialize。
- ROI redaction 应使用不可逆方法，例如 solid fill、不可逆 mosaic、random block scrambling，或经审查接受的 semantic mask/inpaint。
- Detector 不可用时必须走 policy fallback；当前安全 fallback 是 full-frame irreversible mosaic。
- Processing error、unsupported compressed/raw encoding、overload 等情况不得默认 raw forward，必须按 policy drop/quarantine/sample/block。
- 若实现 `sensor_msgs/msg/CompressedImage` 支持，必须明确 decode/encode 错误处理与 fail-closed 行为。
- 对 depth/mono/16-bit/big-endian 图像修改时，要保留或补充对应测试。

### spatial 层

- 本地空间 topic 必须使用同一 session-scoped transform，保持 Odometry、PoseStamped、Path、TFMessage、PointCloud2、Marker 等数据之间的一致性。
- 不要在日志或 manifest 中输出真实 transform 参数、offset、session seed、可逆 key 等敏感值。
- 对 GPS/NavSatFix/经纬度 metadata 不要只做固定偏移；应使用 coarse tile/geohash/generalization + 可选 geo-indistinguishability noise，并删除或降级 altitude/covariance 等高精度字段。
- PointCloud2 改动应保证字段解析安全，无法确认 x/y/z 字段或 endianness 时按 policy fail-closed。
- 如果引入 SE(3) 或尺度扰动，必须说明对运动学/训练数据可用性的影响；默认不要破坏相对运动尺度。

### runtime 层

- `ComplianceFilterV2` 是新链路入口。新处理器应返回 `ComplianceDecision`，由 runtime 统一决定 forward/drop/quarantine/raw-forward。
- 不要在 processor 内偷偷转发 raw；processor 只应产出 decision。
- 必须维护 metrics：seen、sanitized/redacted、dropped、quarantined、raw_forwarded_count、error reason、detected classes。
- 异步/worker pool 优化必须使用 bounded queue，并定义队列满时的 policy 行为。
- ROS callback 上不应做无界阻塞；重处理逻辑应可观测、可限流、可 fail-closed。

### manifest/audit 层

- Manifest 必须保持可审计：记录 policy_version、policy_hash、fail_mode、visual/spatial topic 统计、redaction method、GPS policy、raw_coordinates_removed、errors、raw_forwarded_count。
- 不要把 VIN、device_id、精确 GPS、空间 transform 参数或可逆 key 明文写入 manifest、日志、audit payload。
- device identity 应使用 token/hash；如引入 salt/key，必须记录 key id/policy id，而不是 key 本身。
- manifest schema 变更时，保留兼容字段或明确 bump `manifest_version`，并更新生成逻辑和测试。

## Legacy 模块使用规则

- `src/data_collection/compliance/image_desensitizer.*` 仅可作为 V2 fallback 或兼容层维护，不应继续扩展为主图像隐私架构。
- `src/data_collection/compliance/geospatial_obfuscator.*` 仅可作为 legacy fallback；新空间类型支持应放在 `compliance_v2/spatial/`。
- 若必须修改 legacy 模块，说明为何不能在 V2 实现，并确保 legacy 失败路径仍不 raw-forward 高敏数据。

## 数据采集并发与生命周期规则

- 修改 observer、ring buffer、recorder、executor、uploader、state machine 时，必须保持已有线程安全语义：snapshot 后迭代、atomic 状态、受保护的 start/stop/join、reload/record worker drain。
- 不要在持锁状态下调用可能回调外部组件或执行长耗时 I/O 的逻辑。
- 后台录制流程必须保持：record -> manifest -> compress/upload 的顺序和可失败日志。
- Reload、trigger、recording、compression、upload 状态之间新增交互时，必须考虑竞态和重复触发。

## 测试与验证要求

修改合规链路时，至少考虑以下验证：

- CMake configure/build：`cmake -S . -B build && cmake --build build -j`
- 测试套件：`ctest --test-dir build --output-on-failure`
- 静态检查：`git diff --check`
- Visual golden cases：face、license plate、screen text、QR/barcode、no PII、detector failure、compressed image failure、depth/16-bit image。
- Spatial cases：Odometry/Pose/Path/TF/PointCloud 同一 session transform、一致性检查、NavSatFix generalization、无 transform 参数泄露。
- Manifest cases：VIN/device_id tokenization、raw_forwarded_count、errors、topic counters、policy hash/version、GPS policy 字段。

如果当前容器缺少依赖导致 build/test 无法运行，必须在最终回复中明确说明缺失依赖和失败阶段，不要声称通过。

## 代码风格与审查说明

- 代码应保持 C++17 兼容。
- 不要在 import/include 周围加 try/catch。
- 新增隐私策略或 fallback 行为时，优先写清楚“失败时如何处理 raw 数据”。
- PR 描述中应明确映射审查问题：图像、地信、fail mode、manifest、identity tokenization、性能/worker pool（如涉及）。
- 对尚未完全实现的 V2 能力（例如真实 PII detector、GPU/TensorRT/ONNX detector、完整 compressed image codec）不要夸大；应标记为接口/骨架/fallback，并说明当前 fail-closed 行为。
