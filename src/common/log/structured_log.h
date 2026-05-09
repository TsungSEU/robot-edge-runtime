// structured_log.h — Structured logging with JSON output and correlation IDs
//
// Extends the AD Logger with JSON-formatted output (JSONL) for machine parsing.
// Maintains full backward compatibility with existing text logs.
//
// Usage:
//   AD_INFO_S(StateMachine, {{"from", "IDLE"}, {"to", "PLANNING"}}, "State transition");
//   AD_ERROR_S(Upload, {{"file", filename}, {"bytes", size}}, "Upload failed");

#ifndef AURORA_COMMON_LOG_STRUCTURED_LOG_H_
#define AURORA_COMMON_LOG_STRUCTURED_LOG_H_

#include <string>
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <random>
#include <mutex>
#include <fstream>

namespace aurora::common {

// ===== Correlation ID (thread-local) =====

class CorrelationId {
public:
    // Generate a new UUID v4-style correlation ID
    static std::string generate() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        uint64_t cnt = counter.fetch_add(1);
        std::ostringstream oss;
        oss << std::hex << (now & 0xFFFF) << "-" << (cnt & 0xFFFF);
        return oss.str();
    }

    // Get/set thread-local correlation ID
    static std::string current() {
        return currentId();
    }

    static void set(const std::string& id) {
        currentId() = id;
    }

    static void clear() {
        currentId().clear();
    }

private:
    static std::string& currentId() {
        static thread_local std::string id;
        return id;
    }
};

// ===== Log Level to string =====

inline const char* logLevelToString(int level) {
    switch (level) {
        case -3: return "NONE";
        case -2: return "ERROR";
        case -1: return "WARN";
        case  0: return "INFO";
        default: return "DEBUG";
    }
}

// ===== JSON escape helper =====

inline std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ===== ISO 8601 timestamp =====

inline std::string iso8601Now() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&time_t_now, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// ===== Structured Log Writer =====
// Writes JSONL to a dedicated file alongside the existing text log.

class StructuredLogWriter {
public:
    static StructuredLogWriter& instance() {
        static StructuredLogWriter writer;
        return writer;
    }

    void init(const std::string& jsonl_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) file_.close();
        file_.open(jsonl_path, std::ios::app);
        enabled_ = file_.is_open();
    }

    void write(int level, const char* tag, const char* file, int line,
               const char* message, const char* context_json = nullptr) {
        if (!enabled_) return;
        std::lock_guard<std::mutex> lock(mutex_);

        // Format as single-line JSON
        file_ << "{\"ts\":\"" << iso8601Now()
              << "\",\"level\":\"" << logLevelToString(level)
              << "\",\"tag\":\"" << jsonEscape(tag)
              << "\",\"msg\":\"" << jsonEscape(message)
              << "\"";

        if (file) {
            // Extract just the filename from path
            const char* basename = file;
            for (const char* p = file; *p; ++p) {
                if (*p == '/') basename = p + 1;
            }
            file_ << ",\"file\":\"" << basename << "\",\"line\":" << line;
        }

        std::string corr_id = CorrelationId::current();
        if (!corr_id.empty()) {
            file_ << ",\"corr_id\":\"" << corr_id << "\"";
        }

        if (context_json && context_json[0] != '\0') {
            file_ << ",\"ctx\":" << context_json;
        }

        file_ << "}\n";
        file_.flush();
    }

    bool isEnabled() const { return enabled_; }

private:
    StructuredLogWriter() = default;
    std::mutex mutex_;
    std::ofstream file_;
    bool enabled_ = false;
};

}  // namespace aurora::common

// ===== Structured logging macros =====
// These extend the existing AD_INFO/AD_ERROR macros with JSON context.
// The text log is still written by the original Logger.
// The JSONL log is written by StructuredLogWriter.

#define AD_INFO_S(TAG, CTX, ...)                                             \
    do {                                                                      \
        AD_INFO(TAG, __VA_ARGS__);                                            \
        if (aurora::common::StructuredLogWriter::instance().isEnabled()) {    \
            char _ad_buf[512];                                                \
            snprintf(_ad_buf, sizeof(_ad_buf), __VA_ARGS__);                  \
            aurora::common::StructuredLogWriter::instance().write(            \
                LOG_LEVEL_INFO, #TAG, __FILE__, __LINE__, _ad_buf, CTX);      \
        }                                                                     \
    } while(0)

#define AD_ERROR_S(TAG, CTX, ...)                                            \
    do {                                                                      \
        AD_ERROR(TAG, __VA_ARGS__);                                           \
        if (aurora::common::StructuredLogWriter::instance().isEnabled()) {    \
            char _ad_buf[512];                                                \
            snprintf(_ad_buf, sizeof(_ad_buf), __VA_ARGS__);                  \
            aurora::common::StructuredLogWriter::instance().write(            \
                LOG_LEVEL_ERROR, #TAG, __FILE__, __LINE__, _ad_buf, CTX);     \
        }                                                                     \
    } while(0)

#define AD_WARN_S(TAG, CTX, ...)                                             \
    do {                                                                      \
        AD_WARN(TAG, __VA_ARGS__);                                            \
        if (aurora::common::StructuredLogWriter::instance().isEnabled()) {    \
            char _ad_buf[512];                                                \
            snprintf(_ad_buf, sizeof(_ad_buf), __VA_ARGS__);                  \
            aurora::common::StructuredLogWriter::instance().write(            \
                LOG_LEVEL_WARNING, #TAG, __FILE__, __LINE__, _ad_buf, CTX);   \
        }                                                                     \
    } while(0)

#endif  // AURORA_COMMON_LOG_STRUCTURED_LOG_H_
