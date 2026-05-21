#pragma once

#include <list>
#include <stdexcept>
#include <iterator>
#include <mutex>
#include <vector>

namespace aurora::collector {

template <typename T>
class RingBuffer {
public:
    using value_type = T;
    using size_type = size_t;

    explicit RingBuffer(size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("RingBuffer capacity must be greater than zero.");
        }
    }

    void push_back(const T& value) {
        std::lock_guard<std::mutex> lc(mtx_);
        if (buffer_.size() >= capacity_) {
            buffer_.pop_front();
        }
        buffer_.emplace_back(value);
    }

    bool front(T& value) const {
        std::lock_guard<std::mutex> lc(mtx_);
        if (buffer_.empty()) {
            return false;
        }
        value = buffer_.front();
        return true;
    }

    bool pop_front() {
        std::lock_guard<std::mutex> lc(mtx_);
        if (buffer_.empty()) {
            return false;
        }
        buffer_.pop_front();
        return true;
    }

    std::vector<T> snapshot() const {
        std::lock_guard<std::mutex> lc(mtx_);
        return std::vector<T>(buffer_.begin(), buffer_.end());
    }

    T at(size_t index) const {
        std::lock_guard<std::mutex> lc(mtx_);
        if (index >= buffer_.size()) {
            throw std::out_of_range("Index out of range.");
        }
        auto it = buffer_.begin();
        std::advance(it, index);
        return *it;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.size();
    }

    size_t capacity() const {
        std::lock_guard<std::mutex> lc(mtx_);
        return capacity_;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.empty();
    }

    void clear() {
        std::lock_guard<std::mutex> lc(mtx_);
        buffer_.clear();
    }

private:
    std::list<T> buffer_;
    size_t capacity_;
    mutable std::mutex mtx_;
};

}
