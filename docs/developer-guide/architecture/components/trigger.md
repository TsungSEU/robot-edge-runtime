# 自动驾驶场景实现

本文档详细描述了自动驾驶系统中各类场景触发条件实现。

## 1. 变道场景 (Lane Change)

### 1.1 左绕障变道 (left_nudge)

在效率变道的基础上，自车轨迹终点处于当前自车车道，注意绕障不变更灯。
```cpp
// 左绕障效率变道触发器函数
bool checkLeftObstacleLaneChange(
    double v_ego,
    double d_front_left, double d_rear_left,
    double avg_speed_left_lane,
    double d_end, double lane_center_d, double lane_width,
    bool obstacle_detected,
    bool obstacle_requires_lane_change)
{
    // 1️⃣ efficiency_lane_change（左侧绕障版）
    bool safe_left_gap = (d_front_left > v_ego*1.5) && (d_rear_left > v_ego*1.5); // 安全距离阈值
    bool efficient_left = (avg_speed_left_lane > v_ego + 1.0);                     // 左车道更高效
    bool efficiency_lane_change = safe_left_gap && efficient_left;

    // 2️⃣ ego_trajectory_end_in_current_lane
    bool ego_trajectory_end_in_current_lane = (abs(d_end - lane_center_d) <= lane_width/2);

    // 3️⃣ changing_light_for_obstacle
    bool changing_light_for_obstacle = obstacle_detected && obstacle_requires_lane_change;

    // 4️⃣ 触发条件
    bool trigger = efficiency_lane_change
                 && ego_trajectory_end_in_current_lane
                 && !changing_light_for_obstacle;

    return trigger;
}

```

### 1.2 右绕障变道 (right_nudge)

与左绕障变道类似。
```cpp
// 右绕障效率变道触发器函数
bool checkRightObstacleLaneChange(
    double v_ego,
    double d_front_right, double d_rear_right,
    double avg_speed_right_lane,
    double d_end, double lane_center_d, double lane_width,
    bool obstacle_detected,
    bool obstacle_requires_lane_change)
{
    // 1️⃣ efficiency_lane_change（右侧绕障版）
    // 安全 gap：右前、右后距离满足阈值
    bool safe_right_gap = (d_front_right > v_ego*1.5) && (d_rear_right > v_ego*1.5); 
    // 效率判断：右车道速度更高
    bool efficient_right = (avg_speed_right_lane > v_ego + 1.0);                     
    bool efficiency_lane_change = safe_right_gap && efficient_right;

    // 2️⃣ ego_trajectory_end_in_current_lane
    // 保证绕障后轨迹终点仍在当前车道
    bool ego_trajectory_end_in_current_lane = (abs(d_end - lane_center_d) <= lane_width/2);

    // 3️⃣ changing_light_for_obstacle
    // 判断绕障是否需要开灯变道
    bool changing_light_for_obstacle = obstacle_detected && obstacle_requires_lane_change;

    // 4️⃣ 触发条件
    bool trigger = efficiency_lane_change
                 && ego_trajectory_end_in_current_lane
                 && !changing_light_for_obstacle;

    return trigger;
}

```

### 1.3 左变道

触发条件：
- 存在左车道线(line_start_x20)
- 左车道线是可跨越车道线(虚线)
- 存在变道点(固定距离轨迹和车道线交点)
- 变道点x位置小于轨迹的一半左右(防止变道太晚)
- 轨迹终点小于车道线终点(防止路口导航变道误判)
- 不能跨多个车道线
```cpp
// 左车道变道触发器函数
bool checkLeftLaneChange(
    bool has_left_lane_line,
    bool left_lane_line_is_dashed,
    const std::vector<Point>& lane_change_points, // 变道点列表
    double min_lane_change_x,
    double max_lane_change_x,
    double trajectory_end_d,      // 轨迹终点横向位置
    double lane_center_d,         // 左车道中心d
    double lane_width,            // 单车道宽度
    double trajectory_min_d,      // 轨迹最小d
    double trajectory_max_d       // 轨迹最大d
)
{
    // 1️⃣ 左车道线存在且是虚线
    bool lane_line_ok = has_left_lane_line && left_lane_line_is_dashed;

    // 2️⃣ 存在变道点
    bool has_lane_change_point = !lane_change_points.empty();

    // 3️⃣ 变道点 x 位置合适（取第一个点作为示例）
    bool lane_change_point_x_valid = false;
    if(has_lane_change_point)
    {
        double x = lane_change_points.front().x;
        lane_change_point_x_valid = (x >= min_lane_change_x) && (x <= max_lane_change_x);
    }

    // 4️⃣ 轨迹终点小于车道线终点
    bool trajectory_end_less_than_lane_end = (trajectory_end_d < lane_center_d + lane_width/2);

    // 5️⃣ 不跨越多车道
    double trajectory_lateral_span = trajectory_max_d - trajectory_min_d;
    bool not_crossing_multiple_lanes = (trajectory_lateral_span <= lane_width);

    // 6️⃣ 组合触发条件
    bool trigger = lane_line_ok
                 && has_lane_change_point
                 && lane_change_point_x_valid
                 && trajectory_end_less_than_lane_end
                 && not_crossing_multiple_lanes;

    return trigger;
}

```

### 1.4 右变道

