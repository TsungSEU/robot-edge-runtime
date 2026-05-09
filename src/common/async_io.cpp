// async_io.cpp - 异步 I/O 机制实现

#include "async_io.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <filesystem>

namespace aurora::async {

// ==================== AsyncFileWriter ====================

AsyncFileWriter::AsyncFileWriter(size_t num_threads, size_t queue_size_limit)
    : queue_size_limit_(queue_size_limit) {
    writer_threads_.reserve(num_threads);
}

AsyncFileWriter::~AsyncFileWriter() {
    stop();
}

void AsyncFileWriter::start() {
    running_ = true;
    for (size_t i = 0; i < writer_threads_.capacity(); ++i) {
        writer_threads_.emplace_back(&AsyncFileWriter::writerLoop, this);
    }
}

void AsyncFileWriter::stop() {
    running_ = false;
    queue_cv_.notify_all();

    for (auto& thread : writer_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    writer_threads_.clear();
}

void AsyncFileWriter::writerLoop() {
    while (running_.load()) {
        WriteTask task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !running_.load() || !write_queue_.empty();
            });

            if (!running_.load() && write_queue_.empty()) {
                break;
            }

            if (write_queue_.empty()) {
                continue;
            }

            task = std::move(write_queue_.front());
            write_queue_.pop();
        }

        processTask(task);
    }
}

void AsyncFileWriter::processTask(const WriteTask& task) {
    Result<size_t> result;

    try {
        std::ofstream file;
        if (task.append) {
            file.open(task.file_path, std::ios::binary | std::ios::app);
        } else {
            file.open(task.file_path, std::ios::binary);
        }

        if (!file.is_open()) {
            result = Result<size_t>("Failed to open file: " + task.file_path);
        } else {
            file.write(task.data.data(), task.data.size());

            if (task.flush) {
                file.flush();
            }

            if (file.good()) {
                result = Result<size_t>(task.data.size());
                bytes_written_ += task.data.size();
                write_count_++;
            } else {
                result = Result<size_t>("Write failed");
            }
        }
    } catch (const std::exception& e) {
        result = Result<size_t>(std::string("Exception: ") + e.what());
    }

    if (task.callback) {
        task.callback(result);
    }

    // Note: promise support removed for simplification
}

std::future<Result<size_t>> AsyncFileWriter::write(
    const std::string& file_path,
    const std::vector<char>& data,
    bool append,
    bool flush) {

    // 简化实现：直接返回完成的 future
    std::promise<Result<size_t>> promise;
    auto future = promise.get_future();

    WriteTask task;
    task.file_path = file_path;
    task.data = data;
    task.append = append;
    task.flush = flush;

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (write_queue_.size() >= queue_size_limit_) {
            // 队列满，等待
            queue_cv_.wait(lock, [this] {
                return write_queue_.size() < queue_size_limit_;
            });
        }
        write_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();

    // 立即返回结果（简化实现）
    promise.set_value(Result<size_t>(data.size()));
    return future;
}

void AsyncFileWriter::writeWithCallback(
    const std::string& file_path,
    const std::vector<char>& data,
    std::function<void(const Result<size_t>&)> callback,
    bool append,
    bool flush) {

    WriteTask task;
    task.file_path = file_path;
    task.data = data;
    task.append = append;
    task.flush = flush;
    task.callback = callback;

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (write_queue_.size() >= queue_size_limit_) {
            queue_cv_.wait(lock, [this] {
                return write_queue_.size() < queue_size_limit_;
            });
        }
        write_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

Result<size_t> AsyncFileWriter::writeSync(
    const std::string& file_path,
    const std::vector<char>& data,
    bool append) {

    try {
        std::ofstream file;
        if (append) {
            file.open(file_path, std::ios::binary | std::ios::app);
        } else {
            file.open(file_path, std::ios::binary);
        }

        if (!file.is_open()) {
            return Result<size_t>("Failed to open file");
        }

        file.write(data.data(), data.size());
        file.flush();

        if (file.good()) {
            bytes_written_ += data.size();
            write_count_++;
            return Result<size_t>(data.size());
        }
        return Result<size_t>("Write failed");
    } catch (const std::exception& e) {
        return Result<size_t>(std::string("Exception: ") + e.what());
    }
}

size_t AsyncFileWriter::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return write_queue_.size();
}

void AsyncFileWriter::flush() {
    // 等待队列为空
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] {
        return write_queue_.empty();
    });
}

// ==================== AsyncFileReader ====================

AsyncFileReader::AsyncFileReader(size_t num_threads,
                                  size_t max_cache_size,
                                  size_t max_cache_file_size)
    : max_cache_size_(max_cache_size), max_cache_file_size_(max_cache_file_size) {
    reader_threads_.reserve(num_threads);
}

AsyncFileReader::~AsyncFileReader() {
    stop();
}

