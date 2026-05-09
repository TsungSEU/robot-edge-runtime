// humanoid_state.cpp
#include "humanoid_state.h"
#include "common/log/logger.h"
#include <sstream>
#include <iomanip>

namespace aurora::planner {

HumanoidState HumanoidState::fromStateInfo(const HumanoidStateInfo& info) {
    HumanoidState state;

    // [0-2]: base_lin_vel ×2.0
    state.features[0] = info.vx * 2.0;
    state.features[1] = info.vy * 2.0;
    state.features[2] = info.vz * 2.0;

    // [3-5]: base_ang_vel ×1.0
    state.features[3] = info.wx;
    state.features[4] = info.wy;
    state.features[5] = info.wz;

    // [6-7]: norm_position [x/W, y/H]
    state.features[6] = info.map_width > 0 ? info.x / info.map_width : 0.0;
    state.features[7] = info.map_height > 0 ? info.y / info.map_height : 0.0;

    // [8-9]: heading [sinθ, cosθ]
    state.features[8] = std::sin(info.theta);
    state.features[9] = std::cos(info.theta);

    // [10-11]: goal_direction [sinΔθ, cosΔθ]
    double bearing = info.goal_bearing;
    state.features[10] = std::sin(bearing);
    state.features[11] = std::cos(bearing);

    // [12-14]: goal_distance [Δx, Δy, ‖Δ‖] ÷max_range
    double max_range = info.max_range > 0 ? info.max_range : 10.0;
    state.features[12] = info.goal_dx / max_range;
    state.features[13] = info.goal_dy / max_range;
    state.features[14] = info.goal_distance / max_range;

    // [15-22]: data_value_sectors (8方向) [0,1]
    for (int i = 0; i < 8; ++i) {
        state.features[15 + i] = std::clamp(info.data_value_sectors[i], 0.0, 1.0);
    }

    // [23-26]: obstacle_sectors (4方向) ÷max_range
    for (int i = 0; i < 4; ++i) {
        state.features[23 + i] = info.obstacle_sectors[i] / max_range;
    }

    // [27-28]: current_value [value, rarity]
    state.features[27] = std::clamp(info.current_value, 0.0, 1.0);
    state.features[28] = std::clamp(info.current_rarity, 0.0, 1.0);

    // [29-30]: collection_status [ratio, coverage]
    state.features[29] = std::clamp(info.collected_ratio, 0.0, 1.0);
    state.features[30] = std::clamp(info.coverage_ratio, 0.0, 1.0);

    // [31]: terrain_type ÷6
    state.features[31] = static_cast<double>(info.terrain_type) / 6.0;

    // [32]: obstacle_density [0,1]
    state.features[32] = std::clamp(info.obstacle_density, 0.0, 1.0);

    // [33]: gait_phase sin(2π·φ)
    state.features[33] = std::sin(2.0 * M_PI * info.gait_phase);

    // [34-41]: action_history (最近8步 forward_vel)
    for (int i = 0; i < 8; ++i) {
        state.features[34 + i] = info.action_history[i];
    }

    // [42]: remaining_budget [0,1]
    state.features[42] = std::clamp(info.remaining_budget, 0.0, 1.0);

    return state;
}

std::string HumanoidState::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "HumanoidState[43]:\n";
    oss << "  lin_vel:   [" << features[0] << ", " << features[1] << ", " << features[2] << "]\n";
    oss << "  ang_vel:   [" << features[3] << ", " << features[4] << ", " << features[5] << "]\n";
    oss << "  norm_pos:  [" << features[6] << ", " << features[7] << "]\n";
    oss << "  heading:   [" << features[8] << ", " << features[9] << "]\n";
    oss << "  goal_dir:  [" << features[10] << ", " << features[11] << "]\n";
    oss << "  goal_dist: [" << features[12] << ", " << features[13] << ", " << features[14] << "]\n";
    oss << "  data_val:  [";
    for (int i = 0; i < 8; ++i) {
        if (i > 0) oss << ", ";
        oss << features[15 + i];
    }
    oss << "]\n";
    oss << "  obstacles: [" << features[23] << ", " << features[24]
        << ", " << features[25] << ", " << features[26] << "]\n";
    oss << "  cur_value: [" << features[27] << ", " << features[28] << "]\n";
    oss << "  collect:   [" << features[29] << ", " << features[30] << "]\n";
    oss << "  terrain:   " << features[31] << "\n";
    oss << "  obs_dens:  " << features[32] << "\n";
    oss << "  gait:      " << features[33] << "\n";
    oss << "  act_hist:  [";
    for (int i = 0; i < 8; ++i) {
        if (i > 0) oss << ", ";
        oss << features[34 + i];
    }
    oss << "]\n";
    oss << "  budget:    " << features[42];
    return oss.str();
}

} // namespace aurora::planner
