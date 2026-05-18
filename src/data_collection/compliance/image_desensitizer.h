// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aurora::collector::compliance {

class ImageDesensitizer {
public:
    explicit ImageDesensitizer(int blur_kernel_size);

    bool desensitize(std::vector<uint8_t>& cdr_buffer,
                     const std::string& topic);

    int blurKernelSize() const { return blur_kernel_size_; }

private:
    // Irreversible full-frame mosaic fallback. This replaces reversible-looking
    // blur with block-level pixelation when no ROI detector is available.
    void mosaic(uint8_t* data, int width, int height, int channels, int block_size);

    int blur_kernel_size_;
};

}  // namespace aurora::collector::compliance
