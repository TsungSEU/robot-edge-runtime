#include "data_uploader.h"
#include <cstring>
#include <fstream>
#include <filesystem>
#include <map>
#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>

#include "common/file_splitter.hpp"
#include "common/utils/utils.h"
#include "../../common/utils/sRegex.h"
#include "common/log/logger.h"
#include "common/data.h"

using json = nlohmann::json;

namespace aurora::collector {

bool DataUploader::Init(const AppConfigData::DataUpload& config) {
    config_ = config;
    stop_flag_ = false;

    const auto& debug = AppConfig::getInstance().GetConfig().debug;
    if (!debug.closeDataEnc)
    {
        encryptor_ = std::make_unique<DataEncryption>();
        if (!encryptor_->Init(config.rsa_pub_key_path, config.watch_dir, config.enc_dir)) {
            AD_ERROR(DataUploader, "Encryptor init failed !");
            return false;
        }
    }

    AD_INFO(DataUploader, "encryptorInit success!");

    file_status_manager_ = std::make_unique<FileStatusManager>(config_.fileRecordPath);
    data_proto_ = std::make_shared<DataProto>();
    auto ret = data_proto_->Init(config_.gateway, config_.clientCertPath, config_.clientKeyPath, config_.caCertPath);
    return ret == CURLE_OK;
}

DataUploader::~DataUploader() {
    Stop();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool DataUploader::Start()
{
    worker_thread_ = std::thread(&DataUploader::Run, this);

    return true;
}

//record用于存储文件上传信息   
//检查文件是否有上传记录，获取文件的上传状态并根据需要返回相关的上传信息。
//如果文件没有上传记录，则发起请求来获取上传的 URL 和相关信息
ErrorCode DataUploader::GetUploadInfo(const std::string& full_path, UploadType upload_type, int chunk_count, FileUploadRecord& record) {
    record.uploaded_url_map.clear();
    //检查是否已有文件上传记录
    if (file_status_manager_->GetFileRecord(full_path)) {
        record = file_status_manager_->GetFileRecord(full_path).value();
        record.start_chunk = 0;//从文件起始位置开始上传
        UploadStatusResp resp;
        auto ret = data_proto_->GetUploadStatus(record.file_uuid, resp);
        if (ret != ErrorCode::SUCCESS) {
            AD_ERROR(DataUploader, "Failed to get upload status.");
            return ret;
        }
        // json j = common::ParseJsonFromString(resp);
        // std::cout << j << std::endl;
        // if (!j.contains("statusCode") || !j.contains("data") || !j["data"].contains("uploadStatus") || !j["data"].contains("uploadedPartList")) {
        //     AD_ERROR("Invalid URL response.");
        //     return ErrorCode::INVALID_RESPONSE;
        // }
        // if (j["statusCode"] != "0") {
        //     AD_ERROR("Status code is abnormal.");
        //     return ErrorCode::INVALID_RESPONSE;
        // }
        // if (j["data"]["uploadStatus"] == 3) {
        //     record.start_chunk = chunk_count+1;
        //     return ErrorCode::SUCCESS;
        // }
        // if (j["data"]["uploadedPartList"].is_array()) {
        //     // record.start_chunk = j["data"]["uploadedPartList"].size();
        //     for (const auto& uploadedPart: j["data"]["uploadedPartList"]) {
        //         record.uploaded_url_map.emplace(uploadedPart["partNumber"].get<int>(),uploadedPart["etag"]);
        //     }
        //     return ErrorCode::SUCCESS;
        // }
    } else {
        UploadUrlReq upload_req;
        upload_req.type = UploadType::ActivelyReport;
        upload_req.part_number = chunk_count;
        upload_req.filename = fs::path(full_path).filename().string();
        upload_req.vin = common::Vin();
        
        //模拟上传URL的获取过程，并将结果存入record.uploadedUrlMap中。 
        // for(int i=0; i<chunk_count; i++){
        //     record.upload_url_map.emplace(i+1, "http:");
        // }  
        // record.file_uuid ="sss";
        // record.upload_id = "123456789";
        // record.chunk_count = chunk_count;
        // record.start_chunk = 0;
        // file_status_manager_->AddFileRecord(full_path, record);
        // return ErrorCode::SUCCESS;

        // std::string resp;
        UploadUrlResp resp;
        auto ret = data_proto_->GetUploadUrl(upload_req, resp);
        if (ret != ErrorCode::SUCCESS) {
            AD_ERROR(DataUploader, "Failed to get upload url.");
            return ret;
        }
        // json j = common::ParseJsonFromString(resp);
        // if (!j.contains("statusCode") || !j.contains("data") || !j["data"].contains("partPresignUploadUrlMap") ||
        //     !j["data"].contains("fileUuid") || !j["data"].contains("uploadId")) {
        //     AD_ERROR("Invalid URL response.");
        //     return ErrorCode::INVALID_RESPONSE;
        // }
        // if (j["statusCode"] != "0") {
        //     AD_ERROR("Status code is abnormal.");
        //     return ErrorCode::INVALID_RESPONSE;
        // }
        // for (const auto& [slice_id, url] : j["data"]["partPresignUploadUrlMap"].get<std::map<std::string, std::string>>()) {
        //     record.upload_url_map[std::stoi(slice_id)] = url;
        // }
        // record.upload_url_map = j["data"]["partPresignUploadUrlMap"].get<std::map<std::string, std::string>>();
        // record.upload_url_map = resp.data.upload_url_map;
        record.file_uuid = resp.data.file_uuid;
        record.upload_id = resp.data.upload_id;
        record.chunk_count = chunk_count;
        record.start_chunk = 0;
        return ErrorCode::SUCCESS;
    }
    return ErrorCode::UNKNOWN_ERROR;
}

void DataUploader::GetUploadBagInfo(FileUploadProgress& upload_progress) {

    std::stringstream ss(upload_progress.fileName);
    std::string item;
    int count = 0;

    while (std::getline(ss, item, '_')) {
        ++count;
        if (count == 5) {
            std::cout << "The fifth element is: " << item << '\n';
            break; // 找到第三个元素后可以跳出循环
        }
    }
    double bag_distance;
    try {   
        bag_distance = std::stod(item);
    }
    catch (const std::invalid_argument& e){   
        std::cout << "Invalid argument: cannot convert string :" << item << "to double"<< '\n';   
        return;   
    }
    std::cout << "upload bag_distance: " << bag_distance << '\n';
    // data_reporter_->addUploadBagInfo(bag_distance, upload_progress.dataSize);
}

//将大文件切割成小分片，通过http put将分片上传到服务器，上传过程中提供重试机制，并在上传完成后通知服务器上传成功
ErrorCode DataUploader::UploadFile(const std::string& full_path, UploadType upload_type) {
    //分割切片
    FileSplitter splitter(full_path, config_.uploadFileSliceSizeMb);
    if (splitter.getErrorCode() != FileSplitter::SUCCESS) {
        AD_ERROR(DataUploader, "Split File Failed.");
        return ErrorCode::FILE_CHUNK_ERROR;
    }

    //获取文件上传信息
    FileUploadRecord record;
    // DataUploader::ErrorCode ret;
    auto ret = GetUploadInfo(full_path, upload_type, splitter.getChunkCount(), record);
    if (ret != ErrorCode::SUCCESS) {
        AD_ERROR(DataUploader, "Get Upload Info Failed.");
        return ret;
    }
    //检查文件是否已上传，如果已上传，则删除文件记录
    if (record.chunk_count+1 == record.start_chunk) {
        file_status_manager_->DeleteFileRecord(full_path);
        AD_INFO(DataUploader, "File %s was uploaded, skip.", full_path.c_str());
        return ErrorCode::SUCCESS;
    }

    int complete_chunk = 0;
    CompleteUploadReq complete_req;
    complete_req.upload_status = UploadStatus::Uploaded;
    FileUploadProgress upload_progress;
    upload_progress.fileName = fs::path(full_path).filename().string();
    upload_progress.fileUuid = record.file_uuid;
    upload_progress.dataSize = static_cast<double>(splitter.getFileSize())/1024/1024; 
    upload_progress.uploadStatus = 0;
    for (const auto& [cur_id, url] : record.upload_url_map) {
        AD_INFO(DataUploader, "slice_id: %d", cur_id);
        auto slice_id = std::to_string(cur_id + 1);
        
        // auto cur_id = std::stoi(slice_id);
        if (record.uploaded_url_map.count(cur_id) > 0) {
            AD_INFO(DataUploader, "slice_id: %d has been uploaded", cur_id);
            continue;
        }
        std::vector<char> buffer;
        // splitter.getChunkData(cur_id, buffer);
        if (splitter.getChunkData(cur_id, buffer) != FileSplitter::ErrorCode::SUCCESS) {
            AD_ERROR(DataUploader, "Get chunk data failed.");
            return ErrorCode::FILE_CHUNK_ERROR;
        }
        for (int i = 0; i < config_.retryCount; ++i) {
            if (i > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(config_.retryIntervalSec));
                AD_INFO(DataUploader, "Retry to upload chunk: %d", cur_id);
            }

            std::string resp;
            auto ret = data_proto_->UploadFileChunk(buffer, url, resp);
            if (ret == ErrorCode::SUCCESS) {
                AD_INFO(DataUploader, "Upload chunk %s succeeded.", slice_id.c_str());
                complete_chunk = cur_id;
                complete_req.etag_map[slice_id] = resp;
                break;
            }
            complete_req.upload_status = UploadStatus::Failed;

            // AD_INFO("buffer size: %d", buffer.size());
            // auto ret = curl_wrapper_.HttpPut(
            //     url, buffer, resp, {"Content-Type:"});
            // //模拟HTTP PUT请求，返回CURLE_OK表示成功。
            // // auto ret = CURLE_OK;
            // // resp = "aaa";
            //
            // AD_INFO("HttpPut resp tag: %s", resp.c_str());
            // if (resp.length() > 0) {
            //     complete_req.etag_map[std::to_string(cur_id)] = resp;
            // }
            // AD_INFO("HttpPut ret: %d", ret);
            // std::cout << "ret == CURLE_OK:" << (ret == CURLE_OK) << (resp.length() > 0) << std::endl;
            // if (ret == CURLE_OK && resp.length() > 0) {
            //     std::cout << "upload succeeded" << std::endl;
            //     AD_INFO("Chunk upload succeeded: %d", cur_id);
            //     upload_progress.uploadUrl = url;
            //     upload_progress.progress = static_cast<float>(cur_id)/static_cast<float>(record.chunk_count);
            //     std::this_thread::sleep_for(std::chrono::milliseconds(config_.uploadFileSliceIntervalMs));
            //     if(cur_id < record.chunk_count){
            //         // data_reporter_->publishUploadMessage(upload_progress);
            //     }
            //     break;
            // }
            // complete_req.upload_status = common::UploadStatus::Failed;
            // // break;
        }
        if (complete_req.upload_status == UploadStatus::Failed) {
            AD_ERROR(DataUploader, "Chunk upload failed: %d", cur_id);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.uploadFileSliceIntervalMs));
    }

