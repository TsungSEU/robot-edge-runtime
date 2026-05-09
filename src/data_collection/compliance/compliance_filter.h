// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "channel/observer.h"
#include "compliance_config.h"
#include "geospatial_obfuscator.h"
#include "image_desensitizer.h"

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
    double geoOffsetX() const { return geo_ ? geo_->offsetX() : 0.0; }
    double geoOffsetY() const { return geo_ ? geo_->offsetY() : 0.0; }

private:
    bool isOdomTopic(const std::string& topic) const;
    bool isImageTopic(const std::string& topic) const;
    bool isDepthTopic(const std::string& topic) const;

    std::shared_ptr<rclcpp::Node> node_;
    ComplianceConfig config_;
    std::shared_ptr<Observer> downstream_;
    std::unique_ptr<GeospatialObfuscator> geo_;
    std::unique_ptr<ImageDesensitizer> image_;
};

}  // namespace aurora::collector::compliance
