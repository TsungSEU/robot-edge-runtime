#pragma once
#include <string>
#include <filesystem>
#include <optional>
#include <mutex>
#include <nlohmann/json.hpp>

#include "common/data.h"

namespace aurora::collector {

using json = nlohmann::json;
namespace fs = std::filesystem;

class FileStatusException : public std::runtime_error {
public:
    enum class ErrorType {
        CORRUPTED_FILE,//文件损坏
        WRITE_FAILURE,//写入失败
        BACKUP_FAILURE//备份失败
    };

    ErrorType error_type;
    std::string file_path;

    FileStatusException(ErrorType type, const std::string& path, const std::string& msg)
        : std::runtime_error(msg), error_type(type), file_path(path) {}
};

class FileStatusManager {
public:
    explicit FileStatusManager(const std::string& json_path);
    bool AddFileRecord(const std::string& file_path, const FileUploadRecord& record);
    bool DeleteFileRecord(const std::string& file_path);
    bool UpdateFileStartChunk(const std::string& file_path, int start_chunk);
    std::optional<FileUploadRecord> GetFileRecord(const std::string& file_path);

private:
    bool SaveToFile();
    json ConvertFileRecordToJson(const FileUploadRecord& record);
    void LoadWithRecovery();
    void CreateNewFile();
    void WriteToFile(const std::string& path, const json& j);
    json LoadFromFile(const std::string& path);
    bool TryRecoverFromBackup();

    std::string main_path_;
    std::string backup_path_;
    std::string tmp_path_;
    json data_;
    std::mutex mutex_;
};


}
