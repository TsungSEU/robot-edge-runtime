#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <mutex>

#include "observer.h"
#include "trigger/trigger_manager.h"

namespace aurora::collector {
class ChannelManager : public Observer {
public:
    ChannelManager() = default;
    ~ChannelManager() override = default;

    bool Init(const std::shared_ptr<rclcpp::Node>& node,
              const StrategyConfig& config);

    void AddObserver(const std::shared_ptr<Observer>& observer) const;
    void RemoveObserver(const std::shared_ptr<Observer>& observer) const;
    void Notify(const std::string& topic, const rclcpp::SerializedMessage& msg) const;

private:
    bool InitSubscribers();
    void OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& msg) override;

    std::shared_ptr<rclcpp::Node> node_;
    std::map<std::string, rclcpp::GenericSubscription::SharedPtr> subscribers_;
    StrategyConfig strategy_config_;
    std::shared_ptr<TriggerManager> trigger_manager_{nullptr};
    std::unique_ptr<Subject> message_subject_;

    // Isolated callback group for channel subscriptions
    rclcpp::CallbackGroup::SharedPtr channel_callback_group_;

};
}