    complete_req.type = UploadType::ActivelyReport;
    complete_req.file_uuid = record.file_uuid;
    complete_req.upload_id = record.upload_id;
    complete_req.task_id = "";
    complete_req.vin = common::Vin();
    // std::string complete_resp;
    CompleteUploadResp complete_resp;
    ret = data_proto_->CompleteUpload(complete_req, complete_resp);

    AD_INFO(DataUploader, "Download url: %s", complete_resp.data.presign_download_url.c_str());
    if (ret != ErrorCode::SUCCESS || complete_req.upload_status != UploadStatus::Uploaded) {
        record.start_chunk = complete_chunk + 1;
        file_status_manager_->AddFileRecord(full_path, record);
        AD_ERROR(DataUploader, "Complete upload failed.");
        return ErrorCode::UPLOAD_INCOMPLETE;
    }
    file_status_manager_->DeleteFileRecord(full_path);
    AD_INFO(DataUploader, "%s was uploaded successfully.",  full_path.c_str());

    upload_progress.uploadStatus = 3;
    // data_reporter_->publishUploadMessage(upload_progress);
    GetUploadBagInfo(upload_progress);

    return ErrorCode::SUCCESS;
}

//加载指定目录中的文件列表，并将符合条件的文件路径加入到upload_queue中
void DataUploader::LoadFileList() {
    auto& upload_queue = UploadQueue::GetInstance();
    std::lock_guard<std::mutex> lock(mutex_);
    // std::string folder_name;
    // encryptor_->GetFolderName(folder_name); 
    // AD_INFO("upload_folder_name_today: %s", folder_name.c_str());
    // TODO
    const auto& debug = AppConfig::getInstance().GetConfig().debug;
    if (!debug.closeDataEnc)
    {
        for (const auto& [_, upload_dir] : encryptor_->encrypt_paths) {
            std::string upload_dir_today = upload_dir;
            // std::string upload_dir_today = upload_dir + "/"+ folder_name;
            AD_INFO(DataUploader, "upload_dir_today: %s", upload_dir_today.c_str());

            if (!common::IsDirExist(upload_dir_today)) {
                AD_ERROR(DataUploader, "Directory %s does not exist.", upload_dir_today.c_str());
                continue;
            }

            for (const auto& entry : fs::directory_iterator(upload_dir_today)) {
                if (entry.is_regular_file() && common::IsMatch(entry.path().filename().string(), config_.filenameRegex)) {
                    upload_queue.Push({entry.path().string(), UploadType::ActivelyReport});
                    std::cout << "Path push: " << entry.path().string() << std::endl;
                }
            }
        }
    }

    std::string upload_dir_today = config_.watch_dir;
    // std::string upload_dir_today = upload_dir + "/"+ folder_name;
    AD_INFO(DataUploader, "upload_dir_today: %s", upload_dir_today.c_str());

    for (const auto& entry : fs::directory_iterator(upload_dir_today)) {
        if (entry.is_regular_file() && common::IsMatch(entry.path().filename().string(), config_.filenameRegex)) {
            upload_queue.Push({entry.path().string(), UploadType::ActivelyReport});
            std::cout << "Path push: " << entry.path().string() << std::endl;
        }
    }
    AD_INFO(DataUploader, "Loaded %d files from upload paths.", upload_queue.Size());
}

