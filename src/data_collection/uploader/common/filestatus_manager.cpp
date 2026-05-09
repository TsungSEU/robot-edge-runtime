#include "filestatus_manager.h"
#include <fstream>
#include "common/log/logger.h"
#include "common/utils/utils.h"

namespace aurora::collector {

FileStatusManager::FileStatusManager(const std::string& json_path)
    : main_path_(json_path), backup_path_(json_path + ".bak"), tmp_path_(json_path + ".tmp") {
    std::lock_guard<std::mutex> lock(mutex_);
    LoadWithRecovery();
}

bool FileStatusManager::AddFileRecord(const std::string& file_path, const FileUploadRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[file_path] = ConvertFileRecordToJson(record);
    return SaveToFile();
}

bool FileStatusManager::DeleteFileRecord(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!data_.contains(file_path)) {
        AD_INFO(FileStatusManager, "File %s has no record.", file_path.c_str());
        return false;
    }
    data_.erase(file_path);
    return SaveToFile();
}

bool FileStatusManager::UpdateFileStartChunk(const std::string& file_path, int start_chunk) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!data_.contains(file_path)) {
        AD_INFO(FileStatusManager, "File %s has no record.", file_path.c_str());
        return false;
    }
    data_[file_path]["start_chunk"] = start_chunk;
    return SaveToFile();
}

std::optional<FileUploadRecord> FileStatusManager::GetFileRecord(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!data_.contains(file_path)) {
        AD_INFO(FileStatusManager, "File %s has no record.", file_path.c_str());
        return std::nullopt;
    }
    FileUploadRecord record;
    try {
        record.chunk_count = data_[file_path]["chunk_count"];
        record.start_chunk = data_[file_path]["start_chunk"];
        record.file_uuid = data_[file_path]["file_uuid"];
        record.upload_id = data_[file_path]["upload_id"];
        for (const auto& [slice_id, url] : data_[file_path]["upload_url_map"].get<std::map<std::string, std::string>>()) {
            record.upload_url_map[std::stoi(slice_id)] = url;
        }
        // record.upload_url_map = data_[file_path]["upload_url_map"].get<std::map<int, std::string>>();
    } catch (const std::exception& e) {
        AD_ERROR(FileStatusManager, "Parse json error: %d", e.what());
        return std::nullopt;
    }
    return std::optional(record);
}

//将当前数据写入临时文件，主文件备份，并将临时文件变成主文件，完成数据的保存
bool FileStatusManager::SaveToFile() {
    try {
        WriteToFile(tmp_path_, data_);
        if (fs::exists(main_path_)) {
            fs::rename(main_path_, backup_path_);
            AD_INFO(FileStatusManager, "Backup successfully.");
        }
        fs::rename(tmp_path_, main_path_);
        AD_INFO(FileStatusManager, "Save successfully.");
        return true;
    } catch (const fs::filesystem_error& e) {
        AD_ERROR(FileStatusManager, "Save to file error: %s", e.what());
        return false;
    }
    return false;
}

//将一个文件上传记录对象转化为适合存储或传输的 JSON 格式
json FileStatusManager::ConvertFileRecordToJson(const FileUploadRecord& record) {
    json j = json::object();
    j["chunk_count"] = record.chunk_count;
    j["start_chunk"] = record.start_chunk;
    j["file_uuid"] = record.file_uuid;
    j["upload_id"] = record.upload_id;
    j["upload_url_map"] = json::object();
    for (const auto& [slice_id, url] : record.upload_url_map) {
        j["upload_url_map"][std::to_string(slice_id)] = url;
    }

    return j;
}

//从主文件中加载数据，加载失败会从备份文件中恢复数据
void FileStatusManager::LoadWithRecovery() {
    try {
        if (fs::exists(main_path_)) {
            data_ = LoadFromFile(main_path_);
            AD_INFO(FileStatusManager, "Load from file %s successful", main_path_.c_str());
            return;
        }
    } catch (const FileStatusException& e) {
        if (TryRecoverFromBackup()) {
            AD_INFO(FileStatusManager, "Load from backup file %s successful", backup_path_.c_str());
            return;
        }
    }
    CreateNewFile();
    AD_INFO(FileStatusManager, "Recovery failed, create new file.");
}

bool FileStatusManager::TryRecoverFromBackup() {
    try {
        if (fs::exists(backup_path_)) {
            data_ = LoadFromFile(backup_path_);
            SaveToFile();
            AD_INFO(FileStatusManager, "Recover from backup file %s successful.", backup_path_.c_str());
            return true;
        }
    } catch (const FileStatusException& e) {
        AD_WARN(FileStatusManager, "Load backup file failed: %s", e.what());
        fs::remove(backup_path_);
    }
    return false;
}

//读取路径下的文件并解析成json数据
json FileStatusManager::LoadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file) {
            AD_ERROR(FileStatusManager, "Open file %s failed.", path.c_str());
            throw FileStatusException(FileStatusException::ErrorType::CORRUPTED_FILE, path, "Cannot open file");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return json::parse(buffer.str());
    } catch (const json::exception& e) {
        AD_ERROR(FileStatusManager, "Parse JSON error: %s", e.what());
        throw FileStatusException(FileStatusException::ErrorType::CORRUPTED_FILE, path, "Invalid JSON format: " + std::string(e.what()));
    }
    return json::object();
}

void FileStatusManager::CreateNewFile() {
    data_ = json::object();
    SaveToFile();
    if (!fs::exists(main_path_)) {
        AD_ERROR(FileStatusManager, "Cannot create new file.");
    }
}

//将json对象写入指定路径的文件中
void FileStatusManager::WriteToFile(const std::string& path, const json& j) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        AD_ERROR(FileStatusManager, "Open file %s failed.", path.c_str());
        throw FileStatusException(FileStatusException::ErrorType::WRITE_FAILURE, path, "Cannot open file for writing");
    }
    //将对象j转换为格式化的json字符串，缩进4个空格
    file << j.dump(4) << std::endl;
    file.flush();
    if (!file.good()) {
        AD_ERROR(FileStatusManager, "Write to file %s failed.", path.c_str());
        throw FileStatusException(FileStatusException::ErrorType::WRITE_FAILURE, path, "Write operation failed");
    }
    file.close();
}

}
