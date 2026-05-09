//
// Created by xucong on 25-5-7.
// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#include "utils.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <fstream>
#include "common/log/logger.h"
// #include <strategy/ConfigFileWatcher.hpp>

namespace aurora::common{

//=========================================================
// 文件系统操作
//=========================================================
bool IsDirExist(const std::string &path) {
    DIR *dir = opendir(path.c_str());
    if (dir)
    {
        closedir(dir);
        return true;
    }
    else
    {
        return false;
    }
    // return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

bool DeleteFile(const std::string &path)
{
    return ::remove(path.c_str()) == 0;
}

bool DeleteFiles(const std::vector<std::string>& inputFilePaths) {
    for(const auto& filePath : inputFilePaths){
        if (fs::exists(filePath)) {
            try {
                fs::remove(filePath);
                std::cout << "File deleted successfully: " << filePath << std::endl;
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error deleting file: " << e.what() << std::endl;
                return false;
            }
        } else {
            std::cerr << "File does not exist: " << filePath << std::endl;
        }
    }
    return true;
}

void saveDataToFile(const std::string &data, const std::string &filePath) {
    size_t pos = filePath.find_last_of('/');
    if (pos != std::string::npos)
    {
        std::string dirPath = filePath.substr(0, pos);
        ensureDirectoryExists(dirPath);
    }

    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
    if (ofs.is_open())
    {
        ofs << data << "\n";
        ofs.close();
        std::cout << "Saved to: " << filePath << std::endl;
    }
    else
    {
        std::cerr << "Failed to save to: " << filePath << std::endl;
    }
}

bool ensureDirectoryExists(const std::string &path) {
#ifdef _WIN32
return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool ForceCreateDirRecursive(const std::string& path) {
    try {
        std::filesystem::path dir_path(path);
        bool created = std::filesystem::create_directories(dir_path);
        if (!created && !std::filesystem::is_directory(dir_path)) {
            std::cerr << "Create directory recursively failed.\n";
            return false;
        }
        std::cout << "Recursively create directory " << path << " ok.\n";
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating directory recursively: " << e.what() << std::endl;
        return false;
    }
}

std::string FindFilesWithAllSuffix(const std::string& filePath) {
    fs::path path(filePath);
    fs::path directory = path.parent_path();
    std::string fileName = path.stem().string();

    std::vector<std::string> foundFiles;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry.status())) {
            std::string currentFileName = entry.path().filename().string();
            if (currentFileName.find(fileName) == 0) {
                foundFiles.push_back(entry.path().string());
                // std::cout << "Found file: " << entry.path() << std::endl;
            }
        }
    }

    if (!foundFiles.empty()) {
        return foundFiles[0];
    } else {
        return "";
    }
}

bool RenameFile(const std::string& old_path, const std::string& new_path) {
    try {
        fs::path old_p(old_path);
        fs::path new_p(new_path);
        if (!fs::exists(old_p)) {
            std::cerr << "RenameFile failed, old file not exist. old_path: " << old_path << std::endl;
            return false;
        }

        fs::rename(old_p, new_p);
        // std::cout << "RenameFile ok. old_path: " << old_path << ", new_path: " << new_path << std::endl;
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "RenameFile error: " << e.what() << std::endl;
        return false;
    }
}

std::string RenameRecordFile(const std::string& path) {
    // std::string FindFilesWithAllSuffix(const std::string& filePath);
    std::string fuzzyPath =  FindFilesWithAllSuffix(path);

    std::string newPath = ReplaceSubstring(path, ".recording.00000.rsclbag", ".rsclbag");
    if(newPath.find(".rsclbag") != std::string::npos && RenameFile(path, newPath))
    {
        std::cout << "RenameRecordFile ok. from:" << path << ", to:" << newPath << std::endl;
        return newPath;
    }

    std::cerr << "RenameRecordFile failed. from:" << path << ", to:" << newPath << std::endl;
    return "";
}

std::string readFileToString(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        std::cout << "readFileToString failed, file not exist. filePath: " << filePath << std::endl;
        return "";
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "readFileToString failed, file open failed. filePath: " << filePath << std::endl;
        return "";
    }


    auto fileSize = std::filesystem::file_size(filePath);
    std::string content(8, '\0');

    file.read(content.data(), fileSize);
    return content;
}