触发条件：
- 存在右车道线(line_start_x20)
- 右车道线是可跨越车道线(虚线)
- 存在变道点(固定距离轨迹和车道线交点)
- 变道点x位置小于轨迹的一半左右(防止变道太晚)
- 轨迹终点小于车道线终点(防止路口导航变道误判)
- 不能跨多个车道线
```cpp
// 右车道变道触发器函数
bool checkRightLaneChange(
    bool has_right_lane_line,
    bool right_lane_line_is_dashed,
    const std::vector<Point>& lane_change_points, // 变道点列表
    double min_lane_change_x,
    double max_lane_change_x,
    double trajectory_end_d,      // 轨迹终点横向位置
    double lane_center_d,         // 右车道中心d
    double lane_width,            // 单车道宽度
    double trajectory_min_d,      // 轨迹最小d
    double trajectory_max_d       // 轨迹最大d
)
{
    // 1️⃣ 右车道线存在且是虚线
    bool lane_line_ok = has_right_lane_line && right_lane_line_is_dashed;

    // 2️⃣ 存在变道点
    bool has_lane_change_point = !lane_change_points.empty();

    // 3️⃣ 变道点 x 位置合适（取第一个点作为示例）
    bool lane_change_point_x_valid = false;
    if(has_lane_change_point)
    {
        double x = lane_change_points.front().x;
        lane_change_point_x_valid = (x >= min_lane_change_x) && (x <= max_lane_change_x);
    }

    // 4️⃣ 轨迹终点小于车道线终点
    bool trajectory_end_less_than_lane_end = (trajectory_end_d < lane_center_d + lane_width/2);

    // 5️⃣ 不跨越多车道
    double trajectory_lateral_span = trajectory_max_d - trajectory_min_d;
    bool not_crossing_multiple_lanes = (trajectory_lateral_span <= lane_width);

    // 6️⃣ 组合触发条件
    bool trigger = lane_line_ok
                 && has_lane_change_point
                 && lane_change_point_x_valid
                 && trajectory_end_less_than_lane_end
                 && not_crossing_multiple_lanes;

    return trigger;
}

```

### 1.5 左效率变道

在左变道基础上增加以下判断逻辑：
- 存在自车道的cipv
- 判断cipv是否小于未来自车最大速度
```cpp
// 轨迹相关结构体
struct TrajectoryPoint {
    double x;
    double y;
    double speed;  // m/s
};

// CIPV 结构体
struct CIPV {
    double distance;  // 与自车前向距离
    double speed;     // 当前速度
};

// 判断左变道效率触发条件
bool checkLeftEfficiencyLaneChange(
    bool left_lane_change_triggered,   // 左变道基础触发
    const CIPV* ego_lane_cipv,         // nullptr表示不存在CIPV
    double max_future_ego_speed        // 自车未来轨迹最大速度
)
{
    // 1️⃣ 左变道基础未触发，则直接返回 false
    if (!left_lane_change_triggered) return false;

    // 2️⃣ 判断 CIPV 是否存在
    if (!ego_lane_cipv) return false;

    // 3️⃣ 判断 CIPV 是否小于未来自车最大速度
    bool cipv_less_than_max_future_ego_speed = (ego_lane_cipv->speed < max_future_ego_speed);

    // 满足所有条件，触发左效率变道
    // 4️⃣ 组合触发条件
    bool trigger = left_lane_change_triggered
                 && ego_lane_cipv
                 && cipv_less_than_max_future_ego_speed
    return trigger;
}

```

### 1.6 右效率变道

在右变道基础上增加以下判断逻辑：
- 存在自车道的cipv
- 判断cipv是否小于未来自车最大速度
```cpp
// 轨迹相关结构体
struct TrajectoryPoint {
    double x;
    double y;
    double speed;  // m/s
};

// CIPV 结构体
struct CIPV {
    double distance;  // 与自车前向距离
    double speed;     // 当前速度
};

// 判断右变道效率触发条件
bool checkLeftEfficiencyLaneChange(
    bool right_lane_change_triggered,   // 右变道基础触发
    const CIPV* ego_lane_cipv,         // nullptr表示不存在CIPV
    double max_future_ego_speed        // 自车未来轨迹最大速度
)
{
    // 1️⃣ 右变道基础未触发，则直接返回 false
    if ( right_lane_change_triggered) return false;

    // 2️⃣ 判断 CIPV 是否存在
    if (!ego_lane_cipv) return false;

    // 3️⃣ 判断 CIPV 是否小于未来自车最大速度
    bool cipv_less_than_max_future_ego_speed = (ego_lane_cipv->speed < max_future_ego_speed);

    // 满足所有条件，触发右效率变道
    // 4️⃣ 组合触发条件
    bool trigger = right_lane_change_triggered
                 && ego_lane_cipv
                 && cipv_less_than_max_future_ego_speed
    return trigger;
}

```

## 2. 转弯场景 (Turn)

### 2.1 无交互左转

触发条件：
- 自车需要处于路口状态（左右车道线短于轨迹，或者自车前后一定范围内存在停止线，可以避免环岛等错误场景但是可能误杀很多无停止线的转弯场景）
- 自车的横摆角速度（yaw_raw）大于0.2（rad）
- 自车的结尾帧yaw大于0.3（rad）
- 车道线末端和轨迹末端夹角大于15度(排除曲率弯)
- 自车中点的yaw>0.1且与末端yaw反向
```cpp
bool checkUnprotectedLeftTurn(
    bool at_intersection,          // 是否处于路口
    double yaw_rate,               // 横摆角速度 rad/s
    double final_frame_yaw,        // 最终帧 yaw (rad)
    double lane_end_yaw,           // 车道线末端方向 yaw (rad)
    double traj_end_yaw,           // 轨迹末端方向 yaw (rad)
    double mid_ego_yaw             // 自车中点的 yaw (rad)
){
    // 1️⃣ 横摆角速度条件 yaw_rate > 0.2 rad/s
    bool cond_yaw_rate = yaw_rate > 0.2;

    // 2️⃣ 最终帧 yaw > 0.3 rad
    bool cond_final_frame_yaw = final_frame_yaw > 0.3;

    // 3️⃣ 车道线末端 vs 轨迹末端夹角 > 15°
    //    (15° ≈ 0.261799 rad)
    double angle_diff = fabs(lane_end_yaw - traj_end_yaw);
    bool cond_lane_end_traj_angle = angle_diff > (15.0 * M_PI / 180.0);

    // 4️⃣ 中段自车 yaw > 0.1 并且方向与最终帧 yaw 相反
    //    “反向”用叉乘符号判断：mid * final < 0
    bool mid_yaw_gt_01 = fabs(mid_ego_yaw) > 0.1;
    bool opposite_dir   = (mid_ego_yaw * final_frame_yaw) < 0;
    bool cond_mid_and_opposite = mid_yaw_gt_01 && opposite_dir;

    // 5️⃣ 总触发条件
    bool trigger =
        at_intersection &&
        cond_yaw_rate &&
        cond_final_frame_yaw &&
        cond_lane_end_traj_angle &&
        cond_mid_and_opposite;

    return trigger;
}

```

### 2.2 无交互右转

