//
// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#include "ros2bag_recorder.h"

#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>

#include "rosbag2_cpp/writers/sequential_writer.hpp"
#include "rosbag2_storage/serialized_bag_message.hpp"
#include "rosbag2_storage/topic_metadata.hpp"
#include "rcutils/error_handling.h"
#include "rcutils/allocator.h"
#include "common/utils/utils.h"

namespace aurora::collector {

Ros2BagRecorder::Ros2BagRecorder(std::shared_ptr<rclcpp:: Node> node)
    : node_(node),
      current_mode_(OptMode::WRITE),
      is_initialized_(false),
      is_opened_(false),
      has_data_written_(false),
      max_bag_size_mb_(0),
      messages_since_last_log_(0) {
  bag_info_.start_time = std::chrono::system_clock::now();
  last_log_time_ = std::chrono::steady_clock::now();
}

Ros2BagRecorder::~Ros2BagRecorder() {
  if (is_opened_) {
    Close();
  }
}

bool Ros2BagRecorder::Init() {
  if (is_initialized_) {
    RCLCPP_WARN(node_->get_logger(), "Recorder already initialized");
    return true;
  }

  is_initialized_ = true;
  RCLCPP_INFO(node_->get_logger(),
              "Ros2BagRecorder initialized successfully");
  return true;
}


bool Ros2BagRecorder::InitRingBuffers() {
  // 清空旧的缓冲区
  forward_ringbuffers_.clear();
  backward_ringbuffers_.clear();

  CacheMode cache_mode = strategy_->mode.cacheMode;

  for (const auto& channel : strategy_->cyclone.channels) {
    const std::string& topic = channel.topic;
    if (channel.originalFrameRate <=0 || channel.capturedFrameRate <=0)
    {
      return false;
    }

    int32_t forward_size = cache_mode_.forwardCaptureDurationSec * channel.capturedFrameRate;
    int32_t backward_size = cache_mode_.backwardCaptureDurationSec * channel.capturedFrameRate;

    auto forward_buf = std::make_unique<BufferType>(forward_size);
    if (!forward_buf) {
      RCLCPP_ERROR(node_->get_logger(), "Create forward buffer failed for topic: %s", topic.c_str());
      return false;
    }

    auto backward_buf = std::make_unique<BufferType>(backward_size);
    if (!backward_buf) {
      RCLCPP_ERROR(node_->get_logger(), "Create backward buffer failed for topic: %s", topic.c_str());
      return false;
    }

    forward_ringbuffers_[topic] = std::move(forward_buf);
    backward_ringbuffers_[topic] = std::move(backward_buf);
    // RCLCPP_INFO(node_->get_logger(), "Init buffer for topic: %s, forward size: %d, backward size: %d", topic.c_str(), forward_size, backward_size);
  }
  return true;
}

bool Ros2BagRecorder::Open(OptMode opt_mode, const std::string& full_path) {
  if (!is_initialized_) {
    RCLCPP_ERROR(node_->get_logger(),
                 "Recorder must be initialized before opening");
    return false;
  }

  if (is_opened_) {
    RCLCPP_WARN(node_->get_logger(), "Bag already open, closing first");
    if (! Close()) {
      RCLCPP_ERROR(node_->get_logger(), "Failed to close previous bag");
      return false;
    }
  }

  try {
    current_mode_ = opt_mode;
    current_bag_path_ = full_path;

    if (opt_mode == OptMode::WRITE) {
      // 如果目录已存在，先删除
      std::filesystem::path bag_dir(full_path);
      if (std::filesystem::exists(bag_dir)) {
        RCLCPP_WARN(node_->get_logger(), "Removing existing bag directory: %s", full_path.c_str());
        std::filesystem::remove_all(bag_dir);
      }

      // 确保父目录存在
      std::filesystem::path parent_dir = bag_dir.parent_path();
      if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
        std::error_code ec;
        std::filesystem::create_directories(parent_dir, ec);
        if (ec) {
          RCLCPP_ERROR(node_->get_logger(), "Failed to create directory %s: %s",
                       parent_dir.c_str(), ec.message().c_str());
          return false;
        }
      }

      // 检查磁盘剩余空间（至少需要 256MB）
      {
        std::error_code ec;
        auto space_info = std::filesystem::space(parent_dir, ec);
        if (!ec && space_info.available < 256ULL * 1024 * 1024) {
          RCLCPP_ERROR(node_->get_logger(),
                       "Insufficient disk space: %.1f MB available (need 256 MB)",
                       space_info.available / (1024.0 * 1024.0));
          return false;
        }
      }

      rosbag2_storage::StorageOptions storage_options;
      storage_options.uri = full_path;
      storage_options.storage_id = "sqlite3";
      if (max_bag_size_mb_ > 0) {
        storage_options.max_bagfile_size = max_bag_size_mb_ * 1024 * 1024;
      }

      rosbag2_cpp::ConverterOptions converter_options;
      converter_options.input_serialization_format =
          rmw_get_serialization_format();
      converter_options.output_serialization_format =
          rmw_get_serialization_format();

      writer_ = std::make_unique<rosbag2_cpp::Writer>(std::make_unique<rosbag2_cpp::writers::SequentialWriter>());
      writer_->open(storage_options, converter_options);

      // Create all topics from strategy configuration
      if (strategy_) {
        for (const auto& channel : strategy_->cyclone.channels) {
          rosbag2_storage::TopicMetadata topic_metadata;
          topic_metadata.name = channel.topic;
          topic_metadata.type = channel.type;
          topic_metadata.serialization_format = rmw_get_serialization_format();
          writer_->create_topic(topic_metadata);
          RCLCPP_DEBUG(node_->get_logger(), "Created topic: %s (type: %s)",
                      topic_metadata.name.c_str(), topic_metadata.type.c_str());
        }
        bag_info_.num_topics = strategy_->cyclone.channels.size();
      }

      bag_info_ = TBagInfo{};  // Reset all statistics
      bag_info_.bag_path = full_path;
      bag_info_.is_opened = true;
      bag_info_.mode = OptMode::WRITE;
      bag_info_.start_time = std::chrono::system_clock::now();
      bag_info_.serialization_format = rmw_get_serialization_format();

      RCLCPP_DEBUG(node_->get_logger(), "Opened bag for writing at: %s",
                  full_path.c_str());
    } else if (opt_mode == OptMode:: READ) {
      // TODO: Implement READ mode with rosbag2_cpp:: SequentialReader
      RCLCPP_ERROR(node_->get_logger(),
                   "READ mode not yet implemented");
      return false;
    }

    is_opened_ = true;
    has_data_written_ = false;
    return true;

  } catch (const std::exception& e) {
    RCLCPP_ERROR(node_->get_logger(), "Failed to open bag: %s", e.what());
    is_opened_ = false;
    return false;
  }
}

