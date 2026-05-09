#include "channel_manager.h"
#include "common/log/logger.h"
#include "common/ros2/qos_profiles.h"

namespace aurora::collector {

bool ChannelManager::Init(const std::shared_ptr<rclcpp::Node>& node,
                          const StrategyConfig& config)
{

    node_ = node;
    strategy_config_ = config;

    message_subject_ = std::make_unique<Subject>();

    // Create isolated callback group for channel subscriptions
    channel_callback_group_ = node_->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);

    bool ret = InitSubscribers();
    CHECK_AND_RETURN(ret, ChannelManager, "InitSubscribers failed", false);

    return ret;
}

bool ChannelManager::InitSubscribers() {
    for (const auto& strategy : strategy_config_.strategies) {
        if (!strategy.trigger.enabled)  continue;
        for (const auto& channel : strategy.cyclone.channels) {
            std::string topic = channel.topic;
            if (subscribers_.find(topic) != subscribers_.end()) {
                continue;
            }

            // 使用 channel 中配置的消息类型
            std::string message_type = channel.type;
            auto callback = [this, topic](const std::shared_ptr<rclcpp::SerializedMessage>& msg) {
                this->Notify(topic, *msg);
            };

            rclcpp::SubscriptionOptions sub_options;
            sub_options.callback_group = channel_callback_group_;

            auto subscriber = node_->create_generic_subscription(
                topic,
                message_type,
                aurora::common::qos::channel_data(),
                callback,
                sub_options
            );

            if (!subscriber) {
                AD_ERROR(ChannelManager, "Create subscriber failed for topic: %s", topic.c_str());
                return false;
            }
            AD_DEBUG(ChannelManager, "Init subscriber for topic: %s, type: %s", topic.c_str(), message_type.c_str());
            subscribers_[topic] = subscriber;
        }
    }
    return true;

}

void ChannelManager::OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& msg) {
    // 实现消息处理逻辑
    AD_WARN(ChannelManager, "Received message on topic: %s", topic.c_str());
}

void ChannelManager::AddObserver(const std::shared_ptr<Observer>& observer) const
{
    if (message_subject_) {
        message_subject_->addObserver(observer);
    }
}

void ChannelManager::RemoveObserver(const std::shared_ptr<Observer>& observer) const
{
    if (message_subject_) {
        message_subject_->removeObserver(observer);
    }
}

void ChannelManager::Notify(const std::string& topic, const rclcpp::SerializedMessage& msg) const
{
    if (message_subject_) {
        message_subject_->notifyAll(topic, msg);
    }
}


}