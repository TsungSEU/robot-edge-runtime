#pragma once

#include <list>
#include <stdexcept>
#include <iterator>
#include <mutex>

namespace aurora::collector {

template <typename T>
class RingBuffer {
public:
    using iterator = typename std::list<T>::iterator;
    using const_iterator = typename std::list<T>::const_iterator;
    using value_type = T;
    using size_type = size_t;
    using reference = T&;
    using const_reference = const T&;

    explicit RingBuffer(size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("RingBuffer capacity must be greater than zero.");
        }
    }

    void push_back(const T& value) {
        std::lock_guard<std::mutex> lc(mtx_);
        if (buffer_.size() >= capacity_) {
            buffer_.pop_front();  // 如果已满，移除最早添加的元素
        }
        buffer_.emplace_back(value);
    }

    T& front(){
        std::lock_guard<std::mutex> lc(mtx_);
        if(!buffer_.empty())
            return buffer_.front();
    }

    bool pop_front()
    {
        std::lock_guard<std::mutex> lc(mtx_);
        if(!buffer_.empty())
        {
            buffer_.pop_front();
            return true;
        }

        return false;
    }

    T& operator[](size_t index) {
        std::lock_guard<std::mutex> lc(mtx_);
        if (index >= buffer_.size()) {
            throw std::out_of_range("Index out of range.");
        }
        auto it = buffer_.begin();
        std::advance(it, index);
        return *it;
    }

    T& at(size_t index) {
        std::lock_guard<std::mutex> lc(mtx_);
        if (index >= buffer_.size()) {
            throw std::out_of_range("Index out of range.");
        }
        auto it = buffer_.begin();
        std::advance(it, index);
        return *it;
    }

    const T& at(size_t index) const {
        std::lock_guard<std::mutex> lc(mtx_);
        if (index >= buffer_.size()) {
            throw std::out_of_range("Index out of range.");
        }
        auto it = buffer_.begin();
        std::advance(it, index);
        return *it;
    }

    size_t size() {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.size();
    }

    size_t capacity() {
        std::lock_guard<std::mutex> lc(mtx_);
        return capacity_;
    }

    bool empty() {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.empty();
    }

    void clear() {
        buffer_.clear();
    }

    iterator begin() {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.begin();
    }

    iterator end() {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.end();
    }

    const_iterator begin() const {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.begin();
    }

    const_iterator end() const {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.end();
    }

    const_iterator cbegin() const {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.cbegin();
    }

    const_iterator cend() const {
        std::lock_guard<std::mutex> lc(mtx_);
        return buffer_.cend();
    }

private:
    std::list<T> buffer_;
    size_t capacity_;
    mutable std::mutex mtx_;
};

}

