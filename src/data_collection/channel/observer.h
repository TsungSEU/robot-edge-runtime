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
#include <algorithm>
#include <utility>

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
        std::lock_guard<std::mutex> lock(observers_mutex_);
        observers_.emplace_back(std::move(observer));
    }

    void removeObserver(const std::shared_ptr<Observer>& observer) {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        auto iter = std::find(observers_.begin(), observers_.end(), observer);
        if (iter != observers_.end()) {
            observers_.erase(iter);
        }
    }

    void notifyAll(const std::string& topic, const rclcpp::SerializedMessage& subject) const
    {
        std::vector<std::shared_ptr<Observer>> observers_snapshot;
        {
            std::lock_guard<std::mutex> lock(observers_mutex_);
            observers_snapshot = observers_;
        }

        for (const auto& observer : observers_snapshot) {
            if (observer) {
                observer->OnMessageReceived(topic, subject);
            }
        }
    }

    std::vector<std::shared_ptr<Observer>> getObservers() const {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        return observers_;
    }

protected:
    std::vector<std::shared_ptr<Observer>> observers_;
    mutable std::mutex observers_mutex_;
};

}

#endif