触发条件：
- 自车需要处于路口状态（左右车道线短于轨迹，或者自车前后一定范围内存在停止线）
- 自车的横摆角速度（yaw_raw）大于0.2（rad）
- 自车的结尾帧yaw小于-0.3（rad）
- 车道线末端和轨迹末端夹角大于15度(排除曲率弯)
- 自车中点的yaw>0.1且与末端yaw反向
```cpp
bool checkUnprotectedRightTurn(
    bool at_intersection,          // 是否处于路口
    double yaw_rate,               // 横摆角速度 rad/s
    double final_frame_yaw,        // 最终帧 yaw (rad)
    double lane_end_yaw,           // 车道线末端方向 yaw (rad)
    double traj_end_yaw,           // 轨迹末端方向 yaw (rad)
    double mid_ego_yaw             // 自车中点 yaw (rad)
){
    // 1️⃣ 横摆角速度 > 0.2 rad/s
    bool cond_yaw_rate = yaw_rate > 0.2;

    // 2️⃣ 最终帧 yaw < -0.3 rad（右转）
    bool cond_final_yaw = final_frame_yaw < -0.3;

    // 3️⃣ 车道线末端 vs 轨迹末端夹角 > 15°
    double angle_diff = fabs(lane_end_yaw - traj_end_yaw);
    bool cond_lane_traj_angle = angle_diff > (15.0 * M_PI / 180.0);

    // 4️⃣ 中段自车 yaw > 0.1 且方向与最终帧 yaw 反向
    bool mid_yaw_gt_01 = fabs(mid_ego_yaw) > 0.1;
    bool opposite_dir   = (mid_ego_yaw * final_frame_yaw) < 0;
    bool cond_mid_and_opposite = mid_yaw_gt_01 && opposite_dir;

    // 5️⃣ 总触发条件
    bool trigger =
        at_intersection &&
        cond_yaw_rate &&
        cond_final_yaw &&
        cond_lane_traj_angle &&
        cond_mid_and_opposite;

    return trigger;
}

```

### 2.3 有交互左转

在无交互左转基础上增加交互逻辑判断：
- 根据他车与自车未来轨迹点之间的距离（是否小于4m，并且自车时刻晚于他车时刻，不超过4s）
- 根据polygon判断是否存在交集
- 考虑他车是否减速
```cpp
bool checkInteractiveLeftTurn(
    bool non_interactive_left_turn,   // 已经满足无交互左转
    double dist_other_to_ego_future,  // 他车与自车未来轨迹最近距离
    double ego_arrival_time,          // 自车到达交叉点的时间 (s)
    double other_arrival_time,        // 他车到达交叉点的时间 (s)
    bool polygons_intersect,          // 未来轨迹是否有交集(多边形)
    double other_vehicle_acc,         // 他车加速度 (m/s^2)
    double decel_threshold = -0.5     // 减速判断阈值，可调
){
    // 1️⃣ 他车距离自车未来轨迹 < 4m
    bool other_vehicle_distance_lt_4m = (dist_other_to_ego_future < 4.0);

    // 2️⃣ 自车到达时间比他车晚 ≤ 4s
    double dt = ego_arrival_time - other_arrival_time;
    bool ego_later_than_other_le_4s = (dt >= 0.0 && dt <= 4.0);

    // 3️⃣ 是否存在未来轨迹或碰撞区域交集
    bool polygon_intersection_exists = polygons_intersect;

    // 4️⃣ 他车正在减速
    bool other_vehicle_decelerating = (other_vehicle_acc < decel_threshold);

    // 5️⃣ 总触发条件
    bool trigger =
        non_interactive_left_turn &&
        other_vehicle_distance_lt_4m &&
        ego_later_than_other_le_4s &&
        polygon_intersection_exists &&
        other_vehicle_decelerating;

    return trigger;
}

```

### 2.4 有交互右转

在无交互右转基础上增加交互逻辑判断：
- 根据他车与自车未来轨迹点之间的距离（是否小于4m，并且自车时刻晚于他车时刻，不超过4s）
- 根据polygon判断是否存在交集
- 考虑他车是否减速
```cpp
bool checkInteractiveRightTurn(
    bool non_interactive_right_turn,  // 已满足无交互右转
    double dist_other_to_ego_future,  // 他车到自车未来轨迹最近距离（m）
    double ego_arrival_time,          // 自车到冲突点的到达时间（s）
    double other_arrival_time,        // 他车到冲突点到达时间（s）
    bool polygons_intersect,          // 未来轨迹多边形是否有交集
    double other_vehicle_acc,         // 他车加速度（负值表示减速）
    double decel_threshold = -0.5     // 减速阈值，可调
){
    // 1️⃣ 他车与自车未来轨迹点距离 < 4m
    bool other_vehicle_distance_lt_4m =
        (dist_other_to_ego_future < 4.0);

    // 2️⃣ 自车比他车到达晚，且不超过 4s（代表存在交互）
    double dt = ego_arrival_time - other_arrival_time;
    bool ego_later_than_other_le_4s =
        (dt >= 0.0 && dt <= 4.0);

    // 3️⃣ 是否存在未来轨迹空间交集 (碰撞区域)
    bool polygon_intersection_exists = polygons_intersect;

    // 4️⃣ 他车减速（通常意味着为右转让行）
    bool other_vehicle_decelerating =
        (other_vehicle_acc < decel_threshold);

    // 5️⃣ 综合触发
    bool trigger =
        non_interactive_right_turn &&
        other_vehicle_distance_lt_4m &&
        ego_later_than_other_le_4s &&
        polygon_intersection_exists &&
        other_vehicle_decelerating;

    return trigger;
}

```

### 2.5 左转弯静止

在左转基础上，判断自车是否静止：速度小于0.01m/s，触发回传时间点静止
```cpp
bool checkStaticLeftTurn(
    bool left_turn,      // 已满足左转触发（无交互 or 有交互左转）
    double ego_speed_mps // 自车速度（m/s）
){
    // 自车速度 < 0.01 m/s
    bool ego_speed_lt_001 = (ego_speed_mps < 0.01);

    // 触发逻辑
    bool trigger = left_turn && ego_speed_lt_001;

    return trigger;
}

```

### 2.6 右转弯静止

