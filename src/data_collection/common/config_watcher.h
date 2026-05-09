// config_watcher.h
#ifndef CONFIG_WATCHER_H
#define CONFIG_WATCHER_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <optional>

namespace aurora::collector {

/**
 * @brief 配置文件监视器 - 检测文件变化并触发回调
 *
 * 功能：
 * 1. 周期性检查文件的修改时间
 * 2. 当文件变化时触发注册的回调函数
 * 3. 线程安全设计
 * 4. 支持启动/停止控制
 */
class ConfigWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& file_path)>;

    /**
     * @brief 构造函数
     * @param file_path 要监视的配置文件路径
     * @param check_interval_ms 检查间隔（毫秒），默认 500ms
     */
    explicit ConfigWatcher(const std::string& file_path,
                          uint32_t check_interval_ms = 500);

    ~ConfigWatcher();

    // 禁止拷贝和移动
    ConfigWatcher(const ConfigWatcher&) = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;
    ConfigWatcher(ConfigWatcher&&) = delete;
    ConfigWatcher& operator=(ConfigWatcher&&) = delete;

    /**
     * @brief 启动文件监视
     * @return true 如果成功启动
     */
    bool start();

    /**
     * @brief 停止文件监视
     */
    void stop();

    /**
     * @brief 设置文件变化回调
     * @param callback 当文件变化时调用的函数
     */
    void setChangeCallback(ChangeCallback callback);

    /**
     * @brief 手动检查文件是否变化
     * @return true 如果文件已变化
     */
    bool checkChanged();

    /**
     * @brief 获取文件最后修改时间
     * @return 文件最后修改时间戳
     */
    std::filesystem::file_time_type getLastModifiedTime() const;

    /**
     * @brief 获取监视的文件路径
     */
    const std::string& getFilePath() const { return file_path_; }

    /**
     * @brief 检查是否正在运行
     */
    bool isRunning() const { return running_.load(); }

private:
    /**
     * @brief 监视线程主函数
     */
    void watchThread();

    /**
     * @brief 读取文件当前修改时间
     * @return 文件修改时间，如果失败返回无值
     */
    std::optional<std::filesystem::file_time_type> readFileModTime() const;

private:
    std::string file_path_;
    uint32_t check_interval_ms_;
    std::filesystem::file_time_type last_mod_time_;
    mutable std::mutex mod_time_mutex_;

    ChangeCallback change_callback_;
    std::mutex callback_mutex_;

    std::atomic<bool> running_{false};
    std::thread watch_thread_;

    static constexpr uint32_t DEFAULT_CHECK_INTERVAL_MS = 500;
};

} // namespace aurora::collector

#endif // CONFIG_WATCHER_H
