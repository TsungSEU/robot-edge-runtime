// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#include "privacy_audit_writer.h"

#include "common/audit/audit_logger.h"

namespace aurora::collector::compliance_v2 {

void PrivacyAuditWriter::writeEvent(const std::string& component, const std::string& payload) {
    aurora::common::audit::AuditLogger::instance().log(
        aurora::common::audit::AuditEventType::COMPLIANCE_MANIFEST_GENERATED,
        component,
        payload);
}

}  // namespace aurora::collector::compliance_v2