在右转基础上，判断自车是否静止：速度小于0.01m/s，触发回传时间点静止
```cpp
bool checkStaticRightTurn(
    bool right_turn,     // 已满足右转触发（无交互 or 有交互右转）
    double ego_speed_mps // 自车当前速度（m/s）
){
    // 自车速度 < 0.01 m/s
    bool ego_speed_lt_001 = (ego_speed_mps < 0.01);

    // 总触发：右转中 + 静止
    bool trigger = right_turn && ego_speed_lt_001;

    return trigger;
}

```

### 2.7 左转交互静止

在左转交互基础上，判断自车是否静止：速度小于0.01m/s
```cpp
bool checkInteractiveLeftTurnStatic(
    bool interactive_left_turn, // 已满足“有交互左转”（210011）
    double ego_speed_mps        // 自车速度（m/s）
){
    // 自车速度 < 0.01 m/s → 静止
    bool ego_speed_lt_001 = (ego_speed_mps < 0.01);

    // 触发逻辑
    bool trigger = interactive_left_turn && ego_speed_lt_001;

    return trigger;
}

```

### 2.8 右转交互静止

在右转交互基础上，判断自车是否静止：速度小于0.01m/s
```cpp
bool checkInteractiveRightTurnStatic(
    bool interactive_right_turn, // 已满足“有交互右转”（210012）
    double ego_speed_mps         // 自车速度（m/s）
){
    // 自车速度 < 0.01 m/s → 静止
    bool ego_speed_lt_001 = (ego_speed_mps < 0.01);

    // 触发逻辑
    bool trigger = interactive_right_turn && ego_speed_lt_001;

    return trigger;
}

```

## 3. 静止场景 (Standstill)

触发条件：
- 自车速度小于0.1m/s
- 单次采集90s
- 触发间隔在10分钟
```cpp
bool checkStandstill(
    double ego_speed_mps // 自车当前速度（m/s）
){
    // 自车速度 < 0.1 m/s
    bool ego_speed_lt_01 = (ego_speed_mps < 0.1);

    // 触发条件
    bool trigger = ego_speed_lt_01;

    return trigger;
}

```

## 4. 交互场景 (Interaction)

### 4.1 它车横穿交互

触发条件：
- 满足交互条件
- 它车轨迹和自车轨迹最小距离小于4m
- 自车比它车后到交互点且不能后到太久
- 它车和自车bbx有交叠
- 在满足交互条件上,它车的heading和自车的heading在17-162度之间
```cpp
bool checkCrossingInteraction(
    bool interaction_conditions_met,  // 已满足基础交互条件（如对向或横向车辆存在潜在冲突）
    double min_trajectory_distance,   // 自车与它车轨迹最小距离 (m)
    double ego_arrival_time,          // 自车到交互点时间 (s)
    double other_arrival_time,        // 他车到交互点时间 (s)
    double max_late_time = 4.0,       // 自车后到交互点最大允许延迟 (s)
    bool bboxes_overlap,              // 自车与他车包围盒是否重叠
    double ego_heading_rad,           // 自车朝向 (rad)
    double other_heading_rad          // 他车朝向 (rad)
){
    // 1️⃣ 最小轨迹距离 < 4m
    bool min_trajectory_distance_lt_4m = (min_trajectory_distance < 4.0);

    // 2️⃣ 自车比他车晚到交互点，但不能太晚
    double dt = ego_arrival_time - other_arrival_time;
    bool ego_later_at_interaction_point = (dt >= 0.0 && dt <= max_late_time);

    // 3️⃣ 包围盒重叠
    bool bboxes_overlap_flag = bboxes_overlap;

    // 4️⃣ heading 角度在 17°-162°之间
    //    转换为弧度 17° ≈ 0.2967 rad, 162° ≈ 2.827 rad
    double heading_diff = fabs(ego_heading_rad - other_heading_rad);
    // 确保角度在 0-π 之间
    if (heading_diff > M_PI) heading_diff = 2*M_PI - heading_diff;

    bool heading_angle_between_17_162 = (heading_diff >= 17.0 * M_PI / 180.0 &&
                                         heading_diff <= 162.0 * M_PI / 180.0);

    // 5️⃣ 综合触发条件
    bool trigger = interaction_conditions_met &&
                   min_trajectory_distance_lt_4m &&
                   ego_later_at_interaction_point &&
                   bboxes_overlap_flag &&
                   heading_angle_between_17_162;

    return trigger;
}

```

### 4.2 它车急减速交互

触发条件：
- 同上交互条件
- 在满足交互条件上,它车存在急减速min_agent_hist_acc < -1
```cpp
bool checkSuddenDecelerationInteraction(
    bool interaction_conditions_met, // 已满足基础交互条件（潜在冲突）
    double min_agent_hist_acc        // 它车历史加速度最小值（m/s²）
){
    // 1️⃣ 它车急减速：历史加速度 < -1 m/s²
    bool min_agent_hist_acc_lt_neg1 = (min_agent_hist_acc < -1.0);

    // 2️⃣ 总触发条件
    bool trigger = interaction_conditions_met && min_agent_hist_acc_lt_neg1;

    return trigger;
}

```

## 5. 偏离回正场景 (Deviation Correction)

触发条件：
- 不能处于转弯／曲率弯道／路口场景
- 自车前3/8轨迹有压线趋势, 自车后半段轨迹居中行驶
```cpp
// 轨迹点结构
struct TrajectoryPoint {
    double d;    // 横向偏移，车道中心为0
    double x;    // x坐标
    double y;    // y坐标
};

bool checkDeviationCorrection(
    bool not_turn,                    // 非转弯
    bool not_curve,                   // 非曲率弯道
    bool not_intersection,            // 非路口
    const std::vector<TrajectoryPoint>& trajectory_points, // 自车轨迹
    double lane_width                  // 当前车道宽度
){
    if (trajectory_points.empty()) return false;

    // 1️⃣ 基础场景判断
    bool basic_scene_ok = not_turn && not_curve && not_intersection;

    // 2️⃣ 前3/8轨迹压线趋势
    size_t n_points = trajectory_points.size();
    size_t front_limit = n_points * 3 / 8;

    bool front_press_line_trend = false;
    for (size_t i = 0; i < front_limit; ++i) {
        // 当横向偏移接近车道边界时，认为有压线趋势
        if (std::abs(trajectory_points[i].d) > lane_width/2 * 0.8) {
            front_press_line_trend = true;
            break;
        }
    }

    // 3️⃣ 后半段轨迹居中
    bool latter_half_centered = true;
    size_t latter_start = n_points / 2;
    for (size_t i = latter_start; i < n_points; ++i) {
        if (std::abs(trajectory_points[i].d) > lane_width/2 * 0.3) { 
            // 超过车道宽30%认为不居中
            latter_half_centered = false;
            break;
        }
    }

    // 4️⃣ 综合触发条件
    bool trigger = basic_scene_ok && front_press_line_trend && latter_half_centered;

    return trigger;
}

```

