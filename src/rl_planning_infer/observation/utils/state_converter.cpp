// state_converter.cpp
#include "state_converter.h"
#include "rl_planning_infer/agents/humanoid_state.h"

namespace aurora::planner {

State<HumanoidStateTraits> StateConverter::fromStateInfo(const HumanoidStateInfo& info) {
    // Delegate to HumanoidState::fromStateInfo which handles the 43-dim conversion
    HumanoidState normalized = HumanoidState::fromStateInfo(info);

    State<HumanoidStateTraits> state;
    for (size_t i = 0; i < HumanoidStateTraits::STATE_DIM; ++i) {
        state.setFeature(i, normalized[i]);
    }
    return state;
}

} // namespace aurora::planner
