#pragma once

#include <atomic>
#include "common/base.h"
#include "channel/observer.h"

namespace aurora::collector {

class MessageProvider : public Observer {
public:
    MessageProvider(const std::shared_ptr<rclcpp::Node>& node ):node_(node) {}

    ~MessageProvider() override = default;

    void OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& msg) override;

private:
    std::shared_ptr<rclcpp::Node> node_{nullptr};
};

}
