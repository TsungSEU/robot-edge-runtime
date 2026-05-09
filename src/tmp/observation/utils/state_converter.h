// state_converter.h
#ifndef STATE_CONVERTER_H
#define STATE_CONVERTER_H

#include "../state_base.h"
#include "../traits/auto_state_traits.h"
#include "../traits/humanoid_state_traits.h"
#include "state_normalizer.h"
#include <vector>
#include <stdexcept>


namespace aurora::planner {
    struct HumanoidStateInfo;
}

namespace aurora::planner {

/**
 * @brief Converter for new State representations
 */
class StateConverter {
public:

    /**
     * @brief Build HumanoidState from HumanoidStateInfo
     *
     * This replaces the old HumanoidState::fromStateInfo() method.
     * Uses the new State<HumanoidStateTraits> type.
     *
     * @param info HumanoidStateInfo with raw (unnormalized) data
     * @return State<HumanoidStateTraits> Normalized state
     */
    static State<HumanoidStateTraits> fromStateInfo(const HumanoidStateInfo& info);
};

} // namespace aurora::planner

#endif // STATE_CONVERTER_H
