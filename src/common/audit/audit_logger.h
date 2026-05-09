// audit_logger.h — Audit logging framework for Aurora Edge Runtime
//
// Provides a structured audit trail for key system events:
// state transitions, recording start/stop, triggers, config reloads, errors, QoS violations.
// Writes JSONL to a dedicated audit log file with sequence numbers and correlation IDs.

#ifndef AURORA_COMMON_AUDIT_AUDIT_LOGGER_H_
#define AURORA_COMMON_AUDIT_AUDIT_LOGGER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <fstream>
#include <atomic>

#include "common/log/structured_log.h"

namespace aurora::common::audit {

enum class AuditEventType : uint8_t {
    STATE_TRANSITION,
    RECORDING_START,
    RECORDING_STOP,
    TRIGGER_FIRED,
    CONFIG_RELOAD,
    ERROR_OCCURRED,
    QOS_VIOLATION,
    UPLOAD_START,
    UPLOAD_COMPLETE,
    COMPLIANCE_GEO_OBFUSCATION_APPLIED,
    COMPLIANCE_IMAGE_DESENSITIZED,
    COMPLIANCE_MANIFEST_GENERATED,
};

inline const char* auditEventTypeToString(AuditEventType type) {
    switch (type) {
        case AuditEventType::STATE_TRANSITION:  return "STATE_TRANSITION";
        case AuditEventType::RECORDING_START:   return "RECORDING_START";
        case AuditEventType::RECORDING_STOP:    return "RECORDING_STOP";
        case AuditEventType::TRIGGER_FIRED:     return "TRIGGER_FIRED";
        case AuditEventType::CONFIG_RELOAD:     return "CONFIG_RELOAD";
        case AuditEventType::ERROR_OCCURRED:    return "ERROR_OCCURRED";
        case AuditEventType::QOS_VIOLATION:     return "QOS_VIOLATION";
        case AuditEventType::UPLOAD_START:      return "UPLOAD_START";
        case AuditEventType::UPLOAD_COMPLETE:   return "UPLOAD_COMPLETE";
        case AuditEventType::COMPLIANCE_GEO_OBFUSCATION_APPLIED: return "COMPLIANCE_GEO_OBFUSCATION_APPLIED";
        case AuditEventType::COMPLIANCE_IMAGE_DESENSITIZED: return "COMPLIANCE_IMAGE_DESENSITIZED";
        case AuditEventType::COMPLIANCE_MANIFEST_GENERATED: return "COMPLIANCE_MANIFEST_GENERATED";
        default: return "UNKNOWN";
    }
}

class AuditLogger {
public:
    static AuditLogger& instance() {
        static AuditLogger logger;
        return logger;
    }

    // Initialize with audit log file path
    void init(const std::string& audit_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) file_.close();
        file_.open(audit_path, std::ios::app);
        enabled_ = file_.is_open();
    }

    // Log an audit event with a JSON context string
    void log(AuditEventType type, const char* actor, const std::string& context_json) {
        if (!enabled_) return;
        std::lock_guard<std::mutex> lock(mutex_);

        uint64_t seq = sequence_.fetch_add(1);
        std::string corr_id = CorrelationId::current();

        file_ << "{\"seq\":" << seq
              << ",\"ts\":\"" << iso8601Now() << "\""
              << ",\"event\":\"" << auditEventTypeToString(type) << "\""
              << ",\"actor\":\"" << jsonEscape(actor) << "\"";

        if (!corr_id.empty()) {
            file_ << ",\"corr_id\":\"" << corr_id << "\"";
        }

        file_ << ",\"data\":" << context_json
              << "}\n";

        // Flush periodically (every 10 entries or on state transitions)
        if (seq % 10 == 0 || type == AuditEventType::STATE_TRANSITION) {
            file_.flush();
        }
    }

    bool isEnabled() const { return enabled_; }

private:
    AuditLogger() = default;
    std::mutex mutex_;
    std::ofstream file_;
    std::atomic<uint64_t> sequence_{0};
    bool enabled_ = false;
};

// Convenience macro for audit logging
#define AUDIT_LOG(EVENT_TYPE, ACTOR, CONTEXT_JSON)                           \
    do {                                                                      \
        if (aurora::common::audit::AuditLogger::instance().isEnabled()) {     \
            aurora::common::audit::AuditLogger::instance().log(               \
                aurora::common::audit::AuditEventType::EVENT_TYPE,            \
                ACTOR, CONTEXT_JSON);                                          \
        }                                                                     \
    } while(0)

}  // namespace aurora::common::audit

#endif  // AURORA_COMMON_AUDIT_AUDIT_LOGGER_H_
