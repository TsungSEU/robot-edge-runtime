//
// Created by xucong on 25-9-24.
// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#ifndef OBSERVER_H
#define OBSERVER_H

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <string>
#include <functional>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rosbag2_cpp/rosbag2_cpp/writer.hpp"

namespace aurora::collector {

class Observer {
public:
    virtual ~Observer() = default;

    virtual void OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& subject) = 0;
};

class Subject {
public:
    virtual~Subject() = default;

    void addObserver(std::shared_ptr<Observer> observer) {
        observers_.emplace_back(observer);
    }


    void removeObserver(const std::shared_ptr<Observer>& observer) {
        auto iter = std::find(observers_.begin(), observers_.end(), observer);
        if (iter != observers_.end()) {
            observers_.erase(iter);
        }
    }

    void notifyAll(const std::string& topic, const rclcpp::SerializedMessage& subject) const
    {
        for (const auto& observer : observers_) {
            observer->OnMessageReceived(topic, subject);
        }
    }

    const std::vector<std::shared_ptr<Observer>>& getObservers() const { return observers_; }

protected:
    std::vector<std::shared_ptr<Observer>> observers_;
};

}

#endif