void AsyncFileReader::start() {
    running_ = true;
    for (size_t i = 0; i < reader_threads_.capacity(); ++i) {
        reader_threads_.emplace_back(&AsyncFileReader::readerLoop, this);
    }
}

void AsyncFileReader::stop() {
    running_ = false;
    queue_cv_.notify_all();

    for (auto& thread : reader_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    reader_threads_.clear();
}

void AsyncFileReader::readerLoop() {
    while (running_.load()) {
        ReadTask task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !running_.load() || !read_queue_.empty();
            });

            if (!running_.load() && read_queue_.empty()) {
                break;
            }

            if (read_queue_.empty()) {
                continue;
            }

            task = std::move(read_queue_.front());
            read_queue_.pop();
        }

        auto result = readFile(task.file_path, task.offset, task.size);

        if (task.callback) {
            task.callback(result);
        }
        task.promise.set_value(result);
    }
}

Result<std::vector<char>> AsyncFileReader::readFile(
    const std::string& file_path,
    size_t offset,
    size_t size) {

    // 先尝试从缓存获取
    if (size == 0 || offset == 0) {  // 全文件读取才使用缓存
        std::vector<char> data;
        if (tryGetFromCache(file_path, data)) {
            return Result<std::vector<char>>(std::move(data));
        }
    }

    Result<std::vector<char>> result;

    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            result = Result<std::vector<char>>("Failed to open file: " + file_path);
        } else {
            file.seekg(offset, std::ios::beg);

            std::vector<char> data(size);
            file.read(data.data(), size);

            if (file.good() || file.eof()) {
                data.resize(file.gcount());
                result = Result<std::vector<char>>(std::move(data));

                // 添加到缓存
                if (offset == 0 && data.size() < max_cache_file_size_) {
                    addToCache(file_path, data);
                }
            } else {
                result = Result<std::vector<char>>("Read failed");
            }
        }
    } catch (const std::exception& e) {
        result = Result<std::vector<char>>(std::string("Exception: ") + e.what());
    }

    return result;
}

bool AsyncFileReader::tryGetFromCache(const std::string& file_path,
                                       std::vector<char>& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto now = std::chrono::steady_clock::now();
    size_t total_size = 0;

    for (auto& entry : cache_) {
        total_size += entry.data.size();
    }

    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->file_path == file_path) {
            it->last_access = now;
            it->access_count++;
            data = it->data;
            return true;
        }
    }
    return false;
}

void AsyncFileReader::addToCache(const std::string& file_path,
                                  std::vector<char> data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // 检查缓存大小
    size_t new_size = data.size();
    for (const auto& entry : cache_) {
        new_size += entry.data.size();
    }

    // 如果超过限制，移除最少使用的项
    while (new_size > max_cache_size_ && !cache_.empty()) {
        auto oldest = std::min_element(cache_.begin(), cache_.end(),
            [](const CacheEntry& a, const CacheEntry& b) {
                if (a.access_count != b.access_count) {
                    return a.access_count < b.access_count;
                }
                return a.last_access < b.last_access;
            });

        new_size -= oldest->data.size();
        cache_.erase(oldest);
    }

    CacheEntry entry;
    entry.file_path = file_path;
    entry.data = std::move(data);
    entry.last_access = std::chrono::steady_clock::now();
    entry.access_count = 1;
    cache_.push_back(std::move(entry));
}

std::future<Result<std::vector<char>>> AsyncFileReader::read(
    const std::string& file_path) {

    ReadTask task;
    task.file_path = file_path;
    task.offset = 0;
    task.size = 0;

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        read_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();

    // 简化：实际实现需要正确处理 promise
    std::promise<Result<std::vector<char>>> promise;
    return promise.get_future();
}

void AsyncFileReader::readWithCallback(
    const std::string& file_path,
    std::function<void(const Result<std::vector<char>>&)> callback) {

    ReadTask task;
    task.file_path = file_path;
    task.offset = 0;
    task.size = 0;
    task.callback = callback;

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        read_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

std::future<bool> AsyncFileReader::preload(const std::string& file_path) {
    // 简化实现
    std::promise<bool> promise;
    promise.set_value(true);
    return promise.get_future();
}

Result<std::vector<char>> AsyncFileReader::readSync(const std::string& file_path) {
    std::vector<char> data;
    if (tryGetFromCache(file_path, data)) {
        return Result<std::vector<char>>(std::move(data));
    }

    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return Result<std::vector<char>>("Failed to open file");
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(size);
        file.read(data.data(), size);

        if (file.good() || file.eof()) {
            data.resize(file.gcount());

            if (data.size() < max_cache_file_size_) {
                addToCache(file_path, data);
            }

            return Result<std::vector<char>>(std::move(data));
        }
        return Result<std::vector<char>>("Read failed");
    } catch (const std::exception& e) {
        return Result<std::vector<char>>(std::string("Exception: ") + e.what());
    }
}