bool Ros2BagRecorder::IsOpened() const { return is_opened_; }

bool Ros2BagRecorder::Write(const std::string& topic_name,
                               uint64_t timestamp, const void* buf,
                               size_t buf_len) {
  if (!is_opened_ || !writer_) {
    RCLCPP_ERROR(node_->get_logger(),
                 "Cannot write: bag not open or writer not initialized");
    return false;
  }

  if (!buf || buf_len == 0) {
    RCLCPP_WARN(node_->get_logger(), "Invalid message data for topic %s",
                topic_name.c_str());
    return false;
  }

  try {
    // Create bag message
    auto bag_msg =
        std::make_shared<rosbag2_storage::SerializedBagMessage>();

    bag_msg->topic_name = topic_name;

    // Allocate and initialize a new rcutils_uint8_array_t with its own buffer
    // This avoids the double-free issue caused by sharing a buffer with rclcpp::SerializedMessage
    bag_msg->serialized_data = std::shared_ptr<rcutils_uint8_array_t>(
        new rcutils_uint8_array_t,
        [](rcutils_uint8_array_t* data) {
          if (data->buffer) {
            // Free the allocated buffer
            if (data->allocator.deallocate) {
              data->allocator.deallocate(data->allocator.state, data->buffer);
            } else {
              // Fallback to default delete if no custom allocator
              delete[] reinterpret_cast<uint8_t*>(data->buffer);
            }
            data->buffer = nullptr;
          }
          delete data;
        });

    // Initialize the array structure
    auto& rcl_msg = *bag_msg->serialized_data;
    rcl_msg.buffer = nullptr;
    rcl_msg.buffer_length = 0;
    rcl_msg.buffer_capacity = 0;
    rcl_msg.allocator = rcutils_get_default_allocator();

    // Allocate buffer with capacity equal to buf_len
    // Note: rcutils allocate signature is: allocate(size, state)
    rcl_msg.buffer_capacity = buf_len;
    rcl_msg.buffer = static_cast<uint8_t*>(
        rcl_msg.allocator.allocate(buf_len, rcl_msg.allocator.state));
    rcl_msg.buffer_length = buf_len;

    // Copy data into the new buffer
    std::memcpy(rcl_msg.buffer, buf, buf_len);

    // Set timestamp (convert from microseconds to nanoseconds for rosbag2)
    bag_msg->time_stamp = timestamp * 1000ULL;

    // RCLCPP_INFO(node_->get_logger(), "Before writer_->write");
    // Write to bag
    writer_->write(bag_msg);
    // RCLCPP_INFO(node_->get_logger(), "After writer_->write");

    has_data_written_ = true;
    update_statistics(topic_name, timestamp, buf_len);

    // RCLCPP_INFO(node_->get_logger(), "Write completed successfully");
    return true;

  } catch (const std::exception& e) {
    RCLCPP_ERROR(node_->get_logger(),
                 "Error writing message from %s: %s", topic_name.c_str(),
                 e.what());
    return false;
  }
}

