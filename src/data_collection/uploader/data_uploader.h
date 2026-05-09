#ifndef DATA_UPLOADER_H
#define DATA_UPLOADER_H

#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <iostream>
#include <filesystem>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <atomic>

#include "protocol/data_protocol.h"
#include "common/filestatus_manager.h"
#include "common/data.h"
#include "../common/app_config.h"
#include "data_encryption.h"
#include "common/upload_queue.hpp"


namespace aurora::collector {

namespace fs = std::filesystem;

class DataUploader {
public:
  DataUploader() = default;
  ~DataUploader();

  bool Init(const AppConfigData::DataUpload& config);
  bool Start();
  bool Stop();
  ErrorCode UploadFile(const std::string& full_path, UploadType upload_type);

private:
  ErrorCode GetUploadInfo(const std::string& full_path, UploadType upload_type, int chunk_count, FileUploadRecord& record);
  void Run();
  void LoadFileList();
  void ProcessQueue();
  void GetUploadBagInfo(FileUploadProgress& upload_progress);

  std::unique_ptr<FileStatusManager> file_status_manager_;
  AppConfigData::DataUpload config_;
  // std::queue<std::string> upload_queue_;
  std::thread worker_thread_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> stop_flag_;
  std::unique_ptr<DataEncryption> encryptor_;
  std::shared_ptr<DataProto> data_proto_;

};

}


#endif //DATA_UPLOADER_H
