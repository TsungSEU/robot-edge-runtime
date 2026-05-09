// state_base.h
#ifndef STATE_BASE_H
#define STATE_BASE_H

#include "traits/state_traits_base.h"
#include "traits/auto_state_traits.h"
#include "traits/humanoid_state_traits.h"
#include <array>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace aurora::planner {

/**
 * @brief Type-safe, zero-overhead state representation
 *
 * This template provides:
 * - Compile-time type safety (dimension checking at compile time)
 * - Zero abstraction overhead (stack allocation, no indirection)
 * - Semantic feature access (using named constants instead of magic numbers)
 * - Zero-copy ONNX integration (direct pointer access)
 *
 * @tparam Traits Traits class defining state dimension and feature layout
 *
 * Example usage:
 * @code
 * using AutoState = State<AutoStateTraits>;
 * AutoState state;
 * state.setFeature(AutoStateTraits::NORM_X, 0.5);
 * state.setFeature(AutoStateTraits::NORM_Y, 0.3);
 * double x = state[AutoStateTraits::NORM_X];
 *
 * // Zero-copy ONNX inference
 * ppo_agent->selectAction(state);  // Passes state.data() directly
 * @endcode
 */
template<typename Traits>
class State {
public:
    // ===== Type Definitions =====
    static constexpr size_t STATE_DIM = Traits::STATE_DIM;
    using FeaturesArray = std::array<double, STATE_DIM>;
    using TraitsType = Traits;

    // ===== Constructors =====

    /**
     * @brief Default constructor - initializes all features to 0
     */
    State() : features_{}, feature_count_(STATE_DIM) {
        features_.fill(0.0);
    }

    /**
     * @brief Copy constructor
     */
    State(const State& other) = default;

    /**
     * @brief Move constructor
     */
    State(State&& other) noexcept = default;

    /**
     * @brief Copy assignment
     */
    State& operator=(const State& other) = default;

    /**
     * @brief Move assignment
     */
    State& operator=(State&& other) noexcept = default;

    /**
     * @brief Destructor
     */
    ~State() = default;

    // ===== Feature Access =====

    /**
     * @brief Access feature by index (const)
     *
     * @param index Feature index [0, STATE_DIM)
     * @return Feature value
     * @throws std::out_of_range if index is invalid
     */
    double operator[](size_t index) const {
        if (index >= STATE_DIM) {
            throw std::out_of_range("Feature index " + std::to_string(index) +
                                   " out of range [0, " + std::to_string(STATE_DIM) + ")");
        }
        return features_[index];
    }

    /**
     * @brief Access feature by index (non-const)
     *
     * @param index Feature index [0, STATE_DIM)
     * @return Reference to feature value
     * @throws std::out_of_range if index is invalid
     */
    double& operator[](size_t index) {
        if (index >= STATE_DIM) {
            throw std::out_of_range("Feature index " + std::to_string(index) +
                                   " out of range [0, " + std::to_string(STATE_DIM) + ")");
        }
        return features_[index];
    }

    /**
     * @brief Compile-time indexed feature access (zero overhead)
     *
     * This is the preferred method for performance-critical code.
     * The compiler can fully optimize this and eliminate bounds checking.
     *
     * @tparam Index Compile-time constant index
     * @return Feature value
     */
    template<int Index>
    double get() const noexcept {
        static_assert(Index >= 0 && Index < STATE_DIM,
                     "Feature index out of bounds at compile time");
        return features_[Index];
    }

    /**
     * @brief Compile-time indexed feature set (zero overhead)
     *
     * @tparam Index Compile-time constant index
     * @param value Feature value to set
     */
    template<int Index>
    void set(double value) noexcept {
        static_assert(Index >= 0 && Index < STATE_DIM,
                     "Feature index out of bounds at compile time");
        features_[Index] = value;
    }

    /**
     * @brief Runtime feature set with bounds checking
     *
     * @param index Feature index [0, STATE_DIM)
     * @param value Feature value to set
     */
    void setFeature(size_t index, double value) {
        if (index >= STATE_DIM) {
            throw std::out_of_range("Feature index " + std::to_string(index) +
                                   " out of range [0, " + std::to_string(STATE_DIM) + ")");
        }
        features_[index] = value;
    }

