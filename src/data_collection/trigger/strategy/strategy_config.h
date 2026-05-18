//
// Created by xucong on 25-5-6.
// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>

namespace aurora::collector {

struct Trigger {
    std::string triggerId;
    int8_t priority;
    bool enabled;
    std::string triggerCondition;
    std::string triggerDesc;
};

struct CacheMode {
    uint8_t forwardCaptureDurationSec;
    uint8_t backwardCaptureDurationSec;
    uint8_t cooldownDurationSec;
};

struct Mode {
    uint8_t triggerMode;
    CacheMode cacheMode;
};

struct Channel {
    std::string topic;
    std::string type;
    uint8_t originalFrameRate;
    uint8_t capturedFrameRate;
};

struct Cyclone {
    std::vector<Channel> channels;
};

struct MaskingConfig {
    double geospatialOffsetRadius = 10.0;
    int imageBlurKernelSize = 25;
    bool imageDesensitizeDepth = false;
    std::string imageDetectionMode = "full_frame";
};

struct Strategy {
    std::string businessType;
    Trigger trigger;
    Mode mode;
    bool enableMasking;
    MaskingConfig maskingConfig;
    Cyclone cyclone;
    std::unordered_map<std::string, std::string> upload;
};

struct StrategyConfig {
    std::string configId;
    uint64_t strategyId;
    std::vector<Strategy> strategies;
};

}