std::time_t GetFileCreationTime(const std::string& filepath) {
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        AD_ERROR(utils, "Failed to get file creation time.");
        return 0;
    }
    return file_stat.st_ctime;
}

std::vector<std::string> GetFilesInTimeRange(const std::string& path, uint64_t start_time_ms, uint64_t end_time_ms) {
    std::vector<std::string> res;
    if (start_time_ms > end_time_ms) {
        AD_ERROR(utils, "Invalid time range.");
        return res;
    }
    if (!IsDirExist(path)) {
        AD_ERROR(utils, "Invalid directory path: %s", path.c_str());
        return res;
    }
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (fs::is_regular_file(entry.status())) {
                auto last_write_time = fs::last_write_time(entry);
                auto file_write_time = FileTimeToMs(last_write_time);
                auto create_time = GetFileCreationTime(entry.path().string());
                // auto file_create_time = FileTimeToMs(create_time);
                // if ((file_write_time >= start_time_ms && file_write_time <= end_time_ms) ||
                //     (file_create_time >= start_time_ms && file_create_time <= end_time_ms)) {
                //     res.emplace_back(fs::absolute(entry.path()).string());
                //     AD_WARN(utils, "Found file %s in time range", res.back().c_str());
                //     }
            }
        }
        return res;
    } catch (const fs::filesystem_error& e) {
        return res;
    }
    return res;
}

// bool CopyFileToFolderWithStructure(const std::string& source_file, const std::string& source_path, const std::string& tar_path) {
//     try {
//         if (!fs::exists(source_file) || !fs::is_regular_file(source_file)) {
//             AD_ERROR(utils, "Source file does not exist or is not a regular file: %s", source_file.c_str());
//             return false;
//         }
//         std::string tar_file = common::ReplaceSubstring(source_file, source_path, tar_path);
//         fs::path tar = tar_file;
//         if (!fs::exists(tar.parent_path())) {
//             fs::create_directories(tar.parent_path());
//         }
//         fs::copy_file(source_file, tar_file, overwrite_if_exists);
//         return true;
//     } catch (...) {
//         AD_ERROR(utils, "Error occurred during file copy.");
//         return false;
//     }
// }

//=========================================================
// 时间处理
//=========================================================
uint64_t GetCurrentTimestampMs()
{
    auto now = std::chrono::system_clock::now();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return milliseconds.count();
}

uint64_t GetCurrentTimestamp()
{
    // auto now = std::chrono::system_clock::now();
    // auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    return GetCurrentTimestampMs() * 1000;
}

uint64_t GetCurrentTimestampNs()
{
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    return ns.count();
}

long long getTime() {
    auto now = std::chrono::system_clock::now();
    auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    );
    return milliseconds_since_epoch.count();
}

std::string get14DigitTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);

    int year = tm->tm_year + 1900;
    std::cout << "Current year: " << tm->tm_year << std::endl;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int hour = tm->tm_hour;
    int minute = tm->tm_min;
    int second = tm->tm_sec;

    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << year
        << std::setw(2) << std::setfill('0') << month
        << std::setw(2) << std::setfill('0') << day
        << std::setw(2) << std::setfill('0') << hour
        << std::setw(2) << std::setfill('0') << minute
        << std::setw(2) << std::setfill('0') << second;

    return oss.str();
}

long generateTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    );
    return milliseconds_since_epoch.count();
}

std::string getCurrentTimeFormatted() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);

    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y%m%d%H%M%S");
    oss << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

std::string UnixSecondsToString(
    uint64_t unix_seconds,
    const std::string& format_str) {
    std::time_t t = unix_seconds;
    struct tm ptm;
    struct tm* ret = localtime_r(&t, &ptm);
    if (ret == nullptr) {
        return std::string("");
    }
    uint32_t length = 64;
    std::vector<char> buff(length, '\0');
    strftime(buff.data(), length, format_str.c_str(), ret);
    return std::string(buff.data());
}

