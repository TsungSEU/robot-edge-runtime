// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <cstddef>

namespace aurora::collector::compliance_v2 {

struct ComplianceWorkerPoolConfig {
    size_t worker_count = 1;
    size_t bounded_queue_size = 64;
    const char* overload_action = "sample_and_redact";
};

}  // namespace aurora::collector::compliance_v2