    /**
     * @brief Runtime feature get with bounds checking
     *
     * @param index Feature index [0, STATE_DIM)
     * @return Feature value
     */
    double getFeature(size_t index) const {
        return (*this)[index];
    }

    // ===== Feature Group Access =====

    /**
     * @brief Get all features in a group
     *
     * @param group Feature group identifier
     * @return Vector of feature values in the group
     */
    std::vector<double> getGroup(FeatureGroup group) const {
        auto range = Traits::getFeatureGroupRange(group);
        std::vector<double> result;

        for (size_t i = range.first; i <= range.second && i < STATE_DIM; ++i) {
            result.push_back(features_[i]);
        }

        return result;
    }

    /**
     * @brief Set all features in a group
     *
     * @param group Feature group identifier
     * @param values Vector of feature values (must match group size)
     */
    void setGroup(FeatureGroup group, const std::vector<double>& values) {
        auto range = Traits::getFeatureGroupRange(group);
        size_t expected_size = range.second - range.first + 1;

        if (values.size() != expected_size) {
            throw std::invalid_argument("Group " + std::to_string(static_cast<int>(group)) +
                                      " expects " + std::to_string(expected_size) +
                                      " values, got " + std::to_string(values.size()));
        }

        for (size_t i = 0; i < values.size(); ++i) {
            features_[range.first + i] = values[i];
        }
    }

    // ===== Utility Methods =====

    /**
     * @brief Get state dimension
     */
    static constexpr size_t size() noexcept {
        return STATE_DIM;
    }

    /**
     * @brief Get actual feature count (may be less than STATE_DIM)
     */
    size_t getFeatureCount() const noexcept {
        return feature_count_;
    }

    /**
     * @brief Set actual feature count
     */
    void setFeatureCount(size_t count) noexcept {
        feature_count_ = std::min(count, STATE_DIM);
    }

    /**
     * @brief Clear all features to 0
     */
    void clear() noexcept {
        features_.fill(0.0);
        feature_count_ = 0;
    }

    /**
     * @brief Check if state is valid (no NaN or Inf)
     */
    bool isValid() const noexcept {
        for (size_t i = 0; i < STATE_DIM; ++i) {
            if (std::isnan(features_[i]) || std::isinf(features_[i])) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get feature name by index
     */
    std::string getFeatureName(size_t index) const {
        return Traits::getFeatureName(index);
    }

    /**
     * @brief Get mode name
     */
    static std::string getModeName() {
        return Traits::getModeName();
    }

    /**
     * @brief Convert to string (for debugging)
     */
    std::string toString() const {
        std::string result = "State<" + Traits::getModeName() + ">[" + std::to_string(STATE_DIM) + "]{";

        // Show first few features
        size_t show_count = std::min(size_t(5), STATE_DIM);
        for (size_t i = 0; i < show_count; ++i) {
            result += Traits::getFeatureName(i) + "=" + std::to_string(features_[i]);
            if (i < show_count - 1) result += ", ";
        }

        if (STATE_DIM > show_count) {
            result += ", ...";
        }

        result += "}";
        return result;
    }

    // ===== ONNX Integration (Zero-Copy) =====

    /**
     * @brief Get direct pointer to underlying data array
     *
     * This enables zero-copy integration with ONNX Runtime.
     * The returned pointer can be passed directly to ONNX inference.
     *
     * @return Const pointer to feature array
     */
    const double* data() const noexcept {
        return features_.data();
    }

    /**
     * @brief Get mutable pointer to underlying data array
     *
     * @return Mutable pointer to feature array
     */
    double* data() noexcept {
        return features_.data();
    }

    /**
     * @brief Get size in bytes
     */
    static constexpr size_t byteSize() noexcept {
        return STATE_DIM * sizeof(double);
    }

    // ===== Comparison Operators =====

    bool operator==(const State& other) const {
        return features_ == other.features_;
    }

    bool operator!=(const State& other) const {
        return features_ != other.features_;
    }

private:
    FeaturesArray features_;      // Stack-allocated feature array
    size_t feature_count_;        // Actual number of features used
};

} // namespace aurora::planner

#endif // STATE_BASE_H

