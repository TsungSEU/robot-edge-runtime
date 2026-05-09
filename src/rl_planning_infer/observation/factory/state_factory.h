// state_factory.h
#ifndef STATE_FACTORY_H
#define STATE_FACTORY_H

#include "state_base.h"
#include "traits/auto_state_traits.h"
#include "traits/humanoid_state_traits.h"
#include "utils/state_converter.h"
#include <memory>
#include <string>
#include <stdexcept>

namespace aurora::planner {

/**
 * @brief State mode enumeration
 */
enum class StateMode {
    AUTO,       // 25-dimensional Auto mode
    HUMANOID    // 75-dimensional Humanoid mode
};

/**
 * @brief State type erasure interface for runtime polymorphism
 *
 * Allows storing states of different types (Auto vs Humanoid) in the same container.
 */
class IStateInterface {
public:
    virtual ~IStateInterface() = default;

    /**
     * @brief Get state dimension
     */
    virtual size_t size() const = 0;

    /**
     * @brief Get mode name
     */
    virtual std::string getModeName() const = 0;

    /**
     * @brief Get feature at index
     */
    virtual double getFeature(size_t index) const = 0;

    /**
     * @brief Set feature at index
     */
    virtual void setFeature(size_t index, double value) = 0;

    /**
     * @brief Get raw data pointer (for ONNX)
     */
    virtual const double* data() const = 0;

    /**
     * @brief Check if state is valid
     */
    virtual bool isValid() const = 0;

    /**
     * @brief Clone the state
     */
    virtual std::unique_ptr<IStateInterface> clone() const = 0;
};

/**
 * @brief Template implementation of state interface
 */
template<typename Traits>
class StateInterfaceImpl : public IStateInterface {
public:
    explicit StateInterfaceImpl(const State<Traits>& state) : state_(state) {}
    explicit StateInterfaceImpl() : state_() {}

    size_t size() const override { return state_.size(); }
    std::string getModeName() const override { return Traits::getModeName(); }
    double getFeature(size_t index) const override { return state_[index]; }
    void setFeature(size_t index, double value) override { state_.setFeature(index, value); }
    const double* data() const override { return state_.data(); }
    bool isValid() const override { return state_.isValid(); }

    std::unique_ptr<IStateInterface> clone() const override {
        return std::make_unique<StateInterfaceImpl<Traits>>(state_);
    }

    /**
     * @brief Get underlying state
     */
    const State<Traits>& getState() const { return state_; }
    State<Traits>& getState() { return state_; }

private:
    State<Traits> state_;
};

/**
 * @brief Factory for creating State objects at runtime
 *
 * Provides runtime creation of State objects based on configuration or mode detection.
 * Supports both compile-time type safety (via templates) and runtime polymorphism (via interface).
 *
 * NOTE: Legacy State conversion methods have been removed in Phase 6 v2.0
 * The system now uses only State<Traits> templates.
 */
class StateFactory {
public:
    /**
     * @brief Create a state with the specified mode
     *
     * @param mode State mode (AUTO or HUMANOID)
     * @return Unique pointer to state interface
     */
    static std::unique_ptr<IStateInterface> create(StateMode mode) {
        switch (mode) {
            case StateMode::AUTO:
                return std::make_unique<StateInterfaceImpl<AutoStateTraits>>();
            case StateMode::HUMANOID:
                return std::make_unique<StateInterfaceImpl<HumanoidStateTraits>>();
            default:
                throw std::invalid_argument("Unknown state mode");
        }
    }

    /**
     * @brief Create a state from mode name string
     *
     * @param mode_name Mode name ("auto" or "humanoid", case-insensitive)
     * @return Unique pointer to state interface
     */
    static std::unique_ptr<IStateInterface> create(const std::string& mode_name) {
        std::string mode_lower = mode_name;
        std::transform(mode_lower.begin(), mode_lower.end(),
                      mode_lower.begin(), ::tolower);

        if (mode_lower == "auto") {
            return create(StateMode::AUTO);
        } else if (mode_lower == "humanoid") {
            return create(StateMode::HUMANOID);
        } else {
            throw std::invalid_argument(
                "Unknown mode name: '" + mode_name + "'. Expected 'auto' or 'humanoid'"
            );
        }
    }

    /**
     * @brief Detect state mode from configuration file
     *
     * Reads the planner mode from config file and returns appropriate state.
     *
     * @param config_path Path to configuration file (YAML)
     * @return Unique pointer to state interface
     * @throws std::runtime_error if config cannot be read or mode is invalid
     */
    static std::unique_ptr<IStateInterface> createFromConfig(const std::string& config_path) {
        // Try to detect mode from config file
        StateMode mode = detectModeFromConfig(config_path);
        return create(mode);
    }

