# 问题排查指南

本文档提供 Aurora-Edge-Runtime 常见问题的诊断和解决方案。

## 目录

1. [启动问题](#1-启动问题)
2. [运行时问题](#2-运行时问题)
3. [采集问题](#3-采集问题)
4. [上传问题](#4-上传问题)
5. [性能问题](#5-性能问题)
6. [诊断工具](#6-诊断工具)

---

## 1. 启动问题

### 1.1 DCP 无法启动

**症状**：
```
./build/src/dcp
# Error: Failed to initialize ROS2
```

**诊断步骤**：
```bash
# 1. 检查 ROS2 环境
echo $ROS_DISTRO  # 应该输出 "humble"
source /opt/ros/humble/setup.bash

# 2. 检查 RMW 实现
echo $RMW_IMPLEMENTATION  # 应该输出 "rmw_fastrtps_cpp"
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

# 3. 检查 ROS2 节点
ros2 node list
```

**解决方案**：
```bash
# 在启动脚本中添加
source /opt/ros/humble/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

### 1.2 模型加载失败

**症状**：
```
[ERROR] [dcp] Failed to load ONNX model: models/humanoid.onnx
```

**诊断步骤**：
```bash
# 1. 检查模型文件
ls -lh models/humanoid_ppo.onnx

# 2. 验证模型格式
file models/humanoid_ppo.onnx  # 应该显示 "data"

# 3. 检查 ONNX Runtime
ldd ./build/src/dcp | grep onnxruntime
```

**解决方案**：
```bash
# 下载预训练模型
wget https://github.com/your-org/releases/download/v1.1.2/humanoid_ppo.onnx -P models/

# 或创建软链接
ln -s models/auto_ppo.onnx models/humanoid.onnx
```

### 1.3 配置文件错误

**症状**：
```
[ERROR] [dcp] Failed to parse config file
```

**诊断步骤**：
```bash
# 验证 YAML 语法
python3 -c "import yaml; yaml.safe_load(open('config/planner_weights.yaml'))"

# 验证 JSON 语法
python3 -c "import json; json.load(open('config/app_config.json'))"
```

**解决方案**：
- 检查缩进（YAML 使用空格，不能用 Tab）
- 检查引号匹配
- 检查特殊字符转义

---

## 2. 运行时问题

### 2.1 无法订阅话题

**症状**：
```
[WARN] [dcp] Failed to subscribe to /robot/odom
```

**诊断步骤**：
```bash
# 1. 检查话题是否存在
ros2 topic list | grep robot

# 2. 检查话题类型
ros2 topic info /robot/odom

# 3. 检查发布频率
ros2 topic hz /robot/odom
```

**解决方案**：
```bash
# 确保机器人或仿真器正在运行
ros2 launch aurora_edge_runtime robot_simulator.launch.py
```

### 2.2 推理超时

**症状**：
```
[ERROR] [planner] Inference timeout (>20ms)
```

**诊断步骤**：
```bash
# 1. 检查 CPU 使用率
top -p $(pgrep dcp)

# 2. 检查 CPU 频率
cpupower frequency-info

# 3. 检查系统负载
uptime
```

**解决方案**：
```yaml
# 编辑配置
vim config/planner_weights.yaml

# 减小输入维度
# 或启用量化模型
inference:
  num_inference_threads: 2  # 增加线程数
```

### 2.3 内存占用过高

**症状**：
```
# 内存使用持续增长
top -p $(pgrep dcp)
# PID  %MEM
# 1234 45.2
# 1234 52.1
```

**诊断步骤**：
```bash
# 使用 valgrind 检测
valgrind --leak-check=full ./build/src/dcp

# 检查 Rosbag2 缓存
du -sh /data/dcp/bags/*
```

**解决方案**：
```bash
# 1. 重启服务
sudo systemctl restart dcp

# 2. 减小环形缓冲
vim config/robot_data_collection.json
# 减小 "forwardCaptureDurationSec"

# 3. 清理旧数据
find /data/dcp/bags -mtime +7 -delete
```

---

## 3. 采集问题

### 3.1 没有采集触发

**症状**：
```
# 运行一段时间后，没有采集触发
```

**诊断步骤**：
```bash
# 1. 检查触发器日志
grep "GaitTrigger" /var/log/dcp/dcp.log | tail -20

# 2. 检查话题数据
ros2 topic echo /robot/odom --once

# 3. 检查触发条件
grep "shouldTrigger" /var/log/dcp/dcp.log | tail -20
```

**解决方案**：
```bash
# 检查步态参数
grep "min_step_distance" config/planner_weights.yaml
grep "min_collection_interval" config/planner_weights.yaml

# 降低触发阈值（临时测试）
# min_step_distance: 0.10  # 降低到 10cm
# min_collection_interval: 0.5  # 降低到 0.5s
```

### 3.2 触发过于频繁

**症状**：
```
# 触发频率过高，产生大量冗余数据
```

**解决方案**：
```yaml
# 提高触发阈值
min_step_distance: 0.20      # 提高到 20cm
min_collection_interval: 2.0  # 提高到 2s
stable_phase_threshold: 0.4   # 提高稳定相位阈值
```

### 3.3 Rosbag 保存失败

**症状**：
```
[ERROR] [executor] Failed to save rosbag
```

**诊断步骤**：
```bash
# 1. 检查磁盘空间
df -h /data/dcp

# 2. 检查目录权限
ls -ld /data/dcp/bags

# 3. 检查 Rosbag2 存储
ros2 bag record /robot/odom -o /tmp/test
```

**解决方案**：
```bash
# 清理旧数据
find /data/dcp/bags -mtime +7 -delete

# 创建目录
mkdir -p /data/dcp/bags
chmod 755 /data/dcp/bags
```

---

## 4. 上传问题

### 4.1 上传失败

**症状**：
```
[ERROR] [uploader] Failed to upload: Connection timeout
```

**诊断步骤**：
```bash
# 1. 检查网络连接
ping caic-obs.t3caic.com

# 2. 测试 S3 连接
aws s3 ls s3://caic-dataset --endpoint-url https://caic-obs.t3caic.com

# 3. 检查凭证
cat config/app_config.json | grep -A 10 "aws"
```

**解决方案**：
```bash
# 1. 检查并更新配置
vim config/app_config.json

# 2. 测试上传
aws s3 cp /tmp/test.txt s3://caic-dataset/test/ \
  --endpoint-url https://caic-obs.t3caic.com

# 3. 如果证书过期，更新证书
./ops/update_cert.sh
```

### 4.2 上传速度慢

**症状**：
```
# 上传速度 < 100KB/s
```

**诊断步骤**：
```bash
# 1. 测试网络速度
speedtest-cli

# 2. 检查分片大小
grep "uploadFileSliceSizeMb" config/app_config.json

# 3. 检查并发数
grep "uploadFileSliceIntervalMs" config/app_config.json
```

**解决方案**：
```json
{
  "dataUpload": {
    "uploadFileSliceSizeMb": 10,      // 减小分片
    "uploadFileSliceIntervalMs": 50,  // 减少间隔
    "retryCount": 5,                   // 增加重试
    "retryIntervalSec": 5              // 减少重试间隔
  }
}
```

---

## 5. 性能问题

### 5.1 CPU 使用率过高

**症状**：
```
# CPU 使用率 > 50%
```

**诊断步骤**：
```bash
# 1. 查看 CPU 使用
top -p $(pgrep dcp) -H

# 2. 检查控制频率
grep "frequency" config/planner_weights.yaml

# 3. 检查话题数量
ros2 topic list | wc -l
```

**解决方案**：
```bash
# 1. CPU 绑核
vim ops/dcp.conf
# DCP_CPU_AFFINITY=2-3  # 限制到特定核心

# 2. 降低控制频率
# 修改代码中的控制频率

# 3. 减少订阅话题
# 编辑 config/robot_data_collection.json
```

### 5.2 内存泄漏

**症状**：
```
# 内存持续增长
watch -n 5 'ps aux | grep dcp'
```

**诊断步骤**：
```bash
# 使用 valgrind 检测
valgrind --leak-check=full --show-leak-kinds=all \
  ./build/src/dcp 2>&1 | tee valgrind.log

# 分析报告
grep "definitely lost" valgrind.log
```

**解决方案**：
- 代码中修复内存泄漏
- 定期重启服务（临时方案）
- 设置内存限制

---

## 6. 诊断工具

### 6.1 系统诊断脚本

```bash
#!/bin/bash
# ops/diagnose.sh

echo "=== DCP System Diagnostics ==="

# 1. 进程状态
if pgrep -x "dcp" > /dev/null; then
    echo "✓ DCP Process: Running (PID: $(pgrep -x dcp))"
else
    echo "✗ DCP Process: Not running"
fi

# 2. ROS2 节点
if ros2 node list 2>/dev/null | grep -q "/dcp"; then
    echo "✓ ROS2 Node: Active"
else
    echo "✗ ROS2 Node: Not found"
fi

# 3. 话题检查
TOPIC_COUNT=$(ros2 topic list 2>/dev/null | grep -c robot)
if [ $TOPIC_COUNT -ge 3 ]; then
    echo "✓ Topics: Publishing ($TOPIC_COUNT robot topics)"
else
    echo "✗ Topics: Insufficient ($TOPIC_COUNT robot topics)"
fi

# 4. 最近错误
ERROR_COUNT=$(grep -c "ERROR" /var/log/dcp/dcp.log 2>/dev/null || echo 0)
if [ $ERROR_COUNT -lt 10 ]; then
    echo "✓ Recent Errors: $ERROR_COUNT"
else
    echo "⚠ Recent Errors: $ERROR_COUNT (high)"
fi

echo "=== Diagnostics Complete ==="
```

### 6.2 日志分析工具

```bash
# 统计错误类型
grep ERROR /var/log/dcp/dcp.log | awk -F'[][]' '{print $4}' | sort | uniq -c

# 查看最近的错误
grep ERROR /var/log/dcp/dcp.log | tail -20

# 查看特定时间的日志
sed -n '/2026-03-07 14:3/,/2026-03-07 14:4/p' /var/log/dcp/dcp.log
```

### 6.3 性能分析

```bash
# 推理延迟统计
grep "Inference time" /var/log/dcp/dcp.log | \
  awk '{print $NF}' | \
  awk '{sum+=$1; count++} END {print "Avg:", sum/count, "ms"}'

# 采集统计
grep "Collection triggered" /var/log/dcp/dcp.log | wc -l
```

---

## 获取帮助

如果以上方法无法解决问题：

1. 📖 查看 [完整文档](../README.md)
2. 🔍 [搜索 Issue](https://github.com/your-org/aurora-edge-runtime/issues)
3. 💬 加入 [Discussions](https://github.com/your-org/aurora-edge-runtime/discussions)
4. 📧 发送邮件到 support@example.com（附上诊断信息）
