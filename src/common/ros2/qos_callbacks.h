// qos_callbacks.h — QoS event callback utilities for Aurora Edge Runtime
//
// Provides helpers for handling QoS deadline and liveliness violations.
// Used to detect and log when critical topics become unresponsive.

#ifndef AURORA_COMMON_ROS2_QOS_CALLBACKS_H_
#define AURORA_COMMON_ROS2_QOS_CALLBACKS_H_

#include <rclcpp/subscription_base.hpp>
#include <rclcpp/publisher_base.hpp>
#include <rcl/event.h>
#include <string>
#include "common/log/logger.h"

namespace aurora::common::qos {

// Install a deadline event callback on a subscriber.
// Logs a warning each time the publisher misses its deadline.
template<typename MsgT>
void installDeadlineCallback(
    typename rclcpp::Subscription<MsgT>::SharedPtr sub,
    const std::string& topic_name) {
    sub->set_on_new_qos_event_callback(
        [topic_name](size_t missed_count) {
            AD_WARN(QoSMonitor,
                    "Deadline missed on topic '%s': total_missed=%zu",
                    topic_name.c_str(), missed_count);
        },
        RCL_SUBSCRIPTION_REQUESTED_DEADLINE_MISSED);
}

// Install a liveliness lost callback on a publisher.
// Logs an error each time the publisher fails to assert liveliness.
template<typename MsgT>
void installLivelinessCallback(
    typename rclcpp::Publisher<MsgT>::SharedPtr pub,
    const std::string& topic_name) {
    pub->set_on_new_qos_event_callback(
        [topic_name](size_t lost_count) {
            AD_ERROR(QoSMonitor,
                     "Liveliness lost on topic '%s': total_lost=%zu",
                     topic_name.c_str(), lost_count);
        },
        RCL_PUBLISHER_LIVELINESS_LOST);
}

}  // namespace aurora::common::qos

#endif  // AURORA_COMMON_ROS2_QOS_CALLBACKS_H_
