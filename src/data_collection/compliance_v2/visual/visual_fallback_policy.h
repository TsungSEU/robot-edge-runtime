// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <string>

namespace aurora::collector::compliance_v2 {

struct VisualFallbackPolicy {
    std::string on_detector_unavailable = "full_frame_mosaic";
    std::string on_processing_error = "drop_frame";
    std::string on_overload = "sample_and_redact";

    bool detectorUnavailableUsesMosaic() const {
        return on_detector_unavailable == "full_frame_mosaic";
    }

    bool processingErrorDropsFrame() const {
        return on_processing_error == "drop_frame";
    }
};

}  // namespace aurora::collector::compliance_v2