TBagInfo Ros2BagRecorder::GetBagInfo() const {
  TBagInfo info = bag_info_;
  info.end_time = std::chrono:: system_clock::now();
  return info;
}

bool Ros2BagRecorder::ReadNextFrame(ReadedMessage* message) {
  if (!is_opened_ || current_mode_ != OptMode::READ) {
    RCLCPP_ERROR(node_->get_logger(),
                 "Cannot read: bag not open in READ mode");
    return false;
  }

  if (!message) {
    return false;
  }

  // TODO: Implement reading logic
  RCLCPP_ERROR(node_->get_logger(), "READ mode not yet implemented");
  return false;
}

bool Ros2BagRecorder::Close() {
  if (! is_opened_) {
    return true;
  }

  try {
    if (writer_) {
      writer_. reset();
    }

    is_opened_ = false;
    bag_info_.is_opened = false;
    bag_info_.end_time = std::chrono:: system_clock::now();

    double data_duration = (bag_info_.end_timestamp > 0 && bag_info_.start_timestamp > 0)
                               ? (bag_info_.end_timestamp - bag_info_.start_timestamp) / 1000000.0
                               : std::chrono::duration<double>(bag_info_.end_time -
                                                               bag_info_.start_time).count();
    RCLCPP_INFO(node_->get_logger(),
                "Bag closed.  Recorded %zu messages in %.2f seconds",
                bag_info_.total_messages,
                data_duration);

    return true;

  } catch (const std::exception& e) {
    RCLCPP_ERROR(node_->get_logger(), "Error closing bag:   %s", e.what());
    return false;
  }
}

bool Ros2BagRecorder::HasDataWritten() const { return has_data_written_; }

