// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <string>

namespace aurora::collector::compliance_v2 {

class ImageCodecAdapter {
public:
    static bool isRawImage(const std::string& message_type) {
        return message_type.find("sensor_msgs/msg/Image") != std::string::npos &&
               message_type.find("CompressedImage") == std::string::npos;
    }

    static bool isCompressedImage(const std::string& message_type) {
        return message_type.find("sensor_msgs/msg/CompressedImage") != std::string::npos;
    }
};

}  // namespace aurora::collector::compliance_v2