std::string TimestampNs2Str(uint64_t timestamp) {
    std::stringstream ss;
    std::chrono::nanoseconds duration(timestamp);
    auto target_time = std::chrono::system_clock::time_point(duration);
    std::time_t time = std::chrono::system_clock::to_time_t(target_time);
    std::tm* tm_info = std::localtime(&time);
    ss << std::put_time(tm_info, "%Y%m%d%H%M%S");
    return ss.str();
}

uint64_t FileTimeToMs(const fs::file_time_type& filetime) {
    auto system_tp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        filetime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    auto duration = system_tp.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return static_cast<uint64_t>(milliseconds);
}

uint64_t MonoTime() {
    auto now = std::chrono::steady_clock::now();
    auto nano_time_point =
        std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    auto epoch = nano_time_point.time_since_epoch();
    uint64_t now_nano =
        std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();
    return now_nano;
}

double ToSecond(uint64_t nanoseconds_) {
    return static_cast<double>(nanoseconds_) / 1000000000UL;
}

std::string getCurrentDateTimeString() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", localTime);

    return std::string(buffer);
}

//=========================================================
// 字符串处理
//=========================================================
std::string Vin() {
    const char* vin_env = getenv("VIN");
    return vin_env ? std::string(vin_env) : std::string("UNKNOWN_VIN");
}

std::string ReplaceSubstring(std::string str, const std::string& from, const std::string& to) {
    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != std::string::npos) {
        str.replace(startPos, from.length(), to);
        startPos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

std::string trim(const std::string &s)
{
    if (s.empty())
        return s;

    auto left = s.begin();
    while (left != s.end() && std::isspace(*left))
        ++left;

    auto right = s.end();
    do
    {
        --right;
    } while (std::isspace(*right) && right > left);

    return std::string(left, right + 1);
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// 生成指定长度的随机字符串
std::string generateRandomString(size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, sizeof(alphanum) - 2);

    std::string randomStr;
    randomStr.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        randomStr += alphanum[distribution(generator)];
    }

    return randomStr;
}

std::string getRandMsgID() {
    std::string dateStr = getCurrentTimeFormatted();
    std::string randomStr = generateRandomString(6);
    return dateStr + randomStr;
}

//=========================================================
// JSON处理
//=========================================================
json ParseJsonFromString(const std::string &resp) {
    return json::parse(resp);
}

void jsonFormater(json& infoJson, json& statusJson) {
    for (auto& [key, value] : statusJson.items()) {
        std::cout << "Key: " << key << ", Value: " << value << std::endl;
        json itemJson;
        itemJson["paraName"] = key;
        itemJson["paraValue"] = value;
        infoJson["status"].emplace_back(itemJson);
    }
}

