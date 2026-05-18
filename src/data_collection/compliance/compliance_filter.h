// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>

#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "channel/observer.h"
#include "compliance_config.h"
#include "geospatial_obfuscator.h"
#include "image_desensitizer.h"
#include "compliance_v2/runtime/compliance_filter_v2.h"

namespace aurora::collector::compliance {

class ComplianceFilter : public Observer {
public:
    ComplianceFilter(std::shared_ptr<rclcpp::Node> node,
                     const ComplianceConfig& config);

    ~ComplianceFilter() override = default;

    void OnMessageReceived(const std::string& topic,
                           const rclcpp::SerializedMessage& msg) override;

    void setDownstream(const std::shared_ptr<Observer>& downstream);

    const ComplianceConfig& config() const { return config_; }

private:
    bool isOdomTopic(const std::string& topic) const;
    bool isImageTopic(const std::string& topic) const;
    bool isDepthTopic(const std::string& topic) const;

    std::shared_ptr<rclcpp::Node> node_;
    ComplianceConfig config_;
    std::shared_ptr<Observer> downstream_;
    std::unique_ptr<GeospatialObfuscator> geo_;
    std::unique_ptr<ImageDesensitizer> image_;
    std::unique_ptr<::aurora::collector::compliance_v2::ComplianceFilterV2> v2_filter_;
};

}  // namespace aurora::collector::compliance
