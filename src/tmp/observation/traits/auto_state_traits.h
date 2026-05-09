// auto_state_traits.h
#ifndef AUTO_STATE_TRAITS_H
#define AUTO_STATE_TRAITS_H

#include "state_traits_base.h"
#include <string>
#include <utility>

namespace aurora::planner {

/**
 * @brief Auto mode feature indices (semantic names for 25-dimensional state)
 *
 * Auto mode state layout:
 * - [0-1]:     Position (normalized x, y)
 * - [2-17]:    Data density heatmap (16 cells from 4x4 grid)
 * - [18-21]:   Action history (last 4 actions)
 * - [22]:      Remaining budget
 * - [23]:      Local data density
 * - [24]:      Reachability score
 */
struct AutoStateTraits : public StateTraitsBase<AutoStateTraits> {
    // ===== Feature Index Constants =====
    static constexpr size_t STATE_DIM = 25;

    // Position features
    static constexpr size_t NORM_X = 0;
    static constexpr size_t NORM_Y = 1;

    // Heatmap features (16 cells from 4x4 grid around robot)
    static constexpr size_t HEATMAP_START = 2;
    static constexpr size_t HEATMAP_END = 17;
    static constexpr size_t HEATMAP_SIZE = 16;

    // Individual heatmap cells (optional, for fine-grained access)
    static constexpr size_t HEATMAP_00 = 2;   // Front-left
    static constexpr size_t HEATMAP_01 = 3;
    static constexpr size_t HEATMAP_02 = 4;
    static constexpr size_t HEATMAP_03 = 5;   // Front-right
    static constexpr size_t HEATMAP_10 = 6;
    static constexpr size_t HEATMAP_11 = 7;
    static constexpr size_t HEATMAP_12 = 8;
    static constexpr size_t HEATMAP_13 = 9;
    static constexpr size_t HEATMAP_20 = 10;
    static constexpr size_t HEATMAP_21 = 11;
    static constexpr size_t HEATMAP_22 = 12;
    static constexpr size_t HEATMAP_23 = 13;
    static constexpr size_t HEATMAP_30 = 14;  // Back-left
    static constexpr size_t HEATMAP_31 = 15;
    static constexpr size_t HEATMAP_32 = 16;
    static constexpr size_t HEATMAP_33 = 17;  // Back-right

    // Action history features
    static constexpr size_t ACTION_HISTORY_START = 18;
    static constexpr size_t ACTION_HISTORY_END = 21;
    static constexpr size_t ACTION_HISTORY_SIZE = 4;

    static constexpr size_t ACTION_0 = 18;  // Most recent
    static constexpr size_t ACTION_1 = 19;
    static constexpr size_t ACTION_2 = 20;
    static constexpr size_t ACTION_3 = 21;  // Oldest

    // Budget and density features
    static constexpr size_t REMAINING_BUDGET = 22;
    static constexpr size_t LOCAL_DENSITY = 23;

    // Reachability feature
    static constexpr size_t REACHABILITY = 24;

    // ===== Feature Group Definitions =====
    enum FeatureGroupIndex {
        POSITION_GROUP = 0,      // [0-1]
        HEATMAP_GROUP = 1,       // [2-17]
        HISTORY_GROUP = 2,       // [18-21]
        BUDGET_GROUP = 3,        // [22-23]
        REACHABILITY_GROUP = 4   // [24]
    };

    // ===== Normalization Parameters =====
    struct AutoNormalizationParams : public NormalizationParams {
        // Auto mode uses map coordinates
        double max_x = 50.0;
        double max_y = 50.0;
        double min_x = 0.0;
        double min_y = 0.0;

        // Budget is already normalized [0, 1]
        // Density is already normalized [0, 1]
        // Actions are discrete [0-3], normalized to [0, 1]
    };

    // ===== Static Methods =====

    /**
     * @brief Get feature name by index
     */
    static std::string getFeatureName(size_t index) {
        switch (index) {
            case NORM_X:         return "NORM_X";
            case NORM_Y:         return "NORM_Y";
            case HEATMAP_00:     return "HEATMAP_00";
            case HEATMAP_01:     return "HEATMAP_01";
            case HEATMAP_02:     return "HEATMAP_02";
            case HEATMAP_03:     return "HEATMAP_03";
            case HEATMAP_10:     return "HEATMAP_10";
            case HEATMAP_11:     return "HEATMAP_11";
            case HEATMAP_12:     return "HEATMAP_12";
            case HEATMAP_13:     return "HEATMAP_13";
            case HEATMAP_20:     return "HEATMAP_20";
            case HEATMAP_21:     return "HEATMAP_21";
            case HEATMAP_22:     return "HEATMAP_22";
            case HEATMAP_23:     return "HEATMAP_23";
            case HEATMAP_30:     return "HEATMAP_30";
            case HEATMAP_31:     return "HEATMAP_31";
            case HEATMAP_32:     return "HEATMAP_32";
            case HEATMAP_33:     return "HEATMAP_33";
            case ACTION_0:       return "ACTION_0";
            case ACTION_1:       return "ACTION_1";
            case ACTION_2:       return "ACTION_2";
            case ACTION_3:       return "ACTION_3";
            case REMAINING_BUDGET: return "REMAINING_BUDGET";
            case LOCAL_DENSITY:  return "LOCAL_DENSITY";
            case REACHABILITY:   return "REACHABILITY";
            default:             return "UNKNOWN_FEATURE_" + std::to_string(index);
        }
    }

    /**
     * @brief Get feature group by index
     */
    static FeatureGroup getFeatureGroup(size_t index) {
        if (index <= NORM_Y) return FeatureGroup::POSITION;
        if (index <= HEATMAP_END) return FeatureGroup::HEATMAP;
        if (index <= ACTION_HISTORY_END) return FeatureGroup::HISTORY;
        if (index <= LOCAL_DENSITY) return FeatureGroup::DATA_COLLECTION;
        if (index == REACHABILITY) return FeatureGroup::REACHABILITY;
        return FeatureGroup::UNKNOWN;
    }

    /**
     * @brief Get feature index range for a group
     */
    static std::pair<size_t, size_t> getFeatureGroupRange(FeatureGroup group) {
        switch (group) {
            case FeatureGroup::POSITION:
                return {NORM_X, NORM_Y};
            case FeatureGroup::HEATMAP:
                return {HEATMAP_START, HEATMAP_END};
            case FeatureGroup::HISTORY:
                return {ACTION_HISTORY_START, ACTION_HISTORY_END};
            case FeatureGroup::DATA_COLLECTION:
                return {REMAINING_BUDGET, LOCAL_DENSITY};
            case FeatureGroup::REACHABILITY:
                return {REACHABILITY, REACHABILITY};
            default:
                return {0, 0};
        }
    }

    /**
     * @brief Get normalization parameters
     */
    static const AutoNormalizationParams& getNormalizationParams() {
        static AutoNormalizationParams params;
        return params;
    }

    /**
     * @brief Get mode name
     */
    static std::string getModeName() {
        return "AUTO";
    }

    /**
     * @brief Validate state dimension at compile time
     */
    static_assert(STATE_DIM == 25, "Auto state must be 25-dimensional");
};

} // namespace aurora::planner

#endif // AUTO_STATE_TRAITS_H
