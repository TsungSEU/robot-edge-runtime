#include "aws_data_uploader.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>

#include "common/utils/utils.h"
#include "common/utils/sRegex.h"
#include "common/log/logger.h"
#include "common/upload_queue.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace aurora::collector {

AwsDataUploader::AwsDataUploader() {
    // 初始化AWS SDK
    Aws::InitAPI(sdk_options_);
}

AwsDataUploader::~AwsDataUploader() {
    Stop();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    // 清理AWS SDK
    Aws::ShutdownAPI(sdk_options_);
}

bool AwsDataUploader::Init(const AppConfigData::DataUpload& config) {
    config_ = config;
    stop_flag_ = false;

    // 初始化文件状态管理器
    file_status_manager_ = std::make_unique<FileStatusManager>(config_.fileRecordPath);
    
    // 从配置中获取AWS相关参数
    region_ = config_.aws.region;
    bucket_name_ = config_.aws.bucketName;

    // 创建S3客户端
    Aws::Client::ClientConfiguration clientConfig;
    clientConfig.region = region_;
    clientConfig.endpointOverride = config_.aws.endpointUrl;

    if (!config_.aws.accessKeyId.empty() && !config_.aws.secretAccessKey.empty()) {
        // 使用访问密钥和秘密密钥
        auto credentials = Aws::Auth::AWSCredentials(config_.aws.accessKeyId, config_.aws.secretAccessKey);
        s3_client_ = std::make_shared<Aws::S3::S3Client>(credentials, clientConfig, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
        // AD_INFO(AwsDataUploader, "Using access key and secret key for authentication");
    }

    // AD_INFO(AwsDataUploader, "AwsDataUploader initialized with region: %s, bucket: %s, slice size: %zu MB", 
    //        region_.c_str(), bucket_name_.c_str(), config_.uploadFileSliceSizeMb);
    return true;
}

bool AwsDataUploader::Start() {
    worker_thread_ = std::thread(&AwsDataUploader::Run, this);
    return true;
}

bool AwsDataUploader::Stop() {
    AD_INFO(AwsDataUploader, "Stopping AwsDataUploader...");
    stop_flag_ = true;
    cv_.notify_all();
    return true;
}

ErrorCode AwsDataUploader::UploadFile(const std::string& full_path, UploadType upload_type) {
    try {
        // 获取文件名作为对象键
        std::string object_key = fs::path(full_path).filename().string();

        // 获取文件大小以确定是否需要分片上传
        FileSplitter splitter(full_path, config_.uploadFileSliceSizeMb);
        if (splitter.getErrorCode() != FileSplitter::SUCCESS) {
            AD_ERROR(AwsDataUploader, "Split File Failed.");
            return ErrorCode::FILE_CHUNK_ERROR;
        }

        size_t file_size = splitter.getFileSize();
        AD_INFO(AwsDataUploader, "File size: %zu bytes, Path: %s", file_size, full_path.c_str());

        // 如果文件大于阈值，则使用分片上传
        const size_t MULTIPART_THRESHOLD = config_.uploadFileSliceSizeMb * 1024 * 1024;
        if (file_size > MULTIPART_THRESHOLD) {
            AD_INFO(AwsDataUploader, "Using multipart upload for large file: %s (size: %zu MB)", 
                   full_path.c_str(), file_size / (1024 * 1024));
            
            std::string upload_id;
            ErrorCode result = CreateMultipartUpload(bucket_name_, object_key, upload_id);
            if (result != SUCCESS) {
                AD_ERROR(AwsDataUploader, "Failed to create multipart upload for file: %s", full_path.c_str());
                return result;
            }
            
            result = UploadParts(full_path, bucket_name_, object_key, upload_id);
            return result;
        } else {
            // 对于小文件，直接上传
            AD_INFO(AwsDataUploader, "Uploading small file: %s", full_path.c_str());

            // 读取文件内容
            std::ifstream file_stream(full_path, std::ios::binary);
            if (!file_stream) {
                AD_ERROR(AwsDataUploader, "Failed to open file: %s", full_path.c_str());
                return FILE_NOT_FOUND;
            }

            // 创建PutObjectRequest
            Aws::S3::Model::PutObjectRequest object_request;
            object_request.SetBucket(bucket_name_);
            object_request.SetKey(object_key);
            object_request.SetBody(std::make_shared<Aws::FStream>(full_path.c_str(),
                                                                std::ios_base::in | std::ios_base::binary));

            // 发送请求
            auto put_object_outcome = s3_client_->PutObject(object_request);
            if (put_object_outcome.IsSuccess()) {
                AD_INFO(AwsDataUploader, "Successfully uploaded object: %s to bucket: %s",
                       object_key.c_str(), bucket_name_.c_str());
                return SUCCESS;
            } else {
                AD_ERROR(AwsDataUploader, "Failed to upload object: %s, error: %s",
                        object_key.c_str(),
                        put_object_outcome.GetError().GetMessage().c_str());
                return UNKNOWN_ERROR;
            }
        }
    } catch (const std::exception& e) {
        AD_ERROR(AwsDataUploader, "Exception in UploadFile: %s", e.what());
        return UNKNOWN_ERROR;
    }
    return SUCCESS;
}

void AwsDataUploader::LoadFileList() {
    auto& upload_queue = UploadQueue::GetInstance();
    std::lock_guard<std::mutex> lock(mutex_);

    // 从配置的上传目录加载待上传文件
    std::string upload_dir = config_.watch_dir; // 使用配置中的监控目录
    AD_INFO(AwsDataUploader, "Checking upload directory: %s", upload_dir.c_str());

    if (!common::IsDirExist(upload_dir)) {
        AD_ERROR(AwsDataUploader, "Directory %s does not exist.", upload_dir.c_str());
        return;
    }

    for (const auto& entry : fs::directory_iterator(upload_dir)) {
        if (entry.is_regular_file() && common::IsMatch(entry.path().filename().string(), config_.filenameRegex)) {
            upload_queue.Push({entry.path().string(), UploadType::ActivelyReport});
            AD_INFO(AwsDataUploader, "Added file to upload queue: %s", entry.path().string().c_str());
        }
    }
    AD_INFO(AwsDataUploader, "Loaded %d files from upload paths.", upload_queue.Size());
}

void AwsDataUploader::Run() {
    AD_INFO(AwsDataUploader, "AwsDataUploader running...");
    while (!stop_flag_) {
        LoadFileList();
        ProcessQueue();
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(1));
    }
    AD_INFO(AwsDataUploader, "AwsDataUploader stopped.");
}

