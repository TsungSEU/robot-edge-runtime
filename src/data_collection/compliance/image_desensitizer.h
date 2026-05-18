// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

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

    int blurKernelSize() const { return block_size_; }
    const char* redactionMethod() const { return "full_frame_mosaic"; }

private:
    // Irreversible full-frame mosaic fallback. This replaces reversible-looking
    // blur with block-level pixelation when no ROI detector is available.
    void mosaic8(uint8_t* data, int width, int height, int channels, int block_size);
    void mosaic16(uint8_t* data, int width, int height, int channels, int block_size, bool big_endian);

    int block_size_;
};

}  // namespace aurora::collector::compliance
