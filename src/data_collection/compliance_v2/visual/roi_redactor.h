// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <utility>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "compliance_v2/visual/pii_detector.h"

namespace aurora::collector::compliance_v2 {

class RoiRedactor {
public:
    explicit RoiRedactor(std::vector<uint8_t> fill_color = {0, 0, 0}, double expand_ratio = 0.25)
        : fill_color_(std::move(fill_color)), expand_ratio_(expand_ratio) {}

    void solidFill(std::vector<uint8_t>& pixels,
                   int width,
                   int height,
                   int channels,
                   const std::vector<PrivacyRoi>& rois) const {
        if (width <= 0 || height <= 0 || channels <= 0) return;
        for (const auto& roi : rois) {
            const int expand_x = static_cast<int>(roi.width * expand_ratio_);
            const int expand_y = static_cast<int>(roi.height * expand_ratio_);
            const int x0 = std::max(0, roi.x - expand_x);
            const int y0 = std::max(0, roi.y - expand_y);
            const int x1 = std::min(width, roi.x + roi.width + expand_x);
            const int y1 = std::min(height, roi.y + roi.height + expand_y);
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    const size_t base = (static_cast<size_t>(y) * width + x) * channels;
                    for (int c = 0; c < channels; ++c) {
                        pixels[base + c] = fill_color_.empty()
                            ? 0
                            : fill_color_[static_cast<size_t>(c) % fill_color_.size()];
                    }
                }
            }
        }
    }

private:
    std::vector<uint8_t> fill_color_;
    double expand_ratio_;
};

}  // namespace aurora::collector::compliance_v2