void AwsDataUploader::ProcessQueue() {
    auto& upload_queue = UploadQueue::GetInstance();
    const auto& debug_config = AppConfig::getInstance().GetConfig().debug;

    int consecutive_failures = 0;
    constexpr int MAX_CONSECUTIVE_FAILURES = 5;
    double backoff_sec = config_.retryIntervalSec;

    while (!stop_flag_) {
        if (upload_queue.Empty()) {
            AD_INFO(AwsDataUploader, "No files in upload queue.");
            break;
        }

        UploadItem current_file = upload_queue.Front().value();
        AD_INFO(AwsDataUploader, "Starting upload for file: %s", current_file.file_path.c_str());

        // 执行上传
        auto success_upload = UploadFile(current_file.file_path, current_file.upload_type);
        AD_INFO(AwsDataUploader, "Upload result: %d", static_cast<int>(success_upload));

        if (success_upload == ErrorCode::SUCCESS) {
            consecutive_failures = 0;
            backoff_sec = config_.retryIntervalSec;
            AD_INFO(AwsDataUploader, "Successfully uploaded file: %s", current_file.file_path.c_str());

            // 删除源文件
            common::DeleteFile(current_file.file_path);
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.uploadFileIntervalMs));

            // 从队列中移除已上传的文件
            upload_queue.Pop();
        } else {
            consecutive_failures++;
            AD_ERROR(AwsDataUploader, "Failed to upload file: %s (attempt %d/%d)",
                     current_file.file_path.c_str(), consecutive_failures, MAX_CONSECUTIVE_FAILURES);

            if (consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
                AD_ERROR(AwsDataUploader, "Max consecutive failures reached, skipping file: %s",
                         current_file.file_path.c_str());
                upload_queue.Pop();
                consecutive_failures = 0;
                backoff_sec = config_.retryIntervalSec;
            } else {
                // 指数退避: 10s → 20s → 40s → 80s
                std::lock_guard<std::mutex> lock(mutex_);
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<int64_t>(backoff_sec * 1000)));
                backoff_sec = std::min(backoff_sec * 2.0, 120.0);
            }
        }
    }
}

ErrorCode AwsDataUploader::CreateMultipartUpload(const std::string& bucket_name, const std::string& object_key, std::string& upload_id) {
    try {
        Aws::S3::Model::CreateMultipartUploadRequest request;
        request.SetBucket(bucket_name);
        request.SetKey(object_key);
        
        auto outcome = s3_client_->CreateMultipartUpload(request);
        if (outcome.IsSuccess()) {
            upload_id = outcome.GetResult().GetUploadId();
            AD_INFO(AwsDataUploader, "Created multipart upload with ID: %s for object: %s", 
                   upload_id.c_str(), object_key.c_str());
            return SUCCESS;
        } else {
            AD_ERROR(AwsDataUploader, "Failed to create multipart upload: %s", 
                    outcome.GetError().GetMessage().c_str());
            return UNKNOWN_ERROR;
        }
    } catch (const std::exception& e) {
        AD_ERROR(AwsDataUploader, "Exception in CreateMultipartUpload: %s", e.what());
        return UNKNOWN_ERROR;
    }
}