void DataUploader::Run() {
    AD_INFO(DataUploader, "Run.");
    while (!stop_flag_) {
        LoadFileList();
        ProcessQueue();
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(1));
    }
}

bool DataUploader::Stop() {
    AD_INFO(DataUploader, "Stop.");

    const auto& debug = AppConfig::getInstance().GetConfig().debug;
    if (!debug.closeDataEnc) encryptor_->Stop();

    stop_flag_ = true;
    cv_.notify_all();
    return true;
}

//从队列中抽出文件进行上传，成功则删除文件，失败则重试上传
void DataUploader::ProcessQueue() {
    auto& upload_queue = UploadQueue::GetInstance();
    const auto& debug_config = AppConfig::getInstance().GetConfig().debug;
    while (!stop_flag_) {
        if (upload_queue.Empty()) {
            AD_INFO(DataUploader, "No files in queue.");
            break;
        }

        UploadItem current_file = upload_queue.Front().value();
        AD_INFO(DataUploader, "begin upload file %s.", current_file.file_path.c_str());

        const auto& debug = AppConfig::getInstance().GetConfig().debug;
        if (!debug.closeDataEnc)
        {
            std::filesystem::path current_file_path(current_file.file_path);
            std::string encrypted_file = encryptor_->enc_dir_ + "/" + current_file_path.filename().string() + ".enc";

            AD_INFO(DataUploader, "Encrypting file: %s", encrypted_file.c_str());
            if (!std::filesystem::exists(encrypted_file)) {
                std::string decrypted_file = encryptor_->enc_dir_ + "/" + current_file_path.filename().string() + ".dec";
                if (!debug_config.closeDataEnc)
                {
                    auto success_enc = encryptor_->EncryptChunkFileWithEnvelope(current_file.file_path, encrypted_file);
                    AD_INFO(DataUploader, "encryptor success: %d", success_enc);
                    if (success_enc == 0) {
                        AD_INFO(DataUploader, "encryptor file: %s sucessfully.", current_file.file_path.c_str());
                        // common::DeleteFile(current_file);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    } else {
                        AD_ERROR(DataUploader, "Failed to encrypt file: %s", current_file.file_path.c_str());
                        continue;
                    }
                }
            } else {
                AD_INFO(DataUploader, "file: %s already encrypted.", current_file.file_path.c_str());
            }

            auto success_upload = UploadFile(encrypted_file, current_file.upload_type);
        }
       
        auto success_upload = UploadFile(config_.watch_dir, current_file.upload_type);
        
        AD_INFO(DataUploader, "upload success: %d", success_upload);

        if (success_upload == SUCCESS) {
            AD_INFO(DataUploader, "Uploaded file: %s", current_file.file_path.c_str());
            common::DeleteFile(current_file.file_path);
            common::DeleteFile(config_.watch_dir); //TODO
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.uploadFileIntervalMs));
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            AD_ERROR(DataUploader, "Failed to upload file: %s", current_file.file_path.c_str());
            // upload_queue_.push(current_file);
            std::this_thread::sleep_for(std::chrono::seconds(config_.retryIntervalSec));
        }
    }
}

}
