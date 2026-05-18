/*
 * Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
 * Tsung Xu<congx0829@163.com>
 */

#ifndef LOG_TASK_QUEUE_H
#define LOG_TASK_QUEUE_H

#include <queue>
#include <unordered_set>
#include <mutex>
#include <utility>

#include "common/data.h"
#include "common/base.h"

namespace aurora::collector {

template <typename T, typename Priority, typename Compare = std::less<Priority>, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
class TaskPriorityQueue {
public:
    static TaskPriorityQueue& GetInstance() {
        static TaskPriorityQueue instance;
        return instance;
    }

    void Push(Priority priority, const T& element) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (unique_elements_.find(element) == unique_elements_.end()) {
            pq_.push(std::make_pair(priority, element));
            unique_elements_.insert(element);
        }
    }

    optional<std::pair<Priority, T>> Pop() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (pq_.empty()) {
            return nullopt;
        }
        auto top = pq_.top();
        pq_.pop();
        unique_elements_.erase(top.second);
        return top;
    }

    bool Empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return pq_.empty();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return pq_.size();
    }

private:
    TaskPriorityQueue() = default;
    TaskPriorityQueue(const TaskPriorityQueue&) = delete;
    TaskPriorityQueue& operator=(const TaskPriorityQueue&) = delete;

    struct PriorityPairCompare {
        Compare comp;
        bool operator()(const std::pair<Priority, T>& a, const std::pair<Priority, T>& b) const {
            return comp(a.first, b.first);
        }
    };

    std::priority_queue<std::pair<Priority, T>, std::vector<std::pair<Priority, T>>, PriorityPairCompare> pq_;
    std::unordered_set<T, Hash, Equal> unique_elements_;
    mutable std::mutex mtx_;
};

static std::string serializer(const LogUploadTask& task) {
    std::string res;
    res += task.vin + "_";
    for (auto it : task.log_type) {
        res += std::to_string(static_cast<int>(it)) + "_";
    }
    res += task.start_date + "_" + task.end_date + "_" + task.task_id;
    return res;
}

struct TaskEqual {
    bool operator()(const LogUploadTask& a, const LogUploadTask& b) const { return serializer(a) == serializer(b); }
};

struct TaskHash {
    size_t operator()(const LogUploadTask& t) const { return std::hash<std::string>{}(serializer(t)); }
};

using TaskQueue = TaskPriorityQueue<LogUploadTask, int, std::less<int>, TaskHash, TaskEqual>;

}


#endif //LOG_TASK_QUEUE_H