## 6. 大曲率弯道场景 (High Curvature Turn)

触发条件：
- 自车速度大于5m/s
- 自车左右都存在车道线，且自车未来轨迹在两条车道线之间，自车位于两条车道线内
- 车道线的c2,c3大于0.001
- 自车未来yaw大于0.02
- 自车不压线
```cpp
struct TrajectoryPoint {
    double x;
    double y;
    double d;   // 横向偏移，车道中心为0
    double yaw; // 当前点yaw
};

struct LaneLine {
    bool exists;
    double c2;
    double c3;
    double d_center; // 车道中心横向坐标
};

bool checkHighCurvatureLaneKeeping(
    double ego_speed,                          // 自车速度 m/s
    const LaneLine& left_lane,                 // 左车道线信息
    const LaneLine& right_lane,                // 右车道线信息
    const std::vector<TrajectoryPoint>& future_trajectory, // 自车未来轨迹
    double lane_width
){
    // 1️⃣ 速度大于5 m/s
    bool ego_speed_gt_5 = (ego_speed > 5.0);

    // 2️⃣ 左右车道线存在
    bool both_lane_lines_exist = left_lane.exists && right_lane.exists;

    // 3️⃣ 轨迹在两条车道线之间
    bool future_trajectory_between_lanes = true;
    for (const auto& pt : future_trajectory) {
        if (pt.d < -lane_width/2 || pt.d > lane_width/2) { // 横向偏移超出车道
            future_trajectory_between_lanes = false;
            break;
        }
    }

    // 4️⃣ 自车当前位于两条车道线内
    double ego_d = future_trajectory.empty() ? 0.0 : future_trajectory[0].d;
    bool ego_between_lane_lines = (ego_d >= -lane_width/2 && ego_d <= lane_width/2);

    // 5️⃣ 车道线 c2, c3 > 0.001
    bool lane_c2_c3_gt_0001 = (left_lane.c2 > 0.001 && left_lane.c3 > 0.001 &&
                               right_lane.c2 > 0.001 && right_lane.c3 > 0.001);

    // 6️⃣ 自车未来 yaw > 0.02
    bool future_yaw_gt_002 = false;
    for (const auto& pt : future_trajectory) {
        if (pt.yaw > 0.02) {
            future_yaw_gt_002 = true;
            break;
        }
    }

    // 7️⃣ 不压线
    bool not_pressing_line = true;
    for (const auto& pt : future_trajectory) {
        if (std::abs(pt.d) > lane_width/2 * 0.95) { // 超过95%车道宽度认为压线
            not_pressing_line = false;
            break;
        }
    }

    // 8️⃣ 综合触发条件
    bool trigger = ego_speed_gt_5 &&
                   both_lane_lines_exist &&
                   future_trajectory_between_lanes &&
                   ego_between_lane_lines &&
                   lane_c2_c3_gt_0001 &&
                   future_yaw_gt_002 &&
                   not_pressing_line;

    return trigger;
}

```

## 7. 加减速场景 (Acceleration/Deceleration)

### 7.1 刹车

触发条件：
- 转向稳定(固定距离轨迹曲率判断)并且自车不压线后
- 自车加速度小于-0.15，且存在交互他车
```cpp
bool checkDeceleration(
    bool steering_stable,                // 转向稳定
    bool not_pressing_line,              // 不压线
    double ego_acceleration,             // 自车加速度 m/s²
    bool interactive_other_vehicle_exists // 存在交互它车
){
    // 1️⃣ 自车加速度 < -0.15 m/s²
    bool ego_acceleration_lt_neg015 = (ego_acceleration < -0.15);

    // 2️⃣ 综合触发条件
    bool trigger = steering_stable &&
                   not_pressing_line &&
                   ego_acceleration_lt_neg015 &&
                   interactive_other_vehicle_exists;

    return trigger;
}

```

### 7.2 刹停

触发条件：
- 转向稳定(固定距离轨迹曲率判断)并且自车不压线后
- 固定时间轨迹中，速度包含0.01m/s以下轨迹点
```cpp
struct TrajectoryPoint {
    double x;
    double y;
    double d;       // 横向偏移，车道中心为0
    double speed;   // 当前点速度 m/s
    double yaw;
};

bool checkStop(
    bool steering_stable,                      // 转向稳定
    bool not_pressing_line,                    // 不压线
    const std::vector<TrajectoryPoint>& trajectory_points // 固定时间轨迹
){
    // 1️⃣ 轨迹中是否存在速度 <= 0.01 m/s 的点
    bool trajectory_contains_speed_le_001 = false;
    for (const auto& pt : trajectory_points) {
        if (pt.speed <= 0.01) {
            trajectory_contains_speed_le_001 = true;
            break;
        }
    }

    // 2️⃣ 综合触发条件
    bool trigger = steering_stable &&
                   not_pressing_line &&
                   trajectory_contains_speed_le_001;

    return trigger;
}

```

### 7.3 加速

