**Breadcrumbs:** [Docs](../README.md) / [Operations](index.md) / Index

# 运维手册

本章节面向运维工程师和系统管理员，提供 Aurora-Edge-Runtime (AER) 的部署、监控和维护指南。

## 目录

### 部署指南

- [开发环境部署](deployment/development.md) - 开发环境快速部署
- [生产环境部署](deployment/production.md) - 生产环境部署
- [Docker部署](deployment/docker.md) - 使用 Docker 容器部署
- [边缘设备部署](deployment/edge-device.md) - 边缘设备部署

### 监控指南

- [监控指标](metrics.md) - 系统监控指标说明
- [日志管理](logging.md) - 日志收集与分析
- [告警配置](alerts.md) - 告警规则配置
- [监控面板](dashboard.md) - Grafana 面板配置

### 维护指南

- [备份恢复](backup.md) - 数据备份与恢复
- [升级指南](upgrade.md) - 版本升级流程
- [灾难恢复](disaster-recovery.md) - 灾难恢复预案

### 安全指南

- [认证配置](security/authentication.md) - 身份认证配置
- [加密配置](security/encryption.md) - 数据加密设置
- [访问控制](security/access-control.md) - 访问权限管理

## 快速开始

### 开发环境部署

使用 `aer` 命令进行快速开发和测试：

```bash
# 1. 安装 aer 命令
cd ops/dev
./install-aer.sh

# 2. 启动 AER 服务
aer start

# 3. 查看状态
aer status

# 4. 查看日志
aer logs aer

# 5. 停止服务
aer stop
```

### 生产环境部署

使用 systemd 服务进行生产部署：

```bash
# 1. 安装服务
cd ops/prod
sudo ./install-aer-service.sh install

# 2. 启动服务
sudo systemctl start aer

# 3. 设置开机自启
sudo systemctl enable aer

# 4. 查看状态
sudo systemctl status aer

# 5. 查看日志
sudo journalctl -u aer -f
```

## 环境对比

| 特性 | 开发环境 | 生产环境 |
|------|---------|---------|
| **目录** | `ops/dev/` | `ops/prod/` |
| **命令** | `aer start` | `systemctl start aer` |
| **权限** | 用户 | root |
| **开机自启** | ❌ | ✅ |
| **自动重启** | ❌ | ✅ |
| **日志** | `/tmp/aer.log` | journald |
| **配置** | 命令行参数 | `ops/prod/aer.conf` |

## 监控概览

### 关键指标

| 类别 | 指标 | 正常范围 | 告警阈值 |
|-----|------|---------|---------|
| **性能** | 推理延迟 | <10ms | >20ms |
| **性能** | CPU使用率 | <20% | >80% |
| **性能** | 内存使用 | <150MB | >500MB |
| **业务** | 采集成功率 | >95% | <90% |
| **业务** | 上传成功率 | >99% | <95% |
| **系统** | 磁盘空间 | >10GB | <5GB |

### 服务状态检查

```bash
# 开发环境
aer status

# 生产环境
sudo systemctl status aer
```

## 日志管理

### 日志位置

**开发环境：**
```bash
/tmp/aer.log              # AER 服务日志
/tmp/robot_sim.log        # robot_sim 日志
```

**生产环境：**
```bash
# systemd journald
sudo journalctl -u aer -f

# 或配置文件指定的位置
/var/log/aer/aer.log      # 如果配置了文件日志
```

### 查看日志

```bash
# 开发环境 - 实时日志
aer logs aer

# 开发环境 - 查看文件
tail -f /tmp/aer.log

# 生产环境 - systemd 日志
sudo journalctl -u aer -f

# 生产环境 - 最近 100 行
sudo journalctl -u aer -n 100
```

## 配置管理

### 开发环境配置

使用命令行参数：

```bash
aer start --mode auto          # 自动驾驶模式
aer start --mode humanoid      # 人形机器人模式
AER_MODE=auto aer start        # 环境变量
```

### 生产环境配置

编辑配置文件：

```bash
# 编辑配置
vim ops/prod/aer.conf

# 重新加载配置
sudo systemctl reload aer
```

**主要配置项：**

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `AER_MODE` | 运行模式 | `humanoid` |
| `AER_MODEL_PATH` | ONNX 模型路径 | `models/humanoid_ppo.onnx` |
| `AER_CONFIG_PATH` | 配置文件路径 | `config/planner_weights.yaml` |
| `AER_LOG_LEVEL` | 日志级别 (0-4) | 2 (INFO) |
| `AER_CPU_AFFINITY` | CPU 绑核 | `0` |

## 备份策略

### 备份内容

- 配置文件 (`config/`, `ops/prod/aer.conf`)
- ONNX模型 (`models/`)
- 采集数据 (`data/bags/`)
- 上传记录 (`data/bags/upload_record.json`)

### 备份脚本

```bash
# 手动备份
tar -czf backup_$(date +%Y%m%d).tar.gz \
    config/ \
    models/ \
    ops/prod/aer.conf \
    data/bags/
```

## 升级流程

### 开发环境升级

```bash
# 1. 拉取最新代码
git pull origin main

# 2. 重新编译
cd build && cmake .. && make -j8

# 3. 停止服务
aer stop

# 4. 重新安装 aer 命令
cd ops/dev
./install-aer.sh

# 5. 启动服务
aer start
```

### 生产环境升级

```bash
# 1. 拉取最新代码
git pull origin main

# 2. 重新编译
cd build && cmake .. && make -j8

# 3. 停止服务
sudo systemctl stop aer

# 4. 重新安装服务
cd ops/prod
sudo ./install-aer-service.sh install

# 5. 启动服务
sudo systemctl start aer

# 6. 验证
sudo systemctl status aer
```

## 故障处理

### 常见问题

| 问题 | 症状 | 解决方案 |
|-----|------|---------|
| 服务无法启动 | `aer start` / `systemctl start` 失败 | 检查可执行文件 `build/src/aer` |
| 权限错误 | 无法创建 PID 文件 | 检查 `.pids/` 目录权限 |
| 推理超时 | 日志显示推理延迟>20ms | 检查ONNX模型、CPU资源 |
| 上传失败 | 日志显示上传错误 | 检查网络、S3凭证 |
| 内存泄漏 | 内存持续增长 | 重启服务、收集诊断信息 |

### 开发环境故障排除

```bash
# 检查状态
aer status

# 查看详细日志
aer logs aer

# 清理并重启
rm -f .pids/*.pid
cd ops/dev && ./cleanup.sh
aer start
```

### 生产环境故障排除

```bash
# 检查服务状态
sudo systemctl status aer

# 查看日志
sudo journalctl -u aer -n 100

# 重启服务
sudo systemctl restart aer

# 检查配置
sudo systemd-analyze verify ops/prod/aer.service
```

## robot_sim 管理

**重要说明：** robot_sim **不由服务管理**，需要手动启动：

```bash
# 开发环境：手动启动
./build/src/robot_sim

# 查看日志
tail -f /tmp/robot_sim.log

# 停止
pkill robot_sim
```

生产环境中，robot_sim 应该：
- 由真实机器人硬件替代
- 或在独立的设备上运行
- 或由其他专用服务管理

## 相关文档

- `ops/README.md` - Ops 目录详细说明
- `ops/QUICKREF.md` - 快速参考
- `docs/AER_QUICKSTART.md` - AER 快速入门
- `docs/AER_COMMAND.md` - 命令参考
- `docs/SERVICE_ARCHITECTURE.md` - 服务架构

## 应急联系

- 运维值班: oncall@example.com
- 紧急电话: +86-xxx-xxxx-xxxx
