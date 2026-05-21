**Breadcrumbs:** [Docs](../README.md) / [Faq](index.md) / Index

# 常见问题 (FAQ)

这里是 Aurora-Edge-Runtime 常见问题的解答集合。

## 快速查找

### 分类索引

- [通用问题](#通用问题)
- [安装问题](#安装问题)
- [配置问题](#配置问题)
- [运行时问题](#运行时问题)
- [性能问题](#性能问题)

---

## 通用问题

### Q: Aurora-Edge-Runtime 是什么？

A: Aurora-Edge-Runtime 是 Aurora 产品矩阵中专注于机器人端侧智能数据采集的核心运行时系统。它通过强化学习驱动的智能规划和精准触发机制，实现高价值训练数据的自动化采集。

详见：[产品规格](../product/specification.md)

### Q: 支持哪些机器人平台？

A: 目前支持两大类平台：
- **Auto 模式**: 自动驾驶车辆、轮式机器人
- **Humanoid 模式**: 人形机器人、双足机器人

详见：[运行模式](../user-guide/modes/)

### Q: 系统有什么硬件要求？

A: 最低配置：
- CPU: 4核 ARM64/x86_64 @ 1.5GHz
- 内存: 4GB
- 存储: 50GB 可用空间

推荐配置：
- CPU: 8核 ARM64/x86_64 @ 2.0GHz
- 内存: 8GB
- 存储: 200GB SSD

详见：[硬件要求](../hardware/requirements/minimum.md)

### Q: 是开源的吗？

A: 是的，项目采用 Apache 2.0 许可证。源码：[GitHub](https://github.com/your-org/aurora-edge-runtime)

---

## 安装问题

### Q: 如何安装 Aurora-Edge-Runtime？

A: 提供两种安装方式：

**方式一: Docker（推荐）**
```bash
docker-compose up -d
```

**方式二: 源码编译**
```bash
mkdir build && cd build
cmake .. && make -j$(nproc)
```

详见：[安装指南](../getting-started/index.md)

### Q: 编译失败怎么办？

A: 常见原因和解决方案：

1. **依赖缺失**: 运行 `sudo apt install ros-humble-desktop`
2. **CMake版本低**: 升级到 3.22+
3. **Boost版本不兼容**: 安装 Boost 1.74+

详见：[编译指南](../developer-guide/setup/building.md)

### Q: ROS2 版本兼容性？

A: 目前仅支持 ROS2 Humble。其他版本（Foxy、Galactic、Iron）暂不支持。

---

## 配置问题

### Q: 如何切换运行模式？

A: 编辑 `ops/dcp.conf`:

```bash
DCP_MODE=auto        # 自动驾驶模式
DCP_MODE=humanoid    # 人形机器人模式
```

或使用环境变量：
```bash
export DCP_MODE=humanoid
./build/src/dcp
```

详见：[配置指南](../user-guide/configuration/planner-config.md)

### Q: 如何调整采集参数？

A: 编辑 `config/robot_data_collection.json`:

```json
{
  "mode": {
    "cacheMode": {
      "forwardCaptureDurationSec": 15,   // 前向录制
      "backwardCaptureDurationSec": 5,   // 后向录制
      "cooldownDurationSec": 10           // 冷却时间
    }
  }
}
```

详见：[采集配置](../user-guide/configuration/collection-config.md)

### Q: 配置文件支持热更新吗？

A: 是的。修改以下文件后，系统会自动检测并重新加载：
- `config/planner_weights.yaml`
- `config/robot_data_collection.json`

无需重启服务。

---

## 运行时问题

### Q: DCP 无法启动？

A: 检查以下几点：

1. **ROS2 环境**: `source /opt/ros/humble/setup.bash`
2. **RMW 实现**: `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp`
3. **模型文件**: 确保 `models/humanoid_ppo.onnx` 存在
4. **端口占用**: 检查 8080 端口是否被占用

详见：[问题排查](../user-guide/operation/troubleshooting.md)

### Q: 没有采集数据？

A: 可能原因：

1. **触发条件未满足**: 检查步态触发条件
2. **话题未发布**: `ros2 topic list | grep robot`
3. **冷却期未结束**: 等待冷却时间结束

### Q: 上传失败？

A: 检查网络和配置：

1. **网络连接**: `ping orderseek-obs.orderseek.ai`
2. **凭证配置**: 检查 `config/app_config.json` 中的 AWS 配置
3. **存储空间**: 确保本地有足够空间

---

## 性能问题

### Q: 推理延迟过高？

A: 正常延迟 <10ms。如果过高，检查：

1. **CPU 频率**: `cpupower frequency-info`
2. **CPU 绑核**: 配置 `DCP_CPU_AFFINITY`
3. **模型大小**: 确认使用优化后的模型

### Q: 内存占用过大？

A: 正常占用 <150MB。如果过大，检查：

1. **内存泄漏**: 使用 `valgrind --leak-check=full`
2. **环形缓冲**: 减小 `forwardCaptureDurationSec`
3. **代价地图**: 清理历史数据

### Q: CPU 使用率高？

A: 正常使用率 <20%。如果过高，检查：

1. **控制频率**: 确认是 50Hz
2. **话题数量**: 减少不必要的订阅
3. **ONNX 线程**: 调整 `num_inference_threads`

---

## 仍有问题？

### 获取帮助

1. 📖 查看 [完整文档](../README.md)
2. 🔍 [搜索 Issue](https://github.com/your-org/aurora-edge-runtime/issues)
3. 💬 加入 [Discussions](https://github.com/your-org/aurora-edge-runtime/discussions)
4. 📧 发送邮件到 support@example.com

### 提交问题

提交问题时，请包含：

- Aurora-Edge-Runtime 版本
- 操作系统版本
- 错误日志
- 复现步骤

```bash
# 获取系统信息
./ops/system_info.sh > bug_report.txt
```