void Ros2BagRecorder::snapshotForwardBuffers(
    uint64_t trigger_timestamp,
    std::unordered_map<std::string, std::vector<TimestampedData>>& out_buffers) {
  std::lock_guard<std::mutex> lock(buffer_mutex_);
  uint64_t forward_duration_us = cache_mode_.forwardCaptureDurationSec * 1000000ULL;

  for (const auto& pair : forward_ringbuffers_) {
    const std::string& topic = pair.first;
    auto& buffer = pair.second;

    std::vector<TimestampedData> saved_data;
    const auto buffer_snapshot = buffer->snapshot();
    for (const auto& data : buffer_snapshot) {
      if (data.timestamp <= trigger_timestamp &&
          (trigger_timestamp - data.timestamp) <= forward_duration_us) {
        saved_data.push_back(data);
      }
    }
    out_buffers[topic] = std::move(saved_data);
  }
}

bool Ros2BagRecorder::TriggerRecord(
    uint64_t trigger_timestamp,
    const std::string& output_file_path,
    std::unordered_map<std::string, std::vector<TimestampedData>> saved_forward_buffers) {
  auto& sm = state_machine::StateMachine::getInstance();
  if (sm.getCurrentState() == state_machine::SystemState::DATA_COLLECTING) {
    RCLCPP_WARN(node_->get_logger(), "Trigger ignored: bag already triggered");
    return false;
  }

  trigger_timestamp_ = trigger_timestamp;
  is_cancelled_.store(false);

  // Use pre-saved forward buffers (snapshotted on executor thread)
  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    forward_capture_duration_us_ = cache_mode_.forwardCaptureDurationSec * 1000000ULL;
    triggered_forward_buffers_ = std::move(saved_forward_buffers);
    // Clear backward buffers from previous recording so we collect fresh data
    for (auto& pair : backward_ringbuffers_) {
      pair.second->clear();
    }
  }

  // Set state to TRIGGERED so that OnMessageReceived knows to populate backward ring buffer
  sm.setCurrentState(state_machine::SystemState::DATA_COLLECTING);

  // Dynamic backward wait: if this task was queued and delayed, reduce wait time
  // to account for elapsed time since trigger
  uint32_t backward_duration_ms = cache_mode_.backwardCaptureDurationSec * 1000;
  uint64_t now_us = common::GetCurrentTimestamp();
  if (now_us > trigger_timestamp) {
    uint64_t elapsed_ms = (now_us - trigger_timestamp) / 1000;
    if (elapsed_ms >= backward_duration_ms) {
      backward_duration_ms = 0;
    } else {
      backward_duration_ms -= static_cast<uint32_t>(elapsed_ms);
    }
  }

  std::unique_lock<std::mutex> cancel_lock(cancel_mutex_);
  if (backward_duration_ms > 0) {
    cv_cancel_.wait_for(cancel_lock, std::chrono::milliseconds(backward_duration_ms),
                                       [this] { return is_cancelled_.load(); });
  }

  if (!is_cancelled_.load()) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    write_ringbuffer(output_file_path);
    triggered_forward_buffers_.clear();
  } else {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    RCLCPP_INFO(node_->get_logger(), "Recording was cancelled before completion");
  }

  triggered_forward_buffers_.clear();
  sm.setCurrentState(state_machine::SystemState::IDLE);

  forward_capture_duration_us_ = 0;

  return true;

}

bool Ros2BagRecorder::TriggerRecord(uint64_t trigger_timestamp,
                                    const std::string& output_file_path) {
  // Synchronous path: extract forward buffers now, then delegate
  std::unordered_map<std::string, std::vector<TimestampedData>> saved;
  snapshotForwardBuffers(trigger_timestamp, saved);
  return TriggerRecord(trigger_timestamp, output_file_path, std::move(saved));
}