触发条件：
- 转向稳定(固定距离轨迹曲率判断)并且自车不压线后
- 自车速度大于3/s，且最大加速度大于0.15
```cpp
struct TrajectoryPoint {
    double x;
    double y;
    double d;             // 横向偏移，车道中心为0
    double speed;         // 当前点速度 m/s
    double acceleration;  // 当前点加速度 m/s²
    double yaw;           // 当前点yaw
};

// 计算三点曲率
double computeCurvature(const TrajectoryPoint& p0, const TrajectoryPoint& p1, const TrajectoryPoint& p2) {
    double x1 = p0.x, y1 = p0.y;
    double x2 = p1.x, y2 = p1.y;
    double x3 = p2.x, y3 = p2.y;

    double a = std::hypot(x2 - x1, y2 - y1);
    double b = std::hypot(x3 - x2, y3 - y2);
    double c = std::hypot(x3 - x1, y3 - y1);

    double s = (a + b + c) / 2.0;
    double area = std::sqrt(std::max(s * (s - a) * (s - b) * (s - c), 0.0));

    if (area < 1e-6) return 0.0; // 近似直线

    return (4.0 * area) / (a * b * c);
}

// 固定距离轨迹曲率判断转向稳定
bool isSteeringStable(const std::vector<TrajectoryPoint>& trajectory, double max_curvature_threshold = 0.05) {
    size_t n = trajectory.size();
    if (n < 3) return true;

    size_t window_size = std::min(n, size_t(5));
    for (size_t i = 0; i <= n - 3 && i < window_size; ++i) {
        double k = computeCurvature(trajectory[i], trajectory[i + 1], trajectory[i + 2]);
        if (std::abs(k) > max_curvature_threshold) return false;
    }
    return true;
}

// 加速触发器
bool checkAcceleration(
    const std::vector<TrajectoryPoint>& trajectory,
    double lane_width
) {
    if (trajectory.empty()) return false;

    // 1️⃣ 转向稳定
    bool steering_stable = isSteeringStable(trajectory, 0.05);

    // 2️⃣ 不压线
    bool not_pressing_line = true;
    for (const auto& pt : trajectory) {
        if (std::abs(pt.d) > lane_width / 2 * 0.95) {
            not_pressing_line = false;
            break;
        }
    }

    // 3️⃣ 自车速度 > 3 m/s (取轨迹起点速度)
    double ego_speed = trajectory[0].speed;
    bool ego_speed_gt_3 = (ego_speed > 3.0);

    // 4️⃣ 最大加速度 > 0.15 m/s²
    double max_acceleration = 0.0;
    for (const auto& pt : trajectory) {
        if (pt.acceleration > max_acceleration) max_acceleration = pt.acceleration;
    }
    bool max_acceleration_gt_015 = (max_acceleration > 0.15);

    // 5️⃣ 综合触发
    return steering_stable && not_pressing_line && ego_speed_gt_3 && max_acceleration_gt_015;
}

```

### 7.4 安全加速 (safe_acc)

触发条件：
- 无交互情况下
- 如果自车速度在3m/s ~ 8m/s范围内提速到10m/s以上
```cpp
struct TrajectoryPoint {
    double x;
    double y;
    double d;       // 横向偏移
    double speed;   // 当前点速度 m/s
    double acceleration; // 当前点加速度 m/s²
    double yaw;
};

// 安全加速触发器
bool checkSafeAcceleration(
    bool no_interaction,                       // 无交互
    const std::vector<TrajectoryPoint>& trajectory // 固定时间轨迹
){
    if (trajectory.empty()) return false;

    // 1️⃣ 当前速度在 3~8 m/s 范围（取轨迹起点速度）
    double ego_speed = trajectory[0].speed;
    bool ego_speed_3_to_8 = (ego_speed >= 3.0 && ego_speed <= 8.0);

    // 2️⃣ 提速到 10 m/s 以上（轨迹最大速度）
    double max_speed = ego_speed;
    for (const auto& pt : trajectory) {
        if (pt.speed > max_speed) max_speed = pt.speed;
    }
    bool speed_up_to_gt_10 = (max_speed > 10.0);

    // 3️⃣ 综合触发
    return no_interaction && ego_speed_3_to_8 && speed_up_to_gt_10;
}

```

## 8. 车道保持场景 (Lane Keeping)

### 8.1 普通车道保持

触发条件：
- 自车速度大于5m/s
- 自车左右都存在车道线，且自车未来轨迹在两条车道线之间，自车位于两条车道线中间
- 自车不压线
```cpp
struct TrajectoryPoint {
    double x;
    double y;
    double d;       // 横向偏移，车道中心为0
    double speed;   // 当前点速度 m/s
    double yaw;
};

bool checkLaneKeeping(
    const std::vector<TrajectoryPoint>& trajectory,
    double lane_width,
    bool left_lane_exists,
    bool right_lane_exists
){
    if (trajectory.empty()) return false;

    // 1️⃣ 自车速度 > 5 m/s (取轨迹起点速度)
    double ego_speed = trajectory[0].speed;
    bool ego_speed_gt_5 = (ego_speed > 5.0);

    // 2️⃣ 左右车道线存在
    bool both_lane_lines_exist = left_lane_exists && right_lane_exists;

    // 3️⃣ 自车未来轨迹在两条车道线之间
    bool future_trajectory_between_lanes = true;
    for (const auto& pt : trajectory) {
        if (std::abs(pt.d) > lane_width / 2) {
            future_trajectory_between_lanes = false;
            break;
        }
    }

    // 4️⃣ 自车位于车道中心（±10% 宽度容差）
    bool ego_at_lane_center = true;
    for (const auto& pt : trajectory) {
        if (std::abs(pt.d) > lane_width * 0.1) {
            ego_at_lane_center = false;
            break;
        }
    }

    // 5️⃣ 不压线（±95% 宽度）
    bool not_pressing_line = true;
    for (const auto& pt : trajectory) {
        if (std::abs(pt.d) > lane_width * 0.95) {
            not_pressing_line = false;
            break;
        }
    }

    // 6️⃣ 综合触发
    bool trigger = ego_speed_gt_5 &&
                   both_lane_lines_exist &&
                   future_trajectory_between_lanes &&
                   ego_at_lane_center &&
                   not_pressing_line;

    return trigger;
}

```

### 8.2 过路口车道保持