// //=========================================================
// // 业务相关
// //=========================================================
// std::string MakeRecorderFileName(const std::string& triggerId, const std::string& businessType, uint64_t triggerTimestamp) {
//     auto t_business = GetBusinessType(businessType);
//     std::string data_source = t_business.data_source.empty() ? "Other" : t_business.data_source;
//     std::string data_type = t_business.data_type.empty() ? "AutoDrivingData" : t_business.data_type;
//     std::stringstream ss;
//     ss << Vin() << "_" << data_source << "_" << UnixSecondsToString(triggerTimestamp, "%Y%m%d%H%M%S") << "_" << data_type
//         << "_" << businessType << "_" << triggerId << ".recording";
//     return ss.str();
// }
//
// TBussiness GetBusinessType(const std::string &bt) {
//     for (const auto& bs : kBussiness) {
//         if (bs.bussiness_type == bt) {
//             return bs;
//         }
//     }
//     return TBussiness();
// }
//
// std::string getTokenContent(const std::string& devId, const std::string& vin) {
//     json json_token;
//     json content;
//     content["lat"] = "312914778";
//     content["lon"] = "1212056188";
//     content["vin"] = vin;
//
//     // 设置外层字段
//     json_token["content"] = content.dump();
//     json_token["devId"] = devId;
//     json_token["devType"] = 11;
//     json_token["isEncrypt"] = 0;
//     json_token["msgId"] = common::getRandMsgID();
//     json_token["msgType"] = "obu01";
//     json_token["oemId"] = 1;
//     json_token["requester"] = 2;
//     json_token["timeStamp"] = common::getTime();
//     json_token["verType"] = "OBU-MQTT";
//     json_token["version"] = "v1.3";
//
//
//     return json_token.dump(4);
// }
//
// CPUData readCPUData() {
//     std::ifstream statFile("/proc/stat");
//     if (!statFile.is_open()) {
//         throw std::runtime_error("Could not open /proc/stat file");
//     }
//
//     CPUData data;
//     std::string line;
//     while (std::getline(statFile, line)) {
//         if (line.find("cpu") == 0) { // Look for the first "cpu" line
//             std::istringstream iss(line.substr(4));
//             iss >> data.user >> data.nice >> data.system >> data.idle >> data.iowait >> data.irq >> data.softirq;
//             break;
//         }
//     }
//
//     statFile.close();
//     return data;
// }
//
// double calculateCPUPercentage(const CPUData& oldData, const CPUData& newData) {
//     long oldTotal = oldData.user + oldData.nice + oldData.system + oldData.idle + oldData.iowait + oldData.irq + oldData.softirq;
//     long newTotal = newData.user + newData.nice + newData.system + newData.idle + newData.iowait + newData.irq + newData.softirq;
//
//     long totalDiff = newTotal - oldTotal;
//     long idleDiff = newData.idle - oldData.idle;
//
//     std::cout << "Total Diff: " <<  totalDiff  << "\n";
//     std::cout  << "Idle Diff:   " <<  idleDiff << "\n";
//
//     return (totalDiff - idleDiff) / static_cast<double>(totalDiff);
// }
//
// void getMemData(SysInfo& sys_info) {
//     std::ifstream meminfo("/proc/meminfo");
//     if (!meminfo.is_open()) {
//         std::cerr << "Failed to open /proc/meminfo" << std::endl;
//         return;
//     }
//
//     std::string line;
//     long totalMem = 0, freeMem = 0, buffers = 0, cached = 0;
//
//     while (std::getline(meminfo, line)) {
//         if (line.find("MemTotal") != std::string::npos) {
//             sscanf(line.c_str(), "MemTotal: %ld kB", &totalMem);
//         } else if (line.find("MemFree") != std::string::npos) {
//             sscanf(line.c_str(), "MemFree: %ld kB", &freeMem);
//         } else if (line.find("Buffers") != std::string::npos) {
//             sscanf(line.c_str(), "Buffers: %ld kB", &buffers);
//         } else if (line.find("Cached") != std::string::npos) {
//             sscanf(line.c_str(), "Cached: %ld kB", &cached);
//         }
//     }
//     meminfo.close();
//
//     long usedMem = totalMem - freeMem - buffers - cached;
//     sys_info.memUsage = static_cast<double>(usedMem) / totalMem;
//     // sys_info.memUsage = std::round(static_cast<double>(usedMem) / totalMem * 100) / 100;
//     std::cout << "sys_info.memUsage" << sys_info.memUsage << std::endl;
// }
//
// void getSpaceData(SysInfo& sys_info, const char *path) {
//     struct statvfs fsinfo;
//     if (statvfs(path, &fsinfo) == 0) {
//         // Total number of blocks in the filesystem.
//         uint64_t totalBlocks = fsinfo.f_blocks;
//         // Number of free blocks available to non-superuser.
//         uint64_t freeBlocks = fsinfo.f_bfree;
//         // Block size (in bytes).
//         uint64_t blockSize = fsinfo.f_frsize;
//
//         uint64_t totalSpace = totalBlocks * blockSize;
//         uint64_t freeSpace = freeBlocks * blockSize;
//
//         std::cout << "Path: " << path << std::endl;
//         std::cout << "Total space: " << (totalSpace / (1024 * 1024)) << " MB" << std::endl;
//         std::cout << "Free space: " << (freeSpace / (1024 * 1024)) << " MB" << std::endl;
//         sys_info.harddriveUsage = static_cast<double>(freeSpace) / totalSpace;
//         // sys_info.harddriveUsage = std::round(static_cast<double>(freeSpace) / totalSpace * 100) / 100;
//
//     } else {
//         throw std::runtime_error("statvfs error");
//     }
//
// }

} 