    /**
     * @brief Detect state mode from configuration file
     *
     * @param config_path Path to configuration file (YAML)
     * @return Detected state mode
     * @throws std::runtime_error if config cannot be read or mode is invalid
     */
    static StateMode detectModeFromConfig(const std::string& config_path) {
        // For now, simple heuristic based on config file name
        // TODO: Parse actual YAML configuration

        std::string config_lower = config_path;
        std::transform(config_lower.begin(), config_lower.end(),
                      config_lower.begin(), ::tolower);

        if (config_lower.find("auto") != std::string::npos) {
            return StateMode::AUTO;
        } else if (config_lower.find("humanoid") != std::string::npos) {
            return StateMode::HUMANOID;
        }

        // Default to humanoid if uncertain
        return StateMode::HUMANOID;
    }

    /**
     * @brief Detect state mode from model file name
     *
     * @param model_path Path to ONNX model file
     * @return Detected state mode
     */
    static StateMode detectModeFromModel(const std::string& model_path) {
        std::string model_lower = model_path;
        std::transform(model_lower.begin(), model_lower.end(),
                      model_lower.begin(), ::tolower);

        if (model_lower.find("auto") != std::string::npos) {
            return StateMode::AUTO;
        } else if (model_lower.find("humanoid") != std::string::npos) {
            return StateMode::HUMANOID;
        }

        // Try to detect from expected input dimension
        // This would require loading the ONNX model and checking input shape
        // For now, default to humanoid
        return StateMode::HUMANOID;
    }

    /**
     * @brief Convert mode enum to string
     */
    static std::string modeToString(StateMode mode) {
        switch (mode) {
            case StateMode::AUTO: return "auto";
            case StateMode::HUMANOID: return "humanoid";
            default: return "unknown";
        }
    }

    /**
     * @brief Convert string to mode enum
     */
    static StateMode stringToMode(const std::string& mode_str) {
        std::string mode_lower = mode_str;
        std::transform(mode_lower.begin(), mode_lower.end(),
                      mode_lower.begin(), ::tolower);

        if (mode_lower == "auto") {
            return StateMode::AUTO;
        } else if (mode_lower == "humanoid") {
            return StateMode::HUMANOID;
        }

        throw std::invalid_argument("Invalid mode string: " + mode_str);
    }

    /**
     * @brief Get expected state dimension for a mode
     */
    static size_t getDimensionForMode(StateMode mode) {
        switch (mode) {
            case StateMode::AUTO:
                return AutoStateTraits::STATE_DIM;
            case StateMode::HUMANOID:
                return HumanoidStateTraits::STATE_DIM;
            default:
                return 0;
        }
    }

    /**
     * @brief Create AutoState with specific features
     *
     * Convenience method for creating AutoState with pre-set features.
     *
     * @param norm_x Normalized X coordinate
     * @param norm_y Normalized Y coordinate
     * @param heatmap 16-element heatmap data
     * @param actions 4-element action history
     * @param budget Remaining budget
     * @param density Local density
     * @param reachability Reachability score
     * @return Configured AutoState
     */
    static State<AutoStateTraits> createAutoState(
        double norm_x, double norm_y,
        const std::vector<double>& heatmap,
        const std::vector<double>& actions,
        double budget, double density,
        double reachability = 0.5
    ) {
        State<AutoStateTraits> state;

        state.setFeature(AutoStateTraits::NORM_X, norm_x);
        state.setFeature(AutoStateTraits::NORM_Y, norm_y);

        // Heatmap
        for (size_t i = 0; i < 16 && i < heatmap.size(); ++i) {
            state.setFeature(AutoStateTraits::HEATMAP_START + i, heatmap[i]);
        }

        // Actions
        for (size_t i = 0; i < 4 && i < actions.size(); ++i) {
            state.setFeature(AutoStateTraits::ACTION_0 + i, actions[i]);
        }

        state.setFeature(AutoStateTraits::REMAINING_BUDGET, budget);
        state.setFeature(AutoStateTraits::LOCAL_DENSITY, density);
        state.setFeature(AutoStateTraits::REACHABILITY, reachability);

        return state;
    }

    /**
     * @brief Create zero-initialized AutoState
     */
    static State<AutoStateTraits> createAutoState() {
        return State<AutoStateTraits>();
    }

    /**
     * @brief Create zero-initialized HumanoidState
     */
    static State<HumanoidStateTraits> createHumanoidState() {
        return State<HumanoidStateTraits>();
    }

    /**
     * @brief Create HumanoidState from HumanoidStateInfo
     *
     * Convenience wrapper around StateConverter::fromStateInfo.
     *
     * @param info HumanoidStateInfo with raw data
     * @return Normalized HumanoidState
     */
    static State<HumanoidStateTraits> createHumanoidStateFromInfo(
        const HumanoidStateInfo& info
    ) {
        return StateConverter::fromStateInfo(info);
    }
};

} // namespace aurora::planner

#endif // STATE_FACTORY_H
