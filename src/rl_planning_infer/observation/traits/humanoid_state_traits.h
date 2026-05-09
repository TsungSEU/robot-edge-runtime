// humanoid_state_traits.h
// Humanoid mode feature indices (43-dimensional state)
#ifndef HUMANOID_STATE_TRAITS_H
#define HUMANOID_STATE_TRAITS_H

#include "state_traits_base.h"
#include <string>
#include <utility>

namespace aurora::planner {

/**
 * @brief Humanoid mode feature indices (43-dimensional state)
 *
 * State layout (aligned with training side humanoid_nav_data_training.yaml):
 *  [0-2]:   base_lin_vel [vx, vy, vz]
 *  [3-5]:   base_ang_vel [wx, wy, wz]
 *  [6-7]:   norm_position [x/W, y/H]
 *  [8-9]:   heading [sin, cos]
 *  [10-11]: goal_direction [sin, cos]
 *  [12-14]: goal_distance [dx, dy, dist]
 *  [15-22]: data_value_sectors (8 directions)
 *  [23-26]: obstacle_sectors (4 directions)
 *  [27-28]: current_value [value, rarity]
 *  [29-30]: collection_status [ratio, coverage]
 *  [31]:    terrain_type
 *  [32]:    obstacle_density
 *  [33]:    gait_phase
 *  [34-41]: action_history (8 steps)
 *  [42]:    remaining_budget
 */
struct HumanoidStateTraits : public StateTraitsBase<HumanoidStateTraits> {
    static constexpr size_t STATE_DIM = 43;

    // Base linear velocity [0-2]
    static constexpr size_t LIN_VEL_X = 0;
    static constexpr size_t LIN_VEL_Y = 1;
    static constexpr size_t LIN_VEL_Z = 2;

    // Base angular velocity [3-5]
    static constexpr size_t ANG_VEL_X = 3;
    static constexpr size_t ANG_VEL_Y = 4;
    static constexpr size_t ANG_VEL_Z = 5;

    // Normalized position [6-7]
    static constexpr size_t NORM_X = 6;
    static constexpr size_t NORM_Y = 7;

    // Heading [8-9]
    static constexpr size_t HEADING_SIN = 8;
    static constexpr size_t HEADING_COS = 9;

    // Goal direction [10-11]
    static constexpr size_t GOAL_DIR_SIN = 10;
    static constexpr size_t GOAL_DIR_COS = 11;

    // Goal distance [12-14]
    static constexpr size_t GOAL_DX = 12;
    static constexpr size_t GOAL_DY = 13;
    static constexpr size_t GOAL_DIST = 14;

    // Data value sectors [15-22]
    static constexpr size_t DATA_VALUE_START = 15;
    static constexpr size_t DATA_VALUE_END = 22;

    // Obstacle sectors [23-26]
    static constexpr size_t OBSTACLE_START = 23;
    static constexpr size_t OBSTACLE_END = 26;

    // Current value [27-28]
    static constexpr size_t CURRENT_VALUE = 27;
    static constexpr size_t CURRENT_RARITY = 28;

    // Collection status [29-30]
    static constexpr size_t COLLECTED_RATIO = 29;
    static constexpr size_t COVERAGE_RATIO = 30;

    // Environment [31-32]
    static constexpr size_t TERRAIN_TYPE = 31;
    static constexpr size_t OBSTACLE_DENSITY = 32;

    // Gait phase [33]
    static constexpr size_t GAIT_PHASE = 33;

    // Action history [34-41]
    static constexpr size_t ACTION_HISTORY_START = 34;
    static constexpr size_t ACTION_HISTORY_END = 41;

    // Budget [42]
    static constexpr size_t REMAINING_BUDGET = 42;

    // ===== Static Methods =====

    static std::string getFeatureName(size_t index) {
        if (index <= 2) {
            const char* names[] = {"LIN_VEL_X", "LIN_VEL_Y", "LIN_VEL_Z"};
            return names[index];
        }
        if (index <= 5) {
            const char* names[] = {"ANG_VEL_X", "ANG_VEL_Y", "ANG_VEL_Z"};
            return names[index - 3];
        }
        switch (index) {
            case 6:  return "NORM_X";
            case 7:  return "NORM_Y";
            case 8:  return "HEADING_SIN";
            case 9:  return "HEADING_COS";
            case 10: return "GOAL_DIR_SIN";
            case 11: return "GOAL_DIR_COS";
            case 12: return "GOAL_DX";
            case 13: return "GOAL_DY";
            case 14: return "GOAL_DIST";
            case 27: return "CURRENT_VALUE";
            case 28: return "CURRENT_RARITY";
            case 29: return "COLLECTED_RATIO";
            case 30: return "COVERAGE_RATIO";
            case 31: return "TERRAIN_TYPE";
            case 32: return "OBSTACLE_DENSITY";
            case 33: return "GAIT_PHASE";
            case 42: return "REMAINING_BUDGET";
            default:
                if (index >= 15 && index <= 22)
                    return "DATA_VALUE_" + std::to_string(index - 15);
                if (index >= 23 && index <= 26)
                    return "OBSTACLE_" + std::to_string(index - 23);
                if (index >= 34 && index <= 41)
                    return "ACTION_HIST_" + std::to_string(index - 34);
                return "UNKNOWN_" + std::to_string(index);
        }
    }

    static FeatureGroup getFeatureGroup(size_t index) {
        if (index <= 5)  return FeatureGroup::MOTION;
        if (index <= 9)  return FeatureGroup::POSITION;
        if (index <= 14) return FeatureGroup::DATA_COLLECTION;
        if (index <= 22) return FeatureGroup::HEATMAP;
        if (index <= 26) return FeatureGroup::ENVIRONMENT;
        if (index <= 30) return FeatureGroup::DATA_COLLECTION;
        if (index <= 32) return FeatureGroup::ENVIRONMENT;
        if (index == 33) return FeatureGroup::GAIT;
        if (index <= 41) return FeatureGroup::HISTORY;
        if (index == 42) return FeatureGroup::SYSTEM;
        return FeatureGroup::UNKNOWN;
    }

    static std::pair<size_t, size_t> getFeatureGroupRange(FeatureGroup group) {
        switch (group) {
            case FeatureGroup::MOTION:         return {0, 5};
            case FeatureGroup::POSITION:       return {6, 11};
            case FeatureGroup::HEATMAP:        return {DATA_VALUE_START, DATA_VALUE_END};
            case FeatureGroup::ENVIRONMENT:    return {OBSTACLE_START, OBSTACLE_DENSITY};
            case FeatureGroup::DATA_COLLECTION:return {CURRENT_VALUE, COVERAGE_RATIO};
            case FeatureGroup::HISTORY:        return {ACTION_HISTORY_START, ACTION_HISTORY_END};
            case FeatureGroup::SYSTEM:         return {REMAINING_BUDGET, REMAINING_BUDGET};
            default: return {0, 0};
        }
    }

    struct HumanoidNormalizationParams : public NormalizationParams {
        double map_width = 40.0;
        double map_height = 40.0;
        double max_range = 10.0;
    };

    static const HumanoidNormalizationParams& getNormalizationParams() {
        static HumanoidNormalizationParams params;
        return params;
    }

    static std::string getModeName() { return "HUMANOID"; }

    static_assert(STATE_DIM == 43, "Humanoid state must be 43-dimensional");
};

} // namespace aurora::planner

#endif // HUMANOID_STATE_TRAITS_H
