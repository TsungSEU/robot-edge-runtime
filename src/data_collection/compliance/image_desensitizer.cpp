// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#include "image_desensitizer.h"

#include <algorithm>
#include <cstring>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include "sensor_msgs/msg/image.hpp"
#include "rclcpp/serialization.hpp"

#include "common/log/logger.h"

namespace aurora::collector::compliance {

ImageDesensitizer::ImageDesensitizer(int blur_kernel_size)
    : blur_kernel_size_(blur_kernel_size | 1) {  // Ensure odd
    AD_INFO(ImageDesensitizer, "Initialized: kernel_size=%d", blur_kernel_size_);
}

void ImageDesensitizer::mosaic(uint8_t* data, int width, int height,
                                 int channels, int block_size) {
    for (int by = 0; by < height; by += block_size) {
        for (int bx = 0; bx < width; bx += block_size) {
            const int x_end = std::min(width, bx + block_size);
            const int y_end = std::min(height, by + block_size);
            int sum[4] = {};
            int count = 0;

            for (int y = by; y < y_end; ++y) {
                for (int x = bx; x < x_end; ++x) {
                    const uint8_t* px = data + (y * width + x) * channels;
                    for (int c = 0; c < channels; ++c) {
                        sum[c] += px[c];
                    }
                    ++count;
                }
            }

            uint8_t avg[4] = {};
            for (int c = 0; c < channels; ++c) {
                avg[c] = static_cast<uint8_t>(sum[c] / std::max(1, count));
            }

            for (int y = by; y < y_end; ++y) {
                for (int x = bx; x < x_end; ++x) {
                    uint8_t* px = data + (y * width + x) * channels;
                    for (int c = 0; c < channels; ++c) {
                        px[c] = avg[c];
                    }
                }
            }
        }
    }
}

bool ImageDesensitizer::desensitize(std::vector<uint8_t>& cdr_buffer,
                                     const std::string& topic) {
    try {
        // Deserialize CDR into Image message
        rclcpp::SerializedMessage serialized_msg(cdr_buffer.size());
        auto& rcl_msg = serialized_msg.get_rcl_serialized_message();
        std::memcpy(rcl_msg.buffer, cdr_buffer.data(), cdr_buffer.size());
        rcl_msg.buffer_length = cdr_buffer.size();

        rclcpp::Serialization<sensor_msgs::msg::Image> serializer;
        sensor_msgs::msg::Image img;
        serializer.deserialize_message(&serialized_msg, &img);

        // Determine channels from encoding
        int channels = 1;
        const auto& enc = img.encoding;
        if (enc == "rgb8" || enc == "bgr8" || enc == "rgba8" || enc == "bgra8") {
            channels = (enc == "rgba8" || enc == "bgra8") ? 4 : 3;
        } else if (enc == "mono8" || enc == "8UC1") {
            channels = 1;
        } else if (enc.find("16") != std::string::npos) {
            // 16-bit encodings (e.g. depth) — skip if not configured
            return false;
        }

        int width = static_cast<int>(img.width);
        int height = static_cast<int>(img.height);
        int radius = blur_kernel_size_ / 2;

        if (width <= 0 || height <= 0 || radius <= 0 ||
            static_cast<int>(img.data.size()) < width * height * channels) {
            return false;
        }

        // Apply irreversible full-frame mosaic on pixel data.
        // This is the configured fallback until ROI-based detectors are wired in.
        mosaic(img.data.data(), width, height, channels, blur_kernel_size_);

        // Re-serialize
        rclcpp::SerializedMessage output_msg(cdr_buffer.size() + 64);
        serializer.serialize_message(&img, &output_msg);

        auto& out_rcl = output_msg.get_rcl_serialized_message();
        cdr_buffer.assign(out_rcl.buffer, out_rcl.buffer + out_rcl.buffer_length);
        return true;
    } catch (const std::exception& e) {
        AD_WARN(ImageDesensitizer, "Failed to desensitize image on %s: %s",
                topic.c_str(), e.what());
        return false;
    }
}

}  // namespace aurora::collector::compliance
