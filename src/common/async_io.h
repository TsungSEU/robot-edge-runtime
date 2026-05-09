// async_io.h - 异步 I/O 机制
// 提供高性能的异步文件读写和网络操作

#ifndef ASYNC_IO_H
#define ASYNC_IO_H

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <optional>
#include <variant>

namespace aurora::async {

// ==================== 通用结果类型 ====================

template<typename T>
struct Result {
    T value;
    bool success;
    std::string error_message;

    Result() : success(false) {}
    Result(T v) : value(std::move(v)), success(true) {}
    Result(std::string err) : error_message(std::move(err)), success(false) {}

    explicit operator bool() const { return success; }
};

// void 类型的特化
template<>
struct Result<void> {
    bool success;
    std::string error_message;

    Result() : success(false) {}
    Result(std::string err) : error_message(std::move(err)), success(false) {}
    Result(bool s) : success(s) {}

    static Result<void> Ok() { return Result<void>(true); }

    explicit operator bool() const { return success; }
};

// ==================== I/O 操作类型 ====================

enum class IoOperation {
    READ,
    WRITE,
    FLUSH,
    CLOSE
};

struct IoTask {
    IoOperation operation;
    std::string file_path;
    std::vector<char> data;
    std::function<void(const Result<std::vector<char>>&)> callback;
    std::promise<Result<std::vector<char>>> promise;
};

// ==================== 异步文件写入器 ====================

/**
 * @brief 异步文件写入器
 * 使用后台线程和缓冲队列实现高性能写入
 */
class AsyncFileWriter {
private:
    struct WriteTask {
        std::string file_path;
        std::vector<char> data;
        bool append;
        bool flush;
        std::function<void(const Result<size_t>&)> callback;
    };

    std::queue<WriteTask> write_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::vector<std::thread> writer_threads_;
    std::atomic<bool> running_{false};

    size_t queue_size_limit_;
    std::atomic<uint64_t> bytes_written_{0};
    std::atomic<uint64_t> write_count_{0};

    void writerLoop();
    void processTask(const WriteTask& task);

public:
    explicit AsyncFileWriter(size_t num_threads = 1, size_t queue_size_limit = 10000);
    ~AsyncFileWriter();

    // 禁止拷贝
    AsyncFileWriter(const AsyncFileWriter&) = delete;
    AsyncFileWriter& operator=(const AsyncFileWriter&) = delete;

    // 启动/停止
    void start();
    void stop();

    // 异步写入
    std::future<Result<size_t>> write(const std::string& file_path,
                                     const std::vector<char>& data,
                                     bool append = false,
                                     bool flush = false);

    // 带回调的异步写入
    void writeWithCallback(const std::string& file_path,
                          const std::vector<char>& data,
                          std::function<void(const Result<size_t>&)> callback,
                          bool append = false,
                          bool flush = false);

    // 同步写入 (阻塞，直到队列有空间)
    Result<size_t> writeSync(const std::string& file_path,
                             const std::vector<char>& data,
                             bool append = false);

    // 统计信息
    uint64_t getBytesWritten() const { return bytes_written_.load(); }
    uint64_t getWriteCount() const { return write_count_.load(); }
    size_t getQueueSize() const;

    // 刷新所有待写入数据
    void flush();
};

// ==================== 异步文件读取器 ====================

/**
 * @brief 异步文件读取器
 * 支持文件预加载和缓存
 */
class AsyncFileReader {
private:
    struct ReadTask {
        std::string file_path;
        size_t offset;
        size_t size;
        std::function<void(const Result<std::vector<char>>&)> callback;
        std::promise<Result<std::vector<char>>> promise;
    };

    std::queue<ReadTask> read_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::vector<std::thread> reader_threads_;
    std::atomic<bool> running_{false};

    // 文件缓存
    struct CacheEntry {
        std::string file_path;
        std::vector<char> data;
        std::chrono::steady_clock::time_point last_access;
        size_t access_count;
    };

    std::vector<CacheEntry> cache_;
    mutable std::mutex cache_mutex_;
    size_t max_cache_size_;
    size_t max_cache_file_size_;

    void readerLoop();
    Result<std::vector<char>> readFile(const std::string& file_path,
                                      size_t offset, size_t size);
    bool tryGetFromCache(const std::string& file_path, std::vector<char>& data);
    void addToCache(const std::string& file_path, std::vector<char> data);

public:
    explicit AsyncFileReader(size_t num_threads = 2,
                            size_t max_cache_size = 100 * 1024 * 1024,  // 100MB
                            size_t max_cache_file_size = 10 * 1024 * 1024);  // 10MB
    ~AsyncFileReader();

    // 禁止拷贝
    AsyncFileReader(const AsyncFileReader&) = delete;
    AsyncFileReader& operator=(const AsyncFileReader&) = delete;

    // 启动/停止
    void start();
    void stop();

    // 异步读取整个文件
    std::future<Result<std::vector<char>>> read(const std::string& file_path);

    // 异步读取部分文件
    std::future<Result<std::vector<char>>> read(const std::string& file_path,
                                               size_t offset, size_t size);

    // 带回调的异步读取
    void readWithCallback(const std::string& file_path,
                         std::function<void(const Result<std::vector<char>>&)> callback);

    // 预加载文件到缓存
    std::future<bool> preload(const std::string& file_path);

    // 同步读取 (从缓存或直接读取)
    Result<std::vector<char>> readSync(const std::string& file_path);

    // 清空缓存
    void clearCache();