触发条件：
- 自车速度大于5m/s
- 自车在路口附近
- 自车未来一段时间的轨迹，被夹在两段远处平行车道线之间
```cpp
struct TrajectoryPoint {
    double x;
    double y;
    double d;       // 横向偏移，车道中心为0
    double speed;   // 当前点速度 m/s
    double yaw;
};

// 判断未来轨迹是否被夹在两条远处平行车道线之间
bool isTrajectorySqueezedByParallelLanes(const std::vector<TrajectoryPoint>& trajectory,
                                         double left_lane_d, double right_lane_d)
{
    for (const auto& pt : trajectory) {
        if (pt.d < left_lane_d || pt.d > right_lane_d) {
            return false;
        }
    }
    return true;
}

// 过路口车道保持触发器
bool checkIntersectionLaneKeeping(
    const std::vector<TrajectoryPoint>& trajectory,
    bool near_intersection,  // 是否在路口附近
    double left_lane_d,      // 左远处平行车道线d坐标
    double right_lane_d      // 右远处平行车道线d坐标
){
    if (trajectory.empty()) return false;

    // 1️⃣ 自车速度 > 5 m/s
    double ego_speed = trajectory[0].speed;
    bool ego_speed_gt_5 = (ego_speed > 5.0);

    // 2️⃣ 路口附近
    // 参数 near_intersection 已由上层地图/感知计算
    // bool near_intersection = ...

    // 3️⃣ 未来轨迹被夹在平行车道线之间
    bool future_trajectory_squeezed = isTrajectorySqueezedByParallelLanes(trajectory, left_lane_d, right_lane_d);

    // 4️⃣ 综合触发
    bool trigger = ego_speed_gt_5 && near_intersection && future_trajectory_squeezed;

    return trigger;
}

```

## 9. 路口场景 (Intersection)

### 9.1 接近路口

触发条件：
- 有红绿灯信号（表示在路口附近）
- 固定时间轨迹和任意水平车道线保持1m以上的距离
- 自车速度大于5m/s
```cpp
struct TrajectoryPoint {
    double x;
    double y;
    double d;       // 横向偏移，车道中心为0
    double speed;   // 当前点速度 m/s
    double yaw;
};

struct LaneLine {
    double d;       // 车道线横向位置
};

// 判断轨迹与任意水平车道线保持 > 1m 距离
bool isTrajectoryLaneDistanceGt1m(const std::vector<TrajectoryPoint>& trajectory,
                                  const std::vector<LaneLine>& lane_lines)
{
    for (const auto& pt : trajectory) {
        for (const auto& lane : lane_lines) {
            if (std::abs(pt.d - lane.d) < 1.0) {
                return false; // 距离小于1m，不满足条件
            }
        }
    }
    return true;
}

// 接近路口触发器
bool checkApproachingIntersection(
    const std::vector<TrajectoryPoint>& trajectory,
    bool traffic_light_signal_present,
    const std::vector<LaneLine>& lane_lines
){
    if (trajectory.empty()) return false;

    // 1️⃣ 有红绿灯信号
    bool has_signal = traffic_light_signal_present;

    // 2️⃣ 轨迹与任意水平车道线保持 >1m 距离
    bool traj_lane_distance_ok = isTrajectoryLaneDistanceGt1m(trajectory, lane_lines);

    // 3️⃣ 自车速度 > 5 m/s
    double ego_speed = trajectory[0].speed;
    bool ego_speed_gt_5 = (ego_speed > 5.0);

    // 4️⃣ 综合触发
    return has_signal && traj_lane_distance_ok && ego_speed_gt_5;
}

```

## 10. 分合流场景 (Merge/Diverge)

### 10.1 分流

触发条件：
- 自车向分流车道变道
- 左侧分流：
  - 自车存在左车道线
  - 左车道线起点<分流车道线起点<左车道线终点
  - 分流车道线起点>0
  - 分流车道线起点距左车道线距离dis < dis_th
  - 分流车道线起点方向和左车道线终点方向夹角>theta_th
- 右侧分流：
  - 自车存在右车道线
  - 右车道线起点<分流车道线起点<右车道线终点
  - 分流车道线起点>0
  - 分流车道线起点距右车道线距离dis < dis_th
  - 分流车道线起点方向和右车道线终点方向夹角>theta_th
- 注意：当前场景判断逻辑不覆盖路口少变多类型的分流
```cpp
struct LaneLine {
    double x_start;
    double y_start;
    double x_end;
    double y_end;
};

// 计算两点距离
inline double distance(double x1, double y1, double x2, double y2) {
    return std::hypot(x2 - x1, y2 - y1);
}

// 计算两向量夹角（弧度）
inline double angleBetweenVectors(double x1, double y1, double x2, double y2) {
    double dot = x1*x2 + y1*y2;
    double norm1 = std::hypot(x1, y1);
    double norm2 = std::hypot(x2, y2);
    return std::acos(dot / (norm1 * norm2));
}

// 左侧分流判断
bool checkLeftDiverging(
    const LaneLine& left_lane,
    const LaneLine& diverging_lane,
    double dis_th,
    double theta_th
){
    // 1️⃣ 左车道线存在
    // 2️⃣ 分流起点在左车道线起点和终点之间
    double x_div = diverging_lane.x_start;
    if (!(left_lane.x_start < x_div && x_div < left_lane.x_end)) return false;

    // 3️⃣ 分流起点 > 0
    if (x_div <= 0) return false;

    // 4️⃣ 分流起点距左车道线距离 < dis_th
    double dis = distance(diverging_lane.x_start, diverging_lane.y_start,
                          left_lane.x_start, left_lane.y_start);
    if (dis > dis_th) return false;

    // 5️⃣ 分流起点方向和左车道线终点方向夹角 > theta_th
    double diverging_dir_x = diverging_lane.x_end - diverging_lane.x_start;
    double diverging_dir_y = diverging_lane.y_end - diverging_lane.y_start;
    double left_dir_x = left_lane.x_end - left_lane.x_start;
    double left_dir_y = left_lane.y_end - left_lane.y_start;

    double theta = angleBetweenVectors(diverging_dir_x, diverging_dir_y,
                                       left_dir_x, left_dir_y);

    if (theta < theta_th) return false;

    return true;
}

// 右侧分流判断
bool checkRightDiverging(
    const LaneLine& right_lane,
    const LaneLine& diverging_lane,
    double dis_th,
    double theta_th
){
    // 1️⃣ 右车道线存在
    // 2️⃣ 分流起点在右车道线起点和终点之间
    double x_div = diverging_lane.x_start;
    if (!(right_lane.x_start < x_div && x_div < right_lane.x_end)) return false;

    // 3️⃣ 分流起点 > 0
    if (x_div <= 0) return false;

    // 4️⃣ 分流起点距右车道线距离 < dis_th
    double dis = distance(diverging_lane.x_start, diverging_lane.y_start,
                          right_lane.x_start, right_lane.y_start);
    if (dis > dis_th) return false;

    // 5️⃣ 分流起点方向和右车道线终点方向夹角 > theta_th
    double diverging_dir_x = diverging_lane.x_end - diverging_lane.x_start;
    double diverging_dir_y = diverging_lane.y_end - diverging_lane.y_start;
    double right_dir_x = right_lane.x_end - right_lane.x_start;
    double right_dir_y = right_lane.y_end - right_lane.y_start;

    double theta = angleBetweenVectors(diverging_dir_x, diverging_dir_y,
                                       right_dir_x, right_dir_y);

    if (theta < theta_th) return false;

    return true;
}

// 综合触发
bool checkDiverging(
    const LaneLine* left_lane,   // nullptr表示左车道不存在
    const LaneLine* right_lane,  // nullptr表示右车道不存在
    const LaneLine& diverging_lane,
    double dis_th,
    double theta_th
){
    bool left_trigger = false;
    bool right_trigger = false;

    if (left_lane) left_trigger = checkLeftDiverging(*left_lane, diverging_lane, dis_th, theta_th);
    if (right_lane) right_trigger = checkRightDiverging(*right_lane, diverging_lane, dis_th, theta_th);

    return left_trigger || right_trigger;
}

```