ErrorCode AwsDataUploader::UploadPart(const std::string& bucket_name, const std::string& object_key,
                                     const std::string& upload_id, int part_number, 
                                     const std::vector<char>& data, std::string& etag) {
    try {
        Aws::S3::Model::UploadPartRequest request;
        request.SetBucket(bucket_name);
        request.SetKey(object_key);
        request.SetUploadId(upload_id);
        request.SetPartNumber(part_number);
        
        // 创建内存流
        auto stream = Aws::MakeShared<Aws::StringStream>("UploadPartStream");
        stream->write(data.data(), data.size());
        stream->seekg(0);
        request.SetBody(stream);
        
        auto outcome = s3_client_->UploadPart(request);
        if (outcome.IsSuccess()) {
            etag = outcome.GetResult().GetETag();
            AD_INFO(AwsDataUploader, "Uploaded part %d successfully, ETag: %s", part_number, etag.c_str());
            return SUCCESS;
        } else {
            AD_ERROR(AwsDataUploader, "Failed to upload part %d: %s", part_number, 
                    outcome.GetError().GetMessage().c_str());
            return UNKNOWN_ERROR;
        }
    } catch (const std::exception& e) {
        AD_ERROR(AwsDataUploader, "Exception in UploadPart: %s", e.what());
        return UNKNOWN_ERROR;
    }
}

ErrorCode AwsDataUploader::CompleteMultipartUpload(const std::string& bucket_name, const std::string& object_key,
                                                  const std::string& upload_id, 
                                                  const std::vector<std::pair<int, std::string>>& completed_parts) {
    try {
        Aws::S3::Model::CompleteMultipartUploadRequest request;
        request.SetBucket(bucket_name);
        request.SetKey(object_key);
        request.SetUploadId(upload_id);
        
        Aws::S3::Model::CompletedMultipartUpload completed_multipart_upload;
        for (const auto& part : completed_parts) {
            Aws::S3::Model::CompletedPart completed_part;
            completed_part.SetPartNumber(part.first);
            completed_part.SetETag(part.second);
            completed_multipart_upload.AddParts(completed_part);
        }
        
        request.SetMultipartUpload(completed_multipart_upload);
        
        auto outcome = s3_client_->CompleteMultipartUpload(request);
        if (outcome.IsSuccess()) {
            AD_INFO(AwsDataUploader, "Completed multipart upload successfully for object: %s", object_key.c_str());
            return SUCCESS;
        } else {
            AD_ERROR(AwsDataUploader, "Failed to complete multipart upload: %s", 
                    outcome.GetError().GetMessage().c_str());
            return UNKNOWN_ERROR;
        }
    } catch (const std::exception& e) {
        AD_ERROR(AwsDataUploader, "Exception in CompleteMultipartUpload: %s", e.what());
        return UNKNOWN_ERROR;
    }
}

ErrorCode AwsDataUploader::UploadParts(const std::string& file_path, const std::string& bucket_name, 
                                      const std::string& object_key, const std::string& upload_id) {
    try {
        FileSplitter splitter(file_path, config_.uploadFileSliceSizeMb);
        if (splitter.getErrorCode() != FileSplitter::SUCCESS) {
            AD_ERROR(AwsDataUploader, "Failed to initialize file splitter for: %s", file_path.c_str());
            return FILE_CHUNK_ERROR;
        }
        
        int chunk_count = splitter.getChunkCount();
        std::vector<std::pair<int, std::string>> completed_parts;
        completed_parts.reserve(chunk_count);
        
        AD_INFO(AwsDataUploader, "Starting multipart upload with %d parts for file: %s", 
               chunk_count, file_path.c_str());
        
        // 逐个上传分片
        for (int i = 0; i < chunk_count; ++i) {
            std::vector<char> chunk_data;
            auto result = splitter.getChunkData(i + 1, chunk_data); // FileSplitter内部已经做了-1操作
            if (result != FileSplitter::SUCCESS) {
                AD_ERROR(AwsDataUploader, "Failed to get chunk data for part %d", i + 1);
                return FILE_CHUNK_ERROR;
            }
            
            std::string etag;
            ErrorCode upload_result = UploadPart(bucket_name, object_key, upload_id, i + 1, chunk_data, etag);
            if (upload_result != SUCCESS) {
                AD_ERROR(AwsDataUploader, "Failed to upload part %d", i + 1);
                return upload_result;
            }
            
            completed_parts.emplace_back(i + 1, etag);
            AD_INFO(AwsDataUploader, "Uploaded part %d/%d successfully", i + 1, chunk_count);
        }
        
        // 完成分片上传
        ErrorCode complete_result = CompleteMultipartUpload(bucket_name, object_key, upload_id, completed_parts);
        if (complete_result == SUCCESS) {
            AD_INFO(AwsDataUploader, "Successfully completed multipart upload for file: %s", file_path.c_str());
        }
        return complete_result;
        
    } catch (const std::exception& e) {
        AD_ERROR(AwsDataUploader, "Exception in UploadParts: %s", e.what());
        return UNKNOWN_ERROR;
    }
}

}