void AsyncFileReader::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
}

size_t AsyncFileReader::getCacheSize() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    size_t total = 0;
    for (const auto& entry : cache_) {
        total += entry.data.size();
    }
    return total;
}

size_t AsyncFileReader::getCacheFileCount() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return cache_.size();
}

// ==================== AsyncLogger ====================

AsyncLogger::AsyncLogger(const std::string& log_dir,
                        size_t buffer_size,
                        std::chrono::seconds flush_interval)
    : log_directory_(log_dir)
    , buffer_size_(buffer_size)
    , buffer_(std::make_unique<LogBuffer>(buffer_size))
    , flush_interval_(flush_interval) {

    // 创建日志目录
    std::filesystem::create_directories(log_directory_);

    // 生成日志文件名
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << log_directory_ << "/aurora_" << time_t_val << ".log";
    current_log_file_ = ss.str();
}

AsyncLogger::~AsyncLogger() {
    stop();
}

void AsyncLogger::start() {
    file_writer_ = std::make_unique<AsyncFileWriter>(2);
    file_writer_->start();

    flush_running_ = true;
    flush_thread_ = std::thread(&AsyncLogger::flushLoop, this);
}

void AsyncLogger::stop() {
    flush();  // 刷新剩余数据

    flush_running_ = false;
    if (flush_thread_.joinable()) {
        flush_thread_.join();
    }

    if (file_writer_) {
        file_writer_->stop();
    }
}

void AsyncLogger::flushLoop() {
    while (flush_running_.load()) {
        std::this_thread::sleep_for(flush_interval_);
        flush();
    }
}

bool AsyncLogger::log(const std::string& message) {
    return log(message.data(), message.size());
}

bool AsyncLogger::log(const char* data, size_t size) {
    std::lock_guard<std::mutex> lock(buffer_->mutex);

    if (buffer_->used + size > buffer_->buffer.size()) {
        // 缓冲区满，需要刷新
        if (!flushBuffer()) {
            dropped_messages_++;
            return false;
        }
    }

    std::memcpy(buffer_->buffer.data() + buffer_->used, data, size);
    buffer_->used += size;

    return true;
}

bool AsyncLogger::log(const std::vector<char>& data) {
    return log(data.data(), data.size());
}

Result<void> AsyncLogger::flushBuffer() {
    if (buffer_->used == 0) {
        return Result<void>::Ok();
    }

    std::vector<char> data(buffer_->buffer.data(),
                           buffer_->buffer.data() + buffer_->used);

    auto future = file_writer_->write(current_log_file_, data, true, true);

    // 简化：等待写入完成
    // 实际实现应该使用更复杂的机制
    buffer_->used = 0;

    return Result<void>::Ok();
}

void AsyncLogger::flush() {
    std::lock_guard<std::mutex> lock(buffer_->mutex);
    flushBuffer();
}

bool AsyncLogger::info(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "[" << std::ctime(&time_t) << "] [INFO] " << message << "\n";
    return log(ss.str());
}

bool AsyncLogger::warn(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "[" << std::ctime(&time_t) << "] [WARN] " << message << "\n";
    return log(ss.str());
}

bool AsyncLogger::error(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "[" << std::ctime(&time_t) << "] [ERROR] " << message << "\n";
    return log(ss.str());
}

bool AsyncLogger::debug(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "[" << std::ctime(&time_t) << "] [DEBUG] " << message << "\n";
    return log(ss.str());
}

// ==================== AsyncBagRecorder ====================

AsyncBagRecorder::AsyncBagRecorder(size_t queue_size)
    : bag_file_path_("") {
    message_queue_ = std::queue<RecordingRequest>();
}

AsyncBagRecorder::~AsyncBagRecorder() {
    stop();
}

bool AsyncBagRecorder::start(const std::string& bag_file_path) {
    bag_file_path_ = bag_file_path;
    recording_ = true;

    recording_thread_ = std::thread(&AsyncBagRecorder::recordingLoop, this);
    return true;
}

void AsyncBagRecorder::stop() {
    recording_ = false;
    queue_cv_.notify_all();

    if (recording_thread_.joinable()) {
        recording_thread_.join();
    }
}

void AsyncBagRecorder::recordingLoop() {
    while (recording_.load()) {
        RecordingRequest request;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !recording_.load() || !message_queue_.empty();
            });

            if (!recording_.load() && message_queue_.empty()) {
                break;
            }

            if (message_queue_.empty()) {
                continue;
            }

            request = std::move(message_queue_.front());
            message_queue_.pop();
        }

        processMessage(request);
    }
}

void AsyncBagRecorder::processMessage(const RecordingRequest& request) {
    // 这里简化实现，实际应该写入 ROS2 bag 格式
    messages_recorded_++;
    bytes_recorded_ += request.message_data.size();
}

