// state_normalizer.h
#ifndef STATE_NORMALIZER_H
#define STATE_NORMALIZER_H

#include <cmath>
#include <algorithm>
#include <array>

namespace aurora::planner {

/**
 * @brief State normalization utilities
 *
 * Provides static methods for normalizing raw values to [-1, 1] or [0, 1] ranges.
 * Migrated from humanoid_state.cpp for reuse across different state types.
 */
class StateNormalizer {
public:
    /**
     * @brief Normalize a value to [0, 1] range
     *
     * @param value The raw value to normalize
     * @param max Maximum value (maps to 1.0)
     * @param min Minimum value (maps to 0.0)
     * @return Normalized value in [0, 1]
     */
    static double normalize(double value, double max, double min = 0.0) {
        constexpr double epsilon = 1e-8;
        return (value - min) / (max - min + epsilon);
    }

    /**
     * @brief Normalize a symmetric value to [-1, 1] range
     *
     * @param value The raw value to normalize (assumed symmetric around 0)
     * @param max Maximum absolute value (maps to 1.0 or -1.0)
     * @return Normalized value in [-1, 1]
     */
    static double normalizeSymmetric(double value, double max) {
        constexpr double epsilon = 1e-8;
        double normalized = value / (max + epsilon);
        return std::max(-1.0, std::min(1.0, normalized));
    }

    /**
     * @brief Denormalize a value from [0, 1] to original range
     *
     * @param norm_value Normalized value in [0, 1]
     * @param max Maximum value
     * @param min Minimum value
     * @return Denormalized value
     */
    static double denormalize(double norm_value, double max, double min = 0.0) {
        return norm_value * (max - min) + min;
    }

    /**
     * @brief Denormalize a symmetric value from [-1, 1] to original range
     *
     * @param norm_value Normalized value in [-1, 1]
     * @param max Maximum absolute value
     * @return Denormalized value
     */
    static double denormalizeSymmetric(double norm_value, double max) {
        return norm_value * max;
    }

    /**
     * @brief Normalize an angle to [-1, 1] range
     *
     * Maps angle in radians to [-1, 1] where:
     * - -π maps to -1
     * - 0 maps to 0
     * - π maps to 1
     *
     * @param angle Angle in radians
     * @return Normalized angle in [-1, 1]
     */
    static double normalizeAngle(double angle) {
        // Normalize to [-π, π]
        while (angle > M_PI) angle -= 2 * M_PI;
        while (angle < -M_PI) angle += 2 * M_PI;

        // Map to [-1, 1]
        return angle / M_PI;
    }

    /**
     * @brief Denormalize an angle from [-1, 1] to radians
     *
     * @param norm_angle Normalized angle in [-1, 1]
     * @return Angle in radians
     */
    static double denormalizeAngle(double norm_angle) {
        return norm_angle * M_PI;
    }

    /**
     * @brief Clamp a value to a specified range
     *
     * @param value Value to clamp
     * @param min Minimum value
     * @param max Maximum value
     * @return Clamped value
     */
    static double clamp(double value, double min, double max) {
        return std::max(min, std::min(max, value));
    }

    /**
     * @brief Normalize an array of values
     *
     * @param values Input array of values
     * @param max Maximum value for normalization
     * @param min Minimum value for normalization
     * @return Normalized array
     */
    template<size_t N>
    static std::array<double, N> normalizeArray(
        const std::array<double, N>& values,
        double max,
        double min = 0.0
    ) {
        std::array<double, N> normalized;
        for (size_t i = 0; i < N; ++i) {
            normalized[i] = normalize(values[i], max, min);
        }
        return normalized;
    }

    /**
     * @brief Normalize a vector of symmetric values
     *
     * @param values Input vector of values
     * @param max Maximum absolute value for normalization
     * @return Normalized vector
     */
    static std::vector<double> normalizeSymmetricVector(
        const std::vector<double>& values,
        double max
    ) {
        std::vector<double> normalized(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            normalized[i] = normalizeSymmetric(values[i], max);
        }
        return normalized;
    }

    /**
     * @brief Normalize an angle vector
     *
     * @param angles Input vector of angles in radians
     * @return Normalized angle vector
     */
    static std::vector<double> normalizeAngleVector(
        const std::vector<double>& angles
    ) {
        std::vector<double> normalized(angles.size());
        for (size_t i = 0; i < angles.size(); ++i) {
            normalized[i] = normalizeAngle(angles[i]);
        }
        return normalized;
    }
};

} // namespace aurora::planner

#endif // STATE_NORMALIZER_H