    // 缓存统计
    size_t getCacheSize() const;
    size_t getCacheFileCount() const;
};

// ==================== 异步日志写入器 ====================

/**
 * @brief 高性能异步日志写入器
 * 专为高频日志场景优化
 */
class AsyncLogger {
private:
    std::unique_ptr<AsyncFileWriter> file_writer_;
    std::string log_directory_;
    std::string current_log_file_;

    // 日志缓冲区
    struct LogBuffer {
        std::vector<char> buffer;
        size_t used;
        std::mutex mutex;

        LogBuffer(size_t size) : buffer(size), used(0) {}
    };

    std::unique_ptr<LogBuffer> buffer_;
    size_t buffer_size_;
    std::atomic<uint64_t> dropped_messages_{0};

    // 定期刷新
    std::thread flush_thread_;
    std::atomic<bool> flush_running_{false};
    std::chrono::seconds flush_interval_;

    void flushLoop();
    Result<void> flushBuffer();

public:
    explicit AsyncLogger(const std::string& log_dir = "/tmp/aurora_logs",
                        size_t buffer_size = 1024 * 1024,  // 1MB
                        std::chrono::seconds flush_interval = std::chrono::seconds(5));
    ~AsyncLogger();

    // 禁止拷贝
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    void start();
    void stop();

    // 写入日志
    bool log(const std::string& message);
    bool log(const char* data, size_t size);
    bool log(const std::vector<char>& data);

    // 强制刷新
    void flush();

    // 按日志级别
    bool info(const std::string& message);
    bool warn(const std::string& message);
    bool error(const std::string& message);
    bool debug(const std::string& message);

    // 统计
    uint64_t getDroppedMessages() const { return dropped_messages_.load(); }

    // 获取当前日志文件路径
    std::string getCurrentLogFile() const { return current_log_file_; }
};

// ==================== ROS2 Bag 异步录制器 ====================

/**
 * @brief 异步 ROS2 Bag 录制器包装器
 * 提供非阻塞的 bag 录制功能
 */
class AsyncBagRecorder {
private:
    struct RecordingRequest {
        std::string topic_name;
        std::vector<char> message_data;
        std::chrono::nanoseconds timestamp;
    };

    std::queue<RecordingRequest> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::thread recording_thread_;
    std::atomic<bool> recording_{false};

    std::string bag_file_path_;
    std::atomic<uint64_t> messages_recorded_{0};
    std::atomic<uint64_t> bytes_recorded_{0};

    void recordingLoop();
    void processMessage(const RecordingRequest& request);

public:
    explicit AsyncBagRecorder(size_t queue_size = 10000);
    ~AsyncBagRecorder();

    // 禁止拷贝
    AsyncBagRecorder(const AsyncBagRecorder&) = delete;
    AsyncBagRecorder& operator=(const AsyncBagRecorder&) = delete;

    // 开始录制
    bool start(const std::string& bag_file_path);
    void stop();

    // 添加消息 (非阻塞)
    bool addMessage(const std::string& topic_name,
                   const std::vector<char>& message_data,
                   std::chrono::nanoseconds timestamp);

    // 检查是否正在录制
    bool isRecording() const { return recording_.load(); }

    // 统计
    uint64_t getMessagesRecorded() const { return messages_recorded_.load(); }
    uint64_t getBytesRecorded() const { return bytes_recorded_.load(); }
    size_t getQueueSize() const;
};

// ==================== 流式文件写入器 ====================

/**
 * @brief 流式文件写入器
 * 支持数据分块写入，适合大文件
 */
class StreamingFileWriter {
private:
    std::string file_path_;
    std::ofstream file_stream_;
    std::mutex write_mutex_;

    std::unique_ptr<AsyncFileWriter> async_writer_;
    bool use_async_;

    size_t chunk_size_;
    std::vector<char> write_buffer_;

    std::atomic<uint64_t> total_written_{0};

    Result<void> flushBuffer();

public:
    explicit StreamingFileWriter(const std::string& file_path,
                                bool use_async = true,
                                size_t chunk_size = 64 * 1024);  // 64KB
    ~StreamingFileWriter();

    // 打开文件
    bool open();

    // 关闭文件
    void close();

    // 写入数据
    size_t write(const void* data, size_t size);
    size_t write(const std::vector<char>& data);

    // 刷新
    void flush();

    // 获取已写入字节数
    uint64_t getTotalWritten() const { return total_written_.load(); }
};

// ==================== 异步 I/O 事件循环 ====================

/**
 * @brief 异步 I/O 事件循环
 * 管理多个异步 I/O 操作
 */
class AsyncIoEventLoop {
private:
    struct ScheduledTask {
        std::chrono::steady_clock::time_point execute_time;
        std::function<void()> task;
        bool recurring;
        std::chrono::milliseconds interval;
    };

    std::vector<ScheduledTask> scheduled_tasks_;
    std::mutex tasks_mutex_;

    std::thread event_thread_;
    std::atomic<bool> running_{false};

    void eventLoop();

public:
    AsyncIoEventLoop() = default;
    ~AsyncIoEventLoop();

    // 禁止拷贝
    AsyncIoEventLoop(const AsyncIoEventLoop&) = delete;
    AsyncIoEventLoop& operator=(const AsyncIoEventLoop&) = delete;

    // 启动/停止
    void start();
    void stop();

    // 安排一次性任务
    void schedule(std::function<void()> task,
                std::chrono::milliseconds delay);

    // 安排周期性任务
    void scheduleRecurring(std::function<void()> task,
                          std::chrono::milliseconds interval);

    // 在事件循环中执行
    void execute(std::function<void()> task);

    // 运行一步
    void runOnce();
};

} // namespace aurora::async

#endif // ASYNC_IO_H
