//
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <rosbag2_cpp/writer.hpp>

#include "state_machine/state_machine.h"
#include "channel/observer.h"
#include "common/ringBuffer.h"
#include "trigger/strategy/strategy_config.h"

namespace aurora::collector {

/**
 * Generic Bag Recorder
 * A high-performance, flexible ROS2 bag recorder supporting arbitrary message types
 * 
 * Inspired by professional bag recording interfaces with support for:
 * - Multiple recording modes (WRITE, READ)
 * - Runtime topic subscription without recompilation
 * - Circular buffer recording with trigger-based output
 * - Comprehensive metadata tracking
 */

  
/**
 * @enum OptMode
 * @brief Operation mode for bag recorder
 */
enum class OptMode {
  WRITE = 0,   ///< Writing/Recording mode
  READ = 1     ///< Reading/Playback mode
};

/**
 * @struct TopicMetadata
 * @brief Metadata for each topic being recorded
 */
struct TopicMetadata {
  std::string topic_name;         ///< Topic name (e.g., "/robot/state")
  std::string message_type;       ///< Message type (e.g., "/msg/SportModeState")
  uint64_t message_count = 0;     ///< Total messages received for this topic
  uint64_t last_timestamp = 0;    ///< Timestamp of last received message
  size_t data_size = 0;           ///< Total bytes written for this topic
};

/**
 * @struct TBagInfo
 * @brief Information and statistics about the recorded bag
 */
struct TBagInfo {
  std::string bag_path;              ///< Path to the bag file
  std::string storage_id = "sqlite3"; ///< Storage backend type
  std::string serialization_format;   ///< Serialization format (e.g., "cdr")
  
  uint64_t total_messages = 0;       ///< Total messages recorded
  size_t total_data_size = 0;        ///< Total bytes written
  
  std::chrono::system_clock::time_point start_time;  ///< Recording start time
  std::chrono::system_clock::time_point end_time;    ///< Recording end time
  
  uint64_t start_timestamp = 0;      ///< Earliest message timestamp
  uint64_t end_timestamp = 0;        ///< Latest message timestamp
  
  size_t num_topics = 0;             ///< Number of unique topics
  std::map<std::string, TopicMetadata> topics; ///< Per-topic metadata
  
  bool is_opened = false;            ///< Whether bag is currently open
  OptMode mode = OptMode::WRITE;     ///< Current operation mode
};

/**
 * @struct ReadedMessage
 * @brief A message read from the bag file
 */
struct ReadedMessage {
  std::string topic_name;            ///< Topic name
  std::string message_type;          ///< Message type
  uint64_t timestamp = 0;            ///< Message timestamp
  std::vector<uint8_t> data;         ///< Serialized message data
};

/**
 * @struct TimestampedData
 * @brief A serialized message with its timestamp, used by ring buffers
 */
struct TimestampedData {
  std::vector<uint8_t> buffer;  ///< Raw serialized message data (deep-copy safe)
  uint64_t timestamp;           ///< Timestamp in microseconds
};

/**
 * @class Ros2BagRecorder
 * @brief Professional bag recorder supporting arbitrary message types
 * 
 * This class provides a clean, intuitive API for recording ROS 2 topics to bag files.
 * It supports:
 * - Flexible mode selection (recording/playback)
 * - Dynamic topic subscription at runtime
 * - High-performance serialized message recording
 * - Comprehensive statistics and metadata tracking
 * - Circular buffer recording with trigger-based output (for memory-constrained devices)
 * 
 * Implements the Observer pattern to receive messages from ChannelManager
 */
class Ros2BagRecorder : public Observer {  // 继承自Observer
 public:
  /**
   * @brief Constructor
   * @param node Shared pointer to ROS 2 node (for subscriptions)
   */
  explicit Ros2BagRecorder(std::shared_ptr<rclcpp::Node> node);

  /**
   * @brief Destructor - automatically closes bag if still open
   */
  ~Ros2BagRecorder();

  /**
   * @brief Initialize the recorder
   * Must be called before Open()
   * @return true if initialization successful, false otherwise
   */
  bool Init();

  /**
   * @brief Initialize ring buffers for circular buffer recording
   * @return true if initialization successful, false otherwise
   */
  bool InitRingBuffers();

  /**
   * @brief Set strategy configuration (for channel info)
   * @param strategy Strategy configuration from JSON
   */
  void setStrategy(const std::shared_ptr<Strategy>& strategy) {
    strategy_ = strategy;
  }

  /**
   * @brief Set cache mode configuration
   * @param cache_mode Cache mode from JSON config
   */
  void setCacheMode(const CacheMode& cache_mode) {
    cache_mode_ = cache_mode;
  }

  /**
   * @brief Open a bag file in specified mode
   * @param opt_mode Operation mode (WRITE for recording, READ for playback)
   * @param full_path Full path to bag file
   * @return true if opened successfully, false otherwise
   */
  bool Open(OptMode opt_mode, const std::string& full_path);

  /**
   * @brief Check if a bag file is currently open
   * @return true if bag is open, false otherwise
   */
  bool IsOpened() const;

  /**
   * @brief Write serialized message data to the bag
   * This is the core method for recording messages.  Automatically: 
   * - Manages timestamp ordering
   * - Updates statistics
   * - Handles serialization format
   * 
   * @param topic_name Name of the topic
   * @param timestamp ROS timestamp (nanoseconds since epoch)
   * @param buf Pointer to serialized message data
   * @param buf_len Length of message data in bytes
   * @return true if write successful, false on error
   */
  bool Write(const std::string& topic_name, uint64_t timestamp,
             const void* buf, size_t buf_len);

  /**
   * @brief Get comprehensive information about the recorded bag
   * @return TBagInfo structure with complete metadata
   */
  TBagInfo GetBagInfo() const;

  /**
   * @brief Read next message from bag file (for playback)
   * @param message [out] The read message data
   * @return true if successfully read, false if no more messages
   */
  bool ReadNextFrame(ReadedMessage* message);

  /**
   * @brief Close the bag file
   * Finalizes recording and releases resources
   * @return true if closed successfully, false otherwise
   */
  bool Close();

  /**
   * @brief Check if data has been written to the bag
   * @return true if at least one message was written, false otherwise
   */
  bool HasDataWritten() const;

  /**
   * @brief Snapshot forward ring buffers immediately at trigger time
   * Called on executor thread to capture data before ring buffer rotates.
   * @param trigger_timestamp Timestamp when trigger occurred
   * @param out_buffers [out] Map of topic -> extracted forward data
   */
  void snapshotForwardBuffers(
      uint64_t trigger_timestamp,
      std::unordered_map<std::string, std::vector<TimestampedData>>& out_buffers);

  /**
   * @brief Trigger recording with pre-saved forward buffers
   * Used by async background worker — forward data already snapshotted.
   * @param trigger_timestamp Timestamp when trigger occurred
   * @param output_file_path Path where to save the triggered data
   * @param saved_forward_buffers Pre-extracted forward data (moved in)
   * @return true if trigger successful, false otherwise
   */
  bool TriggerRecord(uint64_t trigger_timestamp,
                     const std::string& output_file_path,
                     std::unordered_map<std::string, std::vector<TimestampedData>> saved_forward_buffers);

  /**
   * @brief Trigger recording (synchronous path — extracts forward buffers internally)
   */
  bool TriggerRecord(uint64_t trigger_timestamp,
                     const std::string& output_file_path);

  void BreakOffRecord();

  /**
   * @brief Set maximum bag file size (for auto-rotation)
   * @param max_size_mb Maximum file size in megabytes (0 = unlimited)
   * @return true if set successfully, false otherwise
   */
  bool SetMaxBagSize(size_t max_size_mb);

  /**
   * @brief Enable continuous recording mode (bypasses ring buffers)
   * @param continuous true for continuous mode, false for trigger-based mode
   */
  void SetContinuousMode(bool continuous);

  /**
   * @brief Check if continuous mode is enabled
   * @return true if continuous mode is enabled
   */
  bool IsContinuousMode() const;

  /**
   * @brief Get current recording statistics
   * @return TBagInfo with current statistics
   */
  TBagInfo GetStatistics() const;

  /**
   * @brief Implementation of Observer interface
   * Called when messages are received from ChannelManager
   * @param topic Topic name
   * @param subject Serialized message
   */
  void OnMessageReceived(const std::string& topic, const rclcpp::SerializedMessage& msg) override;

 private:
  // Internal helper methods
  bool write_ringbuffer(const std::string& outputfilePath);
  
  void update_statistics(const std::string& topic_name, uint64_t timestamp,
                        size_t data_size);
  
  void log_statistics();

  // Member variables
  std::shared_ptr<rclcpp::Node> node_;
  std::unique_ptr<rosbag2_cpp::Writer> writer_;
  
  OptMode current_mode_;
  bool is_initialized_;
  bool is_opened_;
  bool has_data_written_;
  
  std::string current_bag_path_;
  std::map<std::string, TopicMetadata> topics_metadata_;
  
  TBagInfo bag_info_;
  
  // File size limit
  size_t max_bag_size_mb_;
  
  // Statistics
  std::chrono::steady_clock::time_point last_log_time_;
  size_t messages_since_last_log_;

  // Circular buffer
  std::shared_ptr<Strategy> strategy_{nullptr};
  CacheMode cache_mode_;
  using BufferType = RingBuffer<TimestampedData>;
  std::unordered_map<std::string, std::unique_ptr<BufferType>> forward_ringbuffers_;
  std::unordered_map<std::string, std::unique_ptr<BufferType>> backward_ringbuffers_;
  std::unordered_map<std::string, std::vector<TimestampedData>> triggered_forward_buffers_;
  uint64_t forward_capture_duration_us_{0};

  // std::atomic<BusinessState> business_state_{BusinessState::IDLE};
  // std::atomic<bool> is_triggered_{false};
  std::atomic<bool> is_cancelled_{false};  // 添加取消标志
  std::mutex cancel_mutex_;
  std::condition_variable cv_cancel_;
  uint64_t trigger_timestamp_{0};
  std::mutex buffer_mutex_;

  // Continuous recording mode (bypasses ring buffers)
  bool continuous_mode_{false};
};

}