void Ros2BagRecorder::BreakOffRecord() {
  RCLCPP_INFO(node_->get_logger(), "Breaking off current recording");
  is_cancelled_.store(true);
  cv_cancel_.notify_all();
  // {
  //     std::lock_guard<std::mutex> lock(buffer_mutex_);
  //     triggered_forward_buffers_.clear();
  // }

  // std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 确保录制线程有时间响应取消请求
  // is_cancelled_.store(false);
  RCLCPP_INFO(node_->get_logger(), "Break off record completed");
}

bool Ros2BagRecorder::write_ringbuffer(const std::string& outputfilePath) {
    RCLCPP_DEBUG(node_->get_logger(), "write_ringbuffer: Starting, output=%s", outputfilePath.c_str());
    uint64_t min_timestamp = UINT64_MAX;
    uint64_t max_timestamp = 0;

    RCLCPP_DEBUG(node_->get_logger(), "write_ringbuffer: Before Open()");
    if (!Open(OptMode::WRITE, outputfilePath)) {
      RCLCPP_ERROR(node_->get_logger(), "Failed to open bag:   %s", outputfilePath.c_str());
      return false;
    }
    RCLCPP_INFO(node_->get_logger(), "write_ringbuffer: bag opened successfully");

    RCLCPP_DEBUG(node_->get_logger(), "Starting ring buffer write to: %s", outputfilePath.c_str());

    // Calculate time bounds in microseconds (trigger_timestamp_ is in microseconds)
    uint64_t start_time = trigger_timestamp_ - (cache_mode_.forwardCaptureDurationSec * 1000000ULL);
    uint64_t end_time = trigger_timestamp_ + (cache_mode_.backwardCaptureDurationSec * 1000000ULL);

    size_t total_forward_messages = 0;
    size_t total_backward_messages = 0;

    RCLCPP_DEBUG(node_->get_logger(), "write_ringbuffer: Before channel loop");
    for (const auto& channel : strategy_->cyclone.channels) {
        const std::string& topic = channel.topic;
        // RCLCPP_INFO(node_->get_logger(), "write_ringbuffer: Processing topic=%s", topic.c_str());
        auto forward_it = triggered_forward_buffers_.find(topic);
        auto backward_it = backward_ringbuffers_.find(topic);
        auto current_forward_it = forward_ringbuffers_.find(topic);

        if (forward_it == triggered_forward_buffers_.end() && backward_it == backward_ringbuffers_.end()) {
          RCLCPP_WARN(node_->get_logger(), "No buffer found for topic: %s", topic.c_str());
          continue;
        }

        size_t forward_count = 0;
        size_t backward_count = 0;

        std::unordered_set<uint64_t> written_timestamps;
        if (forward_it != triggered_forward_buffers_.end()) {
          for (const auto& data : forward_it->second) {
            if (data.timestamp <= trigger_timestamp_ && data.timestamp >= start_time) {
              min_timestamp = std::min(min_timestamp, data.timestamp);
              max_timestamp = std::max(max_timestamp, data.timestamp);
              // Ensure buffer is not empty before writing
              if (!data.buffer.empty()) {
                if (Write(topic, data.timestamp, data.buffer.data(), data.buffer.size())) {
                  written_timestamps.insert(data.timestamp);
                  forward_count++;
                  total_forward_messages++;
                } else {
                  RCLCPP_ERROR(node_->get_logger(), "Failed to write message for topic: %s", topic.c_str());
                }
              }
            }
          }
        }

        // 处理当前前向缓冲区中的数据
        if (current_forward_it != forward_ringbuffers_.end() && forward_it != triggered_forward_buffers_.end()) {
          const auto current_buffer_snapshot = current_forward_it->second->snapshot();
          for (const auto& data : current_buffer_snapshot) {
            if (data.timestamp <= trigger_timestamp_ && data.timestamp >= start_time) {
              if (written_timestamps.find(data.timestamp) == written_timestamps.end()) {
                min_timestamp = std::min(min_timestamp, data.timestamp);
                max_timestamp = std::max(max_timestamp, data.timestamp);
                // Ensure buffer is not empty before writing
                if (!data.buffer.empty()) {
                  // Pass raw buffer data directly to Write()
                  if (Write(topic, data.timestamp, data.buffer.data(), data.buffer.size())) {
                    written_timestamps.insert(data.timestamp);
                    forward_count++;
                    total_forward_messages++;
                  } else {
                    RCLCPP_ERROR(node_->get_logger(), "Failed to write message for topic: %s", topic.c_str());
                  }
                }
              }
            }
          }
        }

        // 写入后向数据
        if (backward_it != backward_ringbuffers_.end()) {
          const auto backward_buffer_snapshot = backward_it->second->snapshot();
          for (const auto& data : backward_buffer_snapshot) {
            if (data.timestamp > trigger_timestamp_ && data.timestamp <= end_time) {
              min_timestamp = std::min(min_timestamp, data.timestamp);
              max_timestamp = std::max(max_timestamp, data.timestamp);
              // Ensure buffer is not empty before writing
              if (!data.buffer.empty()) {
                // Pass raw buffer data directly to Write()
                if (Write(topic, data.timestamp, data.buffer.data(), data.buffer.size())) {
                  backward_count++;
                  total_backward_messages++;
                } else {
                  RCLCPP_ERROR(node_->get_logger(), "Failed to write message for topic: %s", topic.c_str());
                }
              }
            }
          }
        }
      
      RCLCPP_INFO(node_->get_logger(), "Processed topic: %s, wrote forward messages: %zu, backward messages: %zu",
                     topic.c_str(), forward_count, backward_count);
    }

    RCLCPP_INFO(node_->get_logger(), "Total forward messages written: %zu, backward: %zu",
                 total_forward_messages, total_backward_messages);

    // Diagnostic: log actual timestamp range before override
    if (min_timestamp < UINT64_MAX && max_timestamp > 0) {
        double actual_span = static_cast<double>(max_timestamp - min_timestamp) / 1000000.0;
        double recorded_span = (bag_info_.end_timestamp > 0 && bag_info_.start_timestamp > 0)
            ? static_cast<double>(bag_info_.end_timestamp - bag_info_.start_timestamp) / 1000000.0
            : -1.0;
        RCLCPP_INFO(node_->get_logger(),
            "[DIAG] min_ts=%lu max_ts=%lu actual_span=%.3fs recorded_span=%.3fs trigger_ts=%lu start_bound=%lu end_bound=%lu",
            min_timestamp, max_timestamp, actual_span, recorded_span,
            trigger_timestamp_, start_time, end_time);
    }

    // 覆盖 bag_info_ 的时间范围为实际数据的 min/max timestamp
    // write_ringbuffer 按 topic 交叉写入 forward/backward，
    // update_statistics 的 end_timestamp 会被 topic 间的 forward 数据回退覆盖
    if (min_timestamp < UINT64_MAX && max_timestamp > 0) {
        bag_info_.start_timestamp = min_timestamp;
        bag_info_.end_timestamp = max_timestamp;
    }

    Close();

    RCLCPP_INFO(node_->get_logger(), "Bag write completed successfully");
    return true;
}

bool Ros2BagRecorder::SetMaxBagSize(size_t max_size_mb) {
  max_bag_size_mb_ = max_size_mb;

  RCLCPP_INFO(node_->get_logger(), "Max bag size set to:   %zu MB",
              max_size_mb);

  return true;
}

TBagInfo Ros2BagRecorder::GetStatistics() const {
  return GetBagInfo();
}

void Ros2BagRecorder::OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& msg) {
  auto& sm = state_machine::StateMachine::getInstance();
  uint64_t message_timestamp = common::GetCurrentTimestamp();

  // Continuous mode: write directly to bag file
  if (continuous_mode_) {
    if (is_opened_ && writer_) {
      auto& rcl_msg = msg.get_rcl_serialized_message();
      Write(topic, message_timestamp, rcl_msg.buffer, rcl_msg.buffer_length);
    }
    return;
  }

  // Extract raw bytes from SerializedMessage for deep-copy safe storage
  const auto& rcl_msg = msg.get_rcl_serialized_message();
  std::vector<uint8_t> buffer_data(rcl_msg.buffer, rcl_msg.buffer + rcl_msg.buffer_length);

  // Trigger-based mode: use ring buffers
  std::lock_guard<std::mutex> lock(buffer_mutex_);
  if (forward_ringbuffers_.count(topic)) {
    uint64_t forward_duration_us = cache_mode_.forwardCaptureDurationSec * 1000000ULL;
    TimestampedData oldest_forward;
    while (forward_ringbuffers_[topic]->front(oldest_forward) &&
           (message_timestamp - oldest_forward.timestamp) > forward_duration_us) {
      forward_ringbuffers_[topic]->pop_front();
    }
    forward_ringbuffers_[topic]->push_back(TimestampedData{std::move(buffer_data), message_timestamp});
  }

  if (sm.getCurrentState() == state_machine::SystemState::DATA_COLLECTING && backward_ringbuffers_.count(topic)) {
    uint64_t backward_duration_us = cache_mode_.backwardCaptureDurationSec * 1000000ULL;
    if ((message_timestamp - trigger_timestamp_) <= backward_duration_us) {
      // Create a new copy for backward buffer
      std::vector<uint8_t> backward_buffer(rcl_msg.buffer, rcl_msg.buffer + rcl_msg.buffer_length);
      backward_ringbuffers_[topic]->push_back(TimestampedData{std::move(backward_buffer), static_cast<uint64_t>(message_timestamp)});
    }
  }

}


