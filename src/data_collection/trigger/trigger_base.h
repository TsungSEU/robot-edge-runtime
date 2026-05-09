//
// Created by xucong on 25-2-10.
// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#ifndef TRIGGER_BASE_H
#define TRIGGER_BASE_H

#include <string>
#include <memory>

#include "common/log/logger.h"
#include "strategy/strategy_config.h"
#include "trigger/ITrigger.h"
#include "common/trigger_checker.h"
#include "channel/observer.h"
#include "state_machine/state_machine.h"

namespace aurora::collector {

class TriggerManager;

/**
 * @brief Abstract base class for triggers.
 */
class TriggerBase : public Observer {
public:
    TriggerBase() = default;
    ~TriggerBase() override = default;

    virtual bool init(const std::string& triggerId, const StrategyConfig& strategyConfig);
    virtual bool proc() = 0;
    virtual bool checkCondition() = 0;
    virtual void registerVariableGetter(const std::string& var_name,
                                        std::function<TriggerChecker::Value()> getter) = 0;
    void setTriggerManager(const std::shared_ptr<TriggerManager>& trigger_manager) { trigger_manager_ = trigger_manager;}

    void notifyTrigger(const TriggerContext& context) const;

protected:
    std::unique_ptr<Trigger> trigger_obj_ = nullptr;
    std::shared_ptr<TriggerManager> trigger_manager_ = nullptr;

};

}

#endif //TRIGGER_BASE_H
