//
// Created by xucong on 25-2-10.
// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#include "trigger_base.h"
#include "trigger_manager.h"

namespace aurora::collector {

bool TriggerBase::init(const std::string& triggerId, const StrategyConfig& strategyConfig) {
    for (const auto& st : strategyConfig.strategies) {
        if (st.trigger.triggerId == triggerId) {
            trigger_obj_ = std::make_unique<Trigger>(st.trigger);
            break;
        }
    }

    if (!trigger_obj_) {
        AD_ERROR(TriggerBase, "Trigger object not found for trigger ID: %s", triggerId.c_str());
        return false;
    }

    return true;
}

void TriggerBase::notifyTrigger(const TriggerContext& context) const {
    if (trigger_manager_) {
        trigger_manager_->notifyTriggerContext(context);
    } else {
        AD_ERROR(TriggerBase, "Factory pointer is null when notifying trigger context");
    }
}

}