void Ros2BagRecorder::update_statistics(const std::string& topic_name,
                                           uint64_t timestamp,
                                           size_t data_size) {
  bag_info_.total_messages++;
  bag_info_.total_data_size += data_size;
  messages_since_last_log_++;

  if (bag_info_.start_timestamp == 0) {
    bag_info_.start_timestamp = timestamp;
  }
  bag_info_.end_timestamp = timestamp;

  // Update per-topic statistics
  if (topics_metadata_.find(topic_name) != topics_metadata_.end()) {
    topics_metadata_[topic_name].message_count++;
    topics_metadata_[topic_name].last_timestamp = timestamp;
    topics_metadata_[topic_name].data_size += data_size;
    bag_info_.topics[topic_name] = topics_metadata_[topic_name];
  }

  // Log statistics periodically
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      now - last_log_time_);

  if (elapsed.count() >= 10) {  // Log every 10 seconds
    log_statistics();
    last_log_time_ = now;
    messages_since_last_log_ = 0;
  }
}

void Ros2BagRecorder::log_statistics() {
  RCLCPP_INFO(
      node_->get_logger(),
      "[Recorder Stats] Total:  %zu humanoid_interfaces, %.2f MB, Topics: %zu, Duration: %.1f s",
      bag_info_.total_messages,
      static_cast<double>(bag_info_.total_data_size) / (1024 * 1024),
      bag_info_.num_topics,
      std::chrono::duration<double>(std::chrono::system_clock::now() -
                                    bag_info_. start_time)
          .count());

  for (const auto& [topic, metadata] : bag_info_.topics) {
    RCLCPP_DEBUG(node_->get_logger(),
                 "  %s: %zu humanoid_interfaces, %.2f MB", topic.c_str(),
                 metadata.message_count,
                 static_cast<double>(metadata.data_size) / (1024 * 1024));
  }
}

void Ros2BagRecorder::SetContinuousMode(bool continuous) {
  continuous_mode_ = continuous;
  if (continuous) {
    RCLCPP_INFO(node_->get_logger(), "Continuous recording mode enabled");
  }
}

bool Ros2BagRecorder::IsContinuousMode() const {
  return continuous_mode_;
}

}