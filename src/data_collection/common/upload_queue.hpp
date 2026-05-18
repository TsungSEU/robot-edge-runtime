/*
 * Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
 * Tsung Xu<congx0829@163.com>
 */

#ifndef UPLOAD_QUEUE_H
#define UPLOAD_QUEUE_H

#include <queue>
#include <mutex>
#include <string>
#include "common/base.h"
#include "common/data.h"

namespace aurora::collector {

struct UploadItem {
    std::string file_path;
    UploadType upload_type;
    UploadItem() : file_path(""), upload_type(UploadType::None) {}
    UploadItem(const std::string& path, UploadType type) : file_path(path), upload_type(type) {}
};

class UploadQueue {
public:
    UploadQueue(const UploadQueue&) = delete;
    UploadQueue& operator=(const UploadQueue&) = delete;

    // 获取单例实例
    static UploadQueue& GetInstance() {
        static UploadQueue instance;
        return instance;
    }

    void Push(const UploadItem& elem) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(elem);
    }

    optional<UploadItem> Front() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullopt;
        }
        return queue_.front();
    }

    optional<UploadItem> Pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullopt;
        }
        auto val = queue_.front();
        queue_.pop();
        return val;
    }

    bool Empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    UploadQueue() = default;
    ~UploadQueue() = default;

    mutable std::mutex mutex_;
    std::queue<UploadItem> queue_;
};

}

#endif //UPLOAD_QUEUE_H
