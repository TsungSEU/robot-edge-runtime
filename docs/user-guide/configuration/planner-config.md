# 规划器配置指南

本文档详细说明 `config/planner_weights.yaml` 配置文件的各项参数。

## 目录

1. [配置文件结构](#1-配置文件结构)
2. [通用参数](#2-通用参数)
3. [Auto 模式配置](#3-auto-模式配置)
4. [Humanoid 模式配置](#4-humanoid-模式配置)
5. [参数调优指南](#5-参数调优指南)

---

## 1. 配置文件结构

`config/planner_weights.yaml` 是 Aurora-Edge-Runtime 的核心配置文件，定义了规划器的行为参数。

```yaml
# 规划器模式选择
planner_mode: "humanoid"  # auto | humanoid

# 通用参数
common:
  # 数据闭环参数
  sparse_threshold: 0.15
  exploration_bonus: 10.0
  redundancy_penalty: 5.0
  grid_resolution: 1.0

  # 数据价值权重
  w_spatial_rarity: 0.3
  w_temporal_freshness: 0.15
  w_scene_diversity: 0.2
  w_quality: 0.15
  w_coverage: 0.2

  # 场景稀有度映射
  scene_rarity:
    indoor_flat: 0.3
    indoor_stair: 0.7
    ...

# Auto 模式配置
auto:
  path_planning: {...}
  inference: {...}
  sampling: {...}
  reward: {...}

# Humanoid 模式配置
humanoid:
  state: {...}
  action: {...}
  inference: {...}
  reward: {...}
```

---

## 2. 通用参数

### 2.1 数据闭环参数

| 参数 | 类型 | 默认值 | 说明 |
|-----|------|-------|------|
| `sparse_threshold` | float | 0.15 | 稀疏区域阈值（密度低于此值视为稀疏） |
| `exploration_bonus` | float | 10.0 | 探索稀疏区域的奖励系数 |
| `redundancy_penalty` | float | 5.0 | 重复访问的惩罚系数 |
| `grid_resolution` | float | 1.0 | 代价地图网格分辨率（米） |

**调优建议**：
- 提高探索兴趣 → 增加 `exploration_bonus`
- 减少重复访问 → 增加 `redundancy_penalty`
- 更精细的代价地图 → 减小 `grid_resolution`

### 2.2 数据价值权重

| 参数 | 默认值 | 说明 |
|-----|-------|------|
| `w_spatial_rarity` | 0.3 | 空间稀缺性权重（30%） |
| `w_temporal_freshness` | 0.15 | 时间新鲜度权重（15%） |
| `w_scene_diversity` | 0.2 | 场景多样性权重（20%） |
| `w_quality` | 0.15 | 数据质量权重（15%） |
| `w_coverage` | 0.2 | 覆盖率权重（20%） |

**计算公式**：
```
ValueScore = 0.3 × SpatialRarity
           + 0.15 × TemporalFreshness
           + 0.2 × SceneDiversity
           + 0.15 × QualityScore
           + 0.2 × CoverageRate
```

### 2.3 场景稀有度映射

| 场景 | 默认值 | 说明 |
|-----|-------|------|
| `indoor_flat` | 0.3 | 室内平地（常见） |
| `indoor_stair` | 0.7 | 室内楼梯（稀有） |
| `indoor_ramp` | 0.6 | 室内坡道（较稀有） |
| `outdoor_flat` | 0.4 | 户外平地（较常见） |
| `outdoor_rough` | 0.8 | 户外崎岖（稀有） |
| `outdoor_slope` | 0.7 | 户外斜坡（较稀有） |
| `mixed` | 0.9 | 混合场景（最稀有） |

---

## 3. Auto 模式配置

### 3.1 路径规划参数

```yaml
auto:
  path_planning:
    max_search_iterations: 1000   # 最大搜索迭代次数
    min_path_length: 2.0          # 最小路径长度（米）
    path_smoothness_factor: 0.7   # 路径平滑系数 [0.0-1.0]
    heuristic_weight: 1.0         # A* 启发式权重
```

| 参数 | 说明 | 调优建议 |
|-----|------|---------|
| `max_search_iterations` | A* 最大迭代次数 | 复杂环境可增大 |
| `min_path_length` | 最小路径长度 | 避免过短路径 |
| `path_smoothness_factor` | 路径平滑度 | 越高越平滑，但计算量越大 |
| `heuristic_weight` | 启发式权重 | 影响搜索方向 |

### 3.2 推理参数

```yaml
auto:
  inference:
    action_scale: 1.0             # 动作缩放因子
    init_log_std: -0.5            # 初始 log 标准差
    min_log_std: -2.0             # 最小 log 标准差
    max_log_std: 0.5              # 最大 log 标准差
    num_inference_threads: 1      # 推理线程数
```

### 3.3 奖励参数

```yaml
auto:
  reward:
    distance_improvement_scale: 5.0    # 距离改进奖励系数
    step_penalty: -0.01                # 每步惩罚
    goal_reward: 50.0                  # 到达目标奖励
    collision_penalty: -50.0           # 碰撞惩罚
    new_sparse_reward: 10.0            # 新稀疏区域奖励
    new_area_reward: 2.0               # 新区域奖励
    inefficient_path_penalty: -5.0     # 低效路径惩罚
    repeat_visit_penalty: -2.0         # 重复访问惩罚
```

---

## 4. Humanoid 模式配置

### 4.1 状态空间配置

```yaml
humanoid:
  state:
    state_dim: 43                 # 43 维状态空间
    enable_gait_phase: true       # 启用步态相位
    enable_data_value: true       # 启用数据价值扇区
    enable_action_history: true   # 启用动作历史
```

**状态空间组成**（对齐训练侧 `humanoid_nav_data_training.yaml`）：
```
[0-2]    基座线速度 (vx, vy, vz)              ×2.0
[3-5]    基座角速度 (wx, wy, wz)              ×1.0
[6-7]    归一化位置 (x/W, y/H)
[8-9]    朝向 (sinθ, cosθ)
[10-11]  目标方向 (sinΔθ, cosΔθ)
[12-14]  目标距离 (Δx, Δy, ‖Δ‖)              ÷max_range
[15-22]  数据价值扇区 (8方向)                   [0,1]
[23-26]  障碍物扇区 (4方向)                    ÷max_range
[27-28]  当前位置价值 (value, rarity)           [0,1]
[29-30]  采集状态 (collected_ratio, coverage)  [0,1]
[31]     地形类型                              ÷6
[32]     障碍密度                              [0,1]
[33]     步态相位 sin(2π·φ)
[34-41]  动作历史 (最近8步 forward_vel)
[42]     剩余预算                              [0,1]
```

### 4.2 动作空间配置

```yaml
humanoid:
  action:
    action_dim: 3                 # 3 维连续速度命令
    action_clip: 18.0             # 动作裁剪系数
    forward_vel_range: [-0.3, 0.6]  # 前进速度 (m/s)
    lateral_vel_range: [-0.3, 0.3]  # 侧向速度 (m/s)
    angular_vel_range: [-0.3, 0.3]  # 角速度 (rad/s)
```

**动作空间组成**：
```
[0]  forward_vel  前进速度 [-0.3, 0.6] m/s
[1]  lateral_vel  侧向速度 [-0.3, 0.3] m/s
[2]  angular_vel  角速度   [-0.3, 0.3] rad/s
```

ONNX 模型输出 [-1, 1] 归一化值，通过反归一化映射到实际速度范围。

### 4.3 奖励函数配置

```yaml
humanoid:
  reward:
    # 主要权重
    w_navigation: 5.0             # 导航权重
    w_stability: 0.2              # 稳定性权重
    w_energy: 0.1                 # 能量权重
    w_safety: 0.3                 # 安全权重
    w_smoothness: 0.1             # 平滑性权重

    # 奖励值
    goal_reward: 1000.0           # 到达目标奖励
    progress_reward: 20.0         # 前进奖励
    new_sparse_reward: 1.0        # 稀疏区域奖励

    # 惩罚值
    fall_penalty: -20.0           # 跌倒惩罚
    collision_penalty: -5.0       # 碰撞惩罚
    time_penalty: -0.01           # 时间惩罚
```

---

## 5. 参数调优指南

### 5.1 调优流程

```
1. 确定调优目标
   └─ 提高覆盖率？提高速度？提高稳定性？

2. 识别关键参数
   └─ 根据目标选择相关参数

3. 小步调整
   └─ 每次调整 10-20%

4. A/B 对比测试
   └─ 记录指标变化

5. 验证并部署
   └─ 确认整体效果
```

### 5.2 场景化配置

#### 高效探索模式

```yaml
common:
  exploration_bonus: 20.0       # 提高探索奖励
  sparse_threshold: 0.2         # 提高稀疏阈值

humanoid:
  reward:
    w_navigation: 3.0           # 降低导航权重
    new_sparse_reward: 2.0      # 提高稀疏区域奖励
    repeat_visit_penalty: -5.0  # 提高重复访问惩罚
```

#### 快速导航模式

```yaml
humanoid:
  reward:
    w_navigation: 8.0           # 提高导航权重
    goal_reward: 2000.0         # 提高目标奖励
    progress_reward: 30.0       # 提高前进奖励
    time_penalty: -0.02         # 提高时间惩罚
```

#### 稳定采集模式

```yaml
humanoid:
  reward:
    w_stability: 0.5            # 提高稳定性权重
    w_safety: 0.5               # 提高安全权重
    w_smoothness: 0.2           # 提高平滑性权重
    fall_penalty: -50.0         # 提高跌倒惩罚
```

### 5.3 参数影响速查表

| 参数 | 增大效果 | 减小效果 |
|-----|---------|---------|
| `exploration_bonus` | 更多探索 | 更多利用 |
| `sparse_threshold` | 扩大稀疏定义 | 缩小稀疏定义 |
| `redundancy_penalty` | 减少重复 | 允许重复 |
| `w_navigation` | 更快到达目标 | 更慢更仔细 |
| `w_stability` | 更保守的运动 | 更激进的探索 |
| `goal_reward` | 更强的目标导向 | 更弱的目标导向 |

---

## 配置验证

### 验证语法

```bash
# 使用 Python 验证 YAML 语法
python3 -c "import yaml; yaml.safe_load(open('config/planner_weights.yaml'))"
```

### 验证参数范围

```bash
# 使用配置验证工具
./tools/validate_config.py config/planner_weights.yaml
```

---

## 参考文档

- [数据采集配置](collection-config.md)
- [应用配置](app-config.md)
- [参数调优](../operation/troubleshooting.md)
