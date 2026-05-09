//
// Created by xucong on 26-2-2.
//

#ifndef ITRIGGER_H
#define ITRIGGER_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "rl_planning_infer/maps/costmap.h"  // For Point

namespace aurora::collector {

/**
 * @brief Internal trigger context for trigger system communication
 *
 * USAGE SCOPE (Internal Only):
 * - Used within trigger system (GaitTrigger, RuleTrigger, TriggerManager)
 * - For trigger state tracking and notification
 * - NOT for external recording triggers (use ROS2 /robot/trigger service instead)
 *
 * This struct is an internal data structure for the trigger subsystem.
 * For triggering data recording, all components MUST use the ROS2 service:
 *   - Service: /robot/trigger
 *   - Type: aurora_edge_runtime::srv::TriggerRecording
 *
 * Architecture:
 *   TriggerContext  -> Internal (trigger state tracking)
 *   Service Request  -> External (actual recording trigger)
 */
struct TriggerContext {
    std::string businessType;
    std::string triggerId;
    uint64_t triggerTimestamp;
    std::string triggerDesc;
    Point pos;
};

}

#endif //ITRIGGER_H
