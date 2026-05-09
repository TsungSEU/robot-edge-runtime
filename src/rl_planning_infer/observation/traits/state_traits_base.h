// state_traits_base.h
#ifndef STATE_TRAITS_BASE_H
#define STATE_TRAITS_BASE_H

#include <cstddef>
#include <array>
#include <string>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_4
#define M_PI_4 0.78539816339744830962
#endif

namespace aurora::planner {

/**
 * @brief Feature group identifiers for organized state access
 *
 * States are organized into logical groups for semantic access
 * rather than raw index-based access.
 */
enum class FeatureGroup {
    POSITION,           // Position and orientation
    JOINTS,             // Joint positions and velocities
    MOTION,             // Velocities and accelerations
    GAIT,               // Gait phase and timing
    SYSTEM,             // Battery, CPU, etc.
    ENVIRONMENT,        // Terrain, obstacles, lighting
    DATA_COLLECTION,    // Density, coverage, rewards
    HISTORY,            // Past actions
    HEATMAP,            // Data density heatmap
    REACHABILITY,       // Execution-aware reachability
    UNKNOWN
};

/**
 * @brief Normalization parameters for state features
 *
 * Defines the min/max ranges for normalizing raw values to [-1, 1] or [0, 1].
 */
struct NormalizationParams {
    // Position ranges (meters)
    double max_x = 50.0;
    double max_y = 50.0;
    double min_x = -50.0;
    double min_y = -50.0;

    // Joint ranges (radians)
    double max_joint_pos = M_PI;
    double min_joint_pos = -M_PI;
    double max_joint_vel = 10.0;
    double min_joint_vel = -10.0;

    // Motion ranges
    double max_linear_vel = 2.0;
    double max_angular_vel = M_PI;
    double max_acceleration = 5.0;

    // Center of mass ranges
    double max_com_offset = 0.1;
    double max_com_height = 1.0;

    // Gait ranges
    double max_step_freq = 3.0;
    double max_step_length = 0.6;

    // System ranges
    double max_temperature = 80.0;
    double max_operation_time = 3600;

    // Environment ranges
    double max_obstacle_distance = 10.0;
    double max_terrain_slope = M_PI_4;

    virtual ~NormalizationParams() = default;
};

/**
 * @brief Base class for State traits
 *
 * Each mode (Auto, Humanoid) specializes this template to define:
 * - State dimension (compile-time constant)
 * - Feature indices and names
 * - Normalization parameters
 * - Feature group boundaries
 *
 * @tparam Derived The derived traits class (CRTP pattern)
 */
template<typename Derived>
class StateTraitsBase {
public:
    /**
     * @brief Total state dimension (must be defined by derived class)
     */
    static constexpr size_t STATE_DIM = Derived::STATE_DIM;

    /**
     * @brief Get feature name by index
     */
    static std::string getFeatureName(size_t index) {
        return Derived::getFeatureName(index);
    }

    /**
     * @brief Get feature group by index
     */
    static FeatureGroup getFeatureGroup(size_t index) {
        return Derived::getFeatureGroup(index);
    }

    /**
     * @brief Get feature index range for a group
     */
    static std::pair<size_t, size_t> getFeatureGroupRange(FeatureGroup group) {
        return Derived::getFeatureGroupRange(group);
    }

    /**
     * @brief Get normalization parameters
     */
    static const NormalizationParams& getNormalizationParams() {
        return Derived::getNormalizationParams();
    }

    /**
     * @brief Validate feature index
     */
    static bool isValidIndex(size_t index) {
        return index < STATE_DIM;
    }

    /**
     * @brief Get state mode name (for debugging)
     */
    static std::string getModeName() {
        return Derived::getModeName();
    }
};

} // namespace aurora::planner

#endif // STATE_TRAITS_BASE_H
