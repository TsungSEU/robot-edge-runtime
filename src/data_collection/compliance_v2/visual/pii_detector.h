// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <string>
#include <vector>

namespace aurora::collector::compliance_v2 {

struct PrivacyRoi {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::string pii_class;
    float confidence = 0.0F;
};

class IPiiDetector {
public:
    virtual ~IPiiDetector() = default;
    virtual bool available() const = 0;
    virtual std::vector<PrivacyRoi> detect(const std::vector<uint8_t>& normalized_image,
                                           int width,
                                           int height,
                                           int channels) = 0;
};

// Safe default for deployments without a detector runtime. The V2 processor
// treats this as detector-unavailable and applies the configured fallback
// instead of forwarding raw frames.
class UnavailablePiiDetector final : public IPiiDetector {
public:
    bool available() const override { return false; }
    std::vector<PrivacyRoi> detect(const std::vector<uint8_t>&,
                                   int,
                                   int,
                                   int) override { return {}; }
};

}  // namespace aurora::collector::compliance_v2