### 10.2 合流

触发条件：
- 自车存在左右车道线
- 旁车道线起点<合流车道线终点<旁车道线终点
- 合流线终点>0
- 合流车道线终点距旁车道线距离dis<dis_th
- 合流车道线终点方向合旁车道线起点方向夹角>theta_th
- 注意：当前场景判断逻辑不覆盖路口多变少类型的分流
```cpp
struct LaneLine {
    double x_start;
    double y_start;
    double x_end;
    double y_end;
};

// 计算两点距离
inline double distance(double x1, double y1, double x2, double y2) {
    return std::hypot(x2 - x1, y2 - y1);
}

// 计算两向量夹角（弧度）
inline double angleBetweenVectors(double x1, double y1, double x2, double y2) {
    double dot = x1*x2 + y1*y2;
    double norm1 = std::hypot(x1, y1);
    double norm2 = std::hypot(x2, y2);
    return std::acos(dot / (norm1 * norm2));
}

// 左侧合流判断
bool checkLeftMerging(
    const LaneLine& left_lane,
    const LaneLine& merging_lane,
    double dis_th,
    double theta_th
){
    double x_merge_end = merging_lane.x_end;

    // 1️⃣ 左车道线存在
    // 2️⃣ 合流终点在左车道线起点和终点之间
    if (!(left_lane.x_start < x_merge_end && x_merge_end < left_lane.x_end)) return false;

    // 3️⃣ 合流终点 > 0
    if (x_merge_end <= 0) return false;

    // 4️⃣ 合流终点距左车道线距离 < dis_th
    double dis = distance(merging_lane.x_end, merging_lane.y_end,
                          left_lane.x_end, left_lane.y_end);
    if (dis > dis_th) return false;

    // 5️⃣ 合流终点方向与左车道线起点方向夹角 > theta_th
    double merge_dir_x = merging_lane.x_end - merging_lane.x_start;
    double merge_dir_y = merging_lane.y_end - merging_lane.y_start;
    double left_dir_x = left_lane.x_end - left_lane.x_start;
    double left_dir_y = left_lane.y_end - left_lane.y_start;

    double theta = angleBetweenVectors(merge_dir_x, merge_dir_y, left_dir_x, left_dir_y);
    if (theta < theta_th) return false;

    return true;
}

// 右侧合流判断
bool checkRightMerging(
    const LaneLine& right_lane,
    const LaneLine& merging_lane,
    double dis_th,
    double theta_th
){
    double x_merge_end = merging_lane.x_end;

    if (!(right_lane.x_start < x_merge_end && x_merge_end < right_lane.x_end)) return false;
    if (x_merge_end <= 0) return false;

    double dis = distance(merging_lane.x_end, merging_lane.y_end,
                          right_lane.x_end, right_lane.y_end);
    if (dis > dis_th) return false;

    double merge_dir_x = merging_lane.x_end - merging_lane.x_start;
    double merge_dir_y = merging_lane.y_end - merging_lane.y_start;
    double right_dir_x = right_lane.x_end - right_lane.x_start;
    double right_dir_y = right_lane.y_end - right_lane.y_start;

    double theta = angleBetweenVectors(merge_dir_x, merge_dir_y, right_dir_x, right_dir_y);
    if (theta < theta_th) return false;

    return true;
}

// 综合合流触发
bool checkMerging(
    const LaneLine* left_lane,    // nullptr表示左车道不存在
    const LaneLine* right_lane,   // nullptr表示右车道不存在
    const LaneLine& merging_lane,
    double dis_th,
    double theta_th
){
    bool left_trigger = false;
    bool right_trigger = false;

    if (left_lane) left_trigger = checkLeftMerging(*left_lane, merging_lane, dis_th, theta_th);
    if (right_lane) right_trigger = checkRightMerging(*right_lane, merging_lane, dis_th, theta_th);

    return left_trigger || right_trigger;
}

```

## 11. 环岛场景 (Roundabout)

触发条件：
- 自车靠近环岛
```cpp
struct Point2D {
    double x;
    double y;
};

// 简单判断车辆是否靠近环岛
// 环岛中心 (cx, cy)，半径 r，阈值 distance_th
bool isNearRoundabout(const Point2D& ego_pos,
                      const Point2D& roundabout_center,
                      double roundabout_radius,
                      double distance_th)
{
    double dist = std::hypot(ego_pos.x - roundabout_center.x, ego_pos.y - roundabout_center.y);
    return dist <= (roundabout_radius + distance_th);
}

// 环岛触发器
bool checkRoundabout(const Point2D& ego_pos,
                     const Point2D& roundabout_center,
                     double roundabout_radius,
                     double distance_th = 10.0) // 默认阈值 10m
{
    return isNearRoundabout(ego_pos, roundabout_center, roundabout_radius, distance_th);
}

```