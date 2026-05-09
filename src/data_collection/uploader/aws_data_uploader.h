#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/CompletedMultipartUpload.h>
#include <aws/s3/model/CompletedPart.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include "common/data.h"
#include "../common/app_config.h"
#include "common/filestatus_manager.h"
#include "common/file_splitter.hpp"
#include "data_uploader_interface.h"

namespace aurora::collector {

class AwsDataUploader : public IDataUploader {
public:
    AwsDataUploader();
    ~AwsDataUploader() override;

    bool Init(const AppConfigData::DataUpload& config) override;
    bool Start() override;
    bool Stop() override;
    
    // 上传文件到AWS S3
    ErrorCode UploadFile(const std::string& full_path, UploadType upload_type) override;

private:
    void Run();
    void LoadFileList();
    void ProcessQueue();
    
    // 分片上传相关方法
    ErrorCode UploadParts(const std::string& file_path, const std::string& bucket_name, 
                         const std::string& object_key, const std::string& upload_id);
    ErrorCode CreateMultipartUpload(const std::string& bucket_name, const std::string& object_key, 
                                   std::string& upload_id);
    ErrorCode UploadPart(const std::string& bucket_name, const std::string& object_key,
                        const std::string& upload_id, int part_number, 
                        const std::vector<char>& data, std::string& etag);
    ErrorCode CompleteMultipartUpload(const std::string& bucket_name, const std::string& object_key,
                                     const std::string& upload_id, 
                                     const std::vector<std::pair<int, std::string>>& completed_parts);
    
    std::shared_ptr<Aws::S3::S3Client> s3_client_;
    std::unique_ptr<FileStatusManager> file_status_manager_;
    AppConfigData::DataUpload config_;
    
    std::thread worker_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_flag_;
    
    // AWS SDK相关参数
    Aws::SDKOptions sdk_options_;
    std::string region_;
    std::string bucket_name_;
};

}