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

void ImageDesensitizer::boxBlurH(uint8_t* data, int width, int height,
                                  int channels, int radius) {
    int diam = radius * 2 + 1;
    std::vector<uint8_t> temp(width * channels);

    for (int y = 0; y < height; ++y) {
        uint8_t* row = data + y * width * channels;
        std::memcpy(temp.data(), row, width * channels);

        for (int x = 0; x < width; ++x) {
            int sum[4] = {};
            int count = 0;
            int x0 = std::max(0, x - radius);
            int x1 = std::min(width - 1, x + radius);

            for (int xi = x0; xi <= x1; ++xi) {
                for (int c = 0; c < channels; ++c) {
                    sum[c] += temp[xi * channels + c];
                }
                ++count;
            }

            for (int c = 0; c < channels; ++c) {
                row[x * channels + c] = static_cast<uint8_t>(sum[c] / count);
            }
        }
    }
}

void ImageDesensitizer::boxBlurV(uint8_t* data, int width, int height,
                                  int channels, int radius) {
    std::vector<uint8_t> temp(height * channels);

    for (int x = 0; x < width; ++x) {
        // Extract column
        for (int y = 0; y < height; ++y) {
            uint8_t* px = data + (y * width + x) * channels;
            for (int c = 0; c < channels; ++c) {
                temp[y * channels + c] = px[c];
            }
        }

        for (int y = 0; y < height; ++y) {
            int sum[4] = {};
            int count = 0;
            int y0 = std::max(0, y - radius);
            int y1 = std::min(height - 1, y + radius);

            for (int yi = y0; yi <= y1; ++yi) {
                for (int c = 0; c < channels; ++c) {
                    sum[c] += temp[yi * channels + c];
                }
                ++count;
            }

            uint8_t* px = data + (y * width + x) * channels;
            for (int c = 0; c < channels; ++c) {
                px[c] = static_cast<uint8_t>(sum[c] / count);
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

        // Apply two-pass box blur on pixel data
        boxBlurH(img.data.data(), width, height, channels, radius);
        boxBlurV(img.data.data(), width, height, channels, radius);

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