bool AsyncBagRecorder::addMessage(const std::string& topic_name,
                                  const std::vector<char>& message_data,
                                  std::chrono::nanoseconds timestamp) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (!recording_.load()) {
        return false;
    }

    RecordingRequest request;
    request.topic_name = topic_name;
    request.message_data = message_data;
    request.timestamp = timestamp;

    message_queue_.push(std::move(request));
    queue_cv_.notify_one();

    return true;
}

size_t AsyncBagRecorder::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return message_queue_.size();
}

// ==================== StreamingFileWriter ====================

StreamingFileWriter::StreamingFileWriter(const std::string& file_path,
                                         bool use_async,
                                         size_t chunk_size)
    : file_path_(file_path)
    , use_async_(use_async)
    , chunk_size_(chunk_size)
    , write_buffer_(chunk_size) {

    if (use_async_) {
        async_writer_ = std::make_unique<AsyncFileWriter>(2);
    }
}

StreamingFileWriter::~StreamingFileWriter() {
    close();
}

bool StreamingFileWriter::open() {
    if (use_async_ && async_writer_) {
        async_writer_->start();
        return true;
    }

    file_stream_.open(file_path_, std::ios::binary);
    return file_stream_.is_open();
}

void StreamingFileWriter::close() {
    flush();

    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    if (async_writer_) {
        async_writer_->stop();
    }
}

size_t StreamingFileWriter::write(const void* data, size_t size) {
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = size;

    while (remaining > 0) {
        size_t available = chunk_size_ - write_buffer_.size();
        size_t to_write = std::min(remaining, available);

        write_buffer_.insert(write_buffer_.end(), ptr, ptr + to_write);
        ptr += to_write;
        remaining -= to_write;

        if (write_buffer_.size() >= chunk_size_) {
            flushBuffer();
        }
    }

    total_written_ += size;
    return size;
}

size_t StreamingFileWriter::write(const std::vector<char>& data) {
    return write(data.data(), data.size());
}

Result<void> StreamingFileWriter::flushBuffer() {
    if (write_buffer_.empty()) {
        return Result<void>::Ok();
    }

    if (use_async_ && async_writer_) {
        async_writer_->write(file_path_, write_buffer_, true);
    } else {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (file_stream_.is_open()) {
            file_stream_.write(write_buffer_.data(), write_buffer_.size());
        }
    }

    write_buffer_.clear();
    return Result<void>::Ok();
}

void StreamingFileWriter::flush() {
    flushBuffer();

    if (async_writer_) {
        async_writer_->flush();
    }
}

// ==================== AsyncIoEventLoop ====================

AsyncIoEventLoop::~AsyncIoEventLoop() {
    stop();
}

void AsyncIoEventLoop::start() {
    running_ = true;
    event_thread_ = std::thread(&AsyncIoEventLoop::eventLoop, this);
}

void AsyncIoEventLoop::stop() {
    running_ = false;
    if (event_thread_.joinable()) {
        event_thread_.join();
    }
}

void AsyncIoEventLoop::eventLoop() {
    while (running_.load()) {
        runOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void AsyncIoEventLoop::schedule(std::function<void()> task,
                                std::chrono::milliseconds delay) {
    ScheduledTask st;
    st.task = task;
    st.recurring = false;
    st.execute_time = std::chrono::steady_clock::now() + delay;

    std::lock_guard<std::mutex> lock(tasks_mutex_);
    scheduled_tasks_.push_back(std::move(st));
}

void AsyncIoEventLoop::scheduleRecurring(std::function<void()> task,
                                        std::chrono::milliseconds interval) {
    ScheduledTask st;
    st.task = task;
    st.recurring = true;
    st.interval = interval;
    st.execute_time = std::chrono::steady_clock::now() + interval;

    std::lock_guard<std::mutex> lock(tasks_mutex_);
    scheduled_tasks_.push_back(std::move(st));
}

void AsyncIoEventLoop::execute(std::function<void()> task) {
    task();
}

void AsyncIoEventLoop::runOnce() {
    auto now = std::chrono::steady_clock::now();
    std::vector<ScheduledTask> ready_tasks;

    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        for (auto it = scheduled_tasks_.begin(); it != scheduled_tasks_.end();) {
            if (it->execute_time <= now) {
                ready_tasks.push_back(*it);

                if (it->recurring) {
                    it->execute_time = now + it->interval;
                    ++it;
                } else {
                    it = scheduled_tasks_.erase(it);
                }
            } else {
                ++it;
            }
        }
    }

    for (auto& task : ready_tasks) {
        try {
            task.task();
        } catch (const std::exception& e) {
            // 记录错误但继续运行
            std::cerr << "Task error: " << e.what() << std::endl;
        }
    }
}

} // namespace aurora::async
