#include "channel/message_provider.h"
#include "common/log/logger.h"
#include "common/base.h"

namespace aurora::collector {

void MessageProvider::OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& msg)
{
    ///TODO
    if(topic == "/canbus/vehicle_report")
    {
        AD_INFO(MessageProvider, "Observed topic: %s", topic.c_str());
    } else if (topic == "/decision_planning/planning_state") {
        AD_INFO(MessageProvider, "Observed topic: %s", topic.c_str());
    }
    else if (topic == "/mcu/vehicle_processing") {
        AD_INFO(MessageProvider, "Observed topic: %s", topic.c_str());
    }
    else if (topic == "/mcu/state_machine") {
        AD_INFO(MessageProvider, "Observed topic: %s", topic.c_str());
    }
}

}
