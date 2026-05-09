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
    // Two-pass separable box blur (no OpenCV)
    void boxBlurH(uint8_t* data, int width, int height, int channels, int radius);
    void boxBlurV(uint8_t* data, int width, int height, int channels, int radius);

    int blur_kernel_size_;
};

}  // namespace aurora::collector::compliance
