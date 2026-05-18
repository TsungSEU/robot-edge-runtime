// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>

#include "visual_privacy_processor.h"

#include <utility>

#include <cstring>
#include <vector>

#include "common/log/logger.h"
#include "compliance_v2/visual/image_codec_adapter.h"

namespace aurora::collector::compliance_v2 {

VisualPrivacyProcessor::VisualPrivacyProcessor(VisualPolicy policy,
                                               std::unique_ptr<IPiiDetector> detector)
    : policy_(std::move(policy)),
      detector_(std::move(detector)),
      redactor_(policy_.fill_color, policy_.expand_ratio),
      mosaic_fallback_(policy_.fallback_mosaic_block_size) {}

ComplianceDecision VisualPrivacyProcessor::process(const TopicPolicy& topic_policy,
                                                    const rclcpp::SerializedMessage& msg) {
    if (!policy_.enabled) return ComplianceDecision::passThrough(topic_policy.topic);

    if (!detector_ || !detector_->available()) {
        return fallbackMosaic(topic_policy, msg, "detector_unavailable");
    }

    // Detector-backed ROI redaction is intentionally isolated behind IPiiDetector.
    // Deployments can plug in a face/license/text detector without changing the
    // recorder path. Until then, the fail-closed fallback prevents raw frames.
    return fallbackMosaic(topic_policy, msg, "roi_detector_not_configured");
}

ComplianceDecision VisualPrivacyProcessor::fallbackMosaic(const TopicPolicy& topic_policy,
                                                          const rclcpp::SerializedMessage& msg,
                                                          const std::string& reason) {
    if (ImageCodecAdapter::isCompressedImage(topic_policy.message_type)) {
        return ComplianceDecision::drop(topic_policy.topic,
                                        reason + ": compressed image decode unavailable; policy=drop_frame",
                                        "VisualPrivacyProcessorV2");
    }

    if (!ImageCodecAdapter::isRawImage(topic_policy.message_type)) {
        return ComplianceDecision::drop(topic_policy.topic,
                                        reason + ": unsupported visual message type",
                                        "VisualPrivacyProcessorV2");
    }

    const auto& rcl_msg = msg.get_rcl_serialized_message();
    std::vector<uint8_t> buffer(rcl_msg.buffer, rcl_msg.buffer + rcl_msg.buffer_length);
    if (!mosaic_fallback_.desensitize(buffer, topic_policy.topic)) {
        return ComplianceDecision::drop(topic_policy.topic,
                                        reason + ": full_frame_mosaic_failed",
                                        "VisualPrivacyProcessorV2");
    }

    ComplianceDecision decision;
    decision.action = ComplianceAction::ForwardSanitized;
    decision.topic = topic_policy.topic;
    decision.policy_id = "visual-spatial-v2.1";
    decision.processor = "VisualPrivacyProcessorV2";
    decision.reason = reason + ": full_frame_mosaic";
    decision.message = rclcpp::SerializedMessage(buffer.size());
    auto& out = decision.message.get_rcl_serialized_message();
    std::memcpy(out.buffer, buffer.data(), buffer.size());
    out.buffer_length = buffer.size();
    return decision;
}

}  // namespace aurora::collector::compliance_v2
