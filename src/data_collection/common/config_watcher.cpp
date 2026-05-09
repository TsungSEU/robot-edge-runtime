// config_watcher.cpp
#include "config_watcher.h"
#include "common/log/logger.h"
#include <optional>

namespace aurora::collector {

ConfigWatcher::ConfigWatcher(const std::string& file_path, uint32_t check_interval_ms)
    : file_path_(file_path),
      check_interval_ms_(check_interval_ms) {

    // 初始化读取文件修改时间
    auto mod_time = readFileModTime();
    if (mod_time.has_value()) {
        std::lock_guard<std::mutex> lock(mod_time_mutex_);
        last_mod_time_ = mod_time.value();
    } else {
        AD_WARN(ConfigWatcher, "Failed to read initial modification time for: %s",
                file_path_.c_str());
        // 使用一个默认值
        last_mod_time_ = std::filesystem::file_time_type::min();
    }
}

ConfigWatcher::~ConfigWatcher() {
    stop();
}

bool ConfigWatcher::start() {
    if (running_.load()) {
        AD_WARN(ConfigWatcher, "ConfigWatcher already running for: %s", file_path_.c_str());
        return false;
    }

    running_.store(true);
    watch_thread_ = std::thread(&ConfigWatcher::watchThread, this);

    // AD_INFO(ConfigWatcher, "ConfigWatcher started for: %s (interval: %u ms)",
    //         file_path_.c_str(), check_interval_ms_);
    return true;
}

void ConfigWatcher::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }

    AD_INFO(ConfigWatcher, "ConfigWatcher stopped for: %s", file_path_.c_str());
}

void ConfigWatcher::setChangeCallback(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    change_callback_ = std::move(callback);
}

bool ConfigWatcher::checkChanged() {
    auto new_mod_time = readFileModTime();
    if (!new_mod_time.has_value()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mod_time_mutex_);
    if (new_mod_time.value() > last_mod_time_) {
        last_mod_time_ = new_mod_time.value();
        return true;
    }
    return false;
}

std::filesystem::file_time_type ConfigWatcher::getLastModifiedTime() const {
    std::lock_guard<std::mutex> lock(mod_time_mutex_);
    return last_mod_time_;
}

void ConfigWatcher::watchThread() {
    // AD_INFO(ConfigWatcher, "Watch thread started for: %s", file_path_.c_str());

    while (running_.load()) {
        try {
            if (checkChanged()) {
                AD_WARN(ConfigWatcher, "Config file changed: %s", file_path_.c_str());

                ChangeCallback callback;
                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    callback = change_callback_;
                }

                if (callback) {
                    try {
                        callback(file_path_);
                    } catch (const std::exception& e) {
                        AD_ERROR(ConfigWatcher, "Exception in change callback: %s", e.what());
                    } catch (...) {
                        AD_ERROR(ConfigWatcher, "Unknown exception in change callback");
                    }
                } else {
                    AD_DEBUG(ConfigWatcher, "No callback registered for config change");
                }
            }

        } catch (const std::exception& e) {
            AD_ERROR(ConfigWatcher, "Exception in watch thread: %s", e.what());
        } catch (...) {
            AD_ERROR(ConfigWatcher, "Unknown exception in watch thread");
        }

        // 等待下次检查
        std::unique_lock<std::mutex> lock(mod_time_mutex_);
        // 使用条件变量或 sleep_for 来等待
        lock.unlock();  // 先解锁
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms_));
    }

    AD_INFO(ConfigWatcher, "Watch thread exiting for: %s", file_path_.c_str());
}

std::optional<std::filesystem::file_time_type> ConfigWatcher::readFileModTime() const {
    try {
        std::filesystem::path path(file_path_);
        if (!std::filesystem::exists(path)) {
            AD_DEBUG(ConfigWatcher, "File does not exist: %s", file_path_.c_str());
            return std::nullopt;
        }

        auto ftime = std::filesystem::last_write_time(path);
        return ftime;

    } catch (const std::exception& e) {
        AD_ERROR(ConfigWatcher, "Failed to read file time for %s: %s",
                 file_path_.c_str(), e.what());
        return std::nullopt;
    }
}

} // namespace aurora::collector
