// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#include "image_desensitizer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include "sensor_msgs/msg/image.hpp"
#include "rclcpp/serialization.hpp"

#include "common/log/logger.h"

namespace aurora::collector::compliance {
namespace {

uint16_t readUint16(const uint8_t* data, bool big_endian) {
    if (big_endian) {
        return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
    }
    return static_cast<uint16_t>((static_cast<uint16_t>(data[1]) << 8) | data[0]);
}

void writeUint16(uint8_t* data, uint16_t value, bool big_endian) {
    if (big_endian) {
        data[0] = static_cast<uint8_t>((value >> 8) & 0xff);
        data[1] = static_cast<uint8_t>(value & 0xff);
    } else {
        data[0] = static_cast<uint8_t>(value & 0xff);
        data[1] = static_cast<uint8_t>((value >> 8) & 0xff);
    }
}

}  // namespace

ImageDesensitizer::ImageDesensitizer(int blur_kernel_size)
    : block_size_(std::max(3, blur_kernel_size | 1)) {  // Ensure odd and non-trivial.
    AD_INFO(ImageDesensitizer, "Initialized: method=full_frame_mosaic, block_size=%d", block_size_);
}

void ImageDesensitizer::mosaic8(uint8_t* data, int width, int height,
                                int channels, int block_size) {
    for (int by = 0; by < height; by += block_size) {
        for (int bx = 0; bx < width; bx += block_size) {
            const int x_end = std::min(width, bx + block_size);
            const int y_end = std::min(height, by + block_size);
            uint64_t sum[4] = {};
            uint64_t count = 0;

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
                avg[c] = static_cast<uint8_t>(sum[c] / std::max<uint64_t>(1, count));
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

void ImageDesensitizer::mosaic16(uint8_t* data, int width, int height,
                                 int channels, int block_size, bool big_endian) {
    constexpr int bytes_per_channel = 2;
    const int bytes_per_pixel = channels * bytes_per_channel;

    for (int by = 0; by < height; by += block_size) {
        for (int bx = 0; bx < width; bx += block_size) {
            const int x_end = std::min(width, bx + block_size);
            const int y_end = std::min(height, by + block_size);
            uint64_t sum[4] = {};
            uint64_t count = 0;

            for (int y = by; y < y_end; ++y) {
                for (int x = bx; x < x_end; ++x) {
                    const uint8_t* px = data + (y * width + x) * bytes_per_pixel;
                    for (int c = 0; c < channels; ++c) {
                        sum[c] += readUint16(px + c * bytes_per_channel, big_endian);
                    }
                    ++count;
                }
            }

            uint16_t avg[4] = {};
            for (int c = 0; c < channels; ++c) {
                avg[c] = static_cast<uint16_t>(sum[c] / std::max<uint64_t>(1, count));
            }

            for (int y = by; y < y_end; ++y) {
                for (int x = bx; x < x_end; ++x) {
                    uint8_t* px = data + (y * width + x) * bytes_per_pixel;
                    for (int c = 0; c < channels; ++c) {
                        writeUint16(px + c * bytes_per_channel, avg[c], big_endian);
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

        int channels = 1;
        int bytes_per_channel = 1;
        const auto& enc = img.encoding;
        if (enc == "rgb8" || enc == "bgr8" || enc == "rgba8" || enc == "bgra8" ||
            enc == "8UC3" || enc == "8UC4") {
            channels = (enc == "rgba8" || enc == "bgra8" || enc == "8UC4") ? 4 : 3;
        } else if (enc == "mono8" || enc == "8UC1") {
            channels = 1;
        } else if (enc == "mono16" || enc == "16UC1") {
            channels = 1;
            bytes_per_channel = 2;
        } else if (enc == "16UC3") {
            channels = 3;
            bytes_per_channel = 2;
        } else if (enc == "16UC4") {
            channels = 4;
            bytes_per_channel = 2;
        } else {
            AD_WARN(ImageDesensitizer, "Unsupported image encoding on %s: %s",
                    topic.c_str(), enc.c_str());
            return false;
        }

        const int width = static_cast<int>(img.width);
        const int height = static_cast<int>(img.height);
        const int bytes_per_pixel = channels * bytes_per_channel;

        if (width <= 0 || height <= 0 ||
            img.data.size() < static_cast<size_t>(width) * height * bytes_per_pixel) {
            return false;
        }

        // Apply irreversible full-frame mosaic on pixel data.
        // This is the configured fallback until ROI-based detectors are wired in.
        if (bytes_per_channel == 1) {
            mosaic8(img.data.data(), width, height, channels, block_size_);
        } else {
            mosaic16(img.data.data(), width, height, channels, block_size_, img.is_bigendian != 0);
        }

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
