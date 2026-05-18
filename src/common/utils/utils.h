//
// Created by xucong on 25-5-7.
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#ifndef UTILS_H
#define UTILS_H

#include <dirent.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <chrono>
#include <iomanip>
#include <random>

#include "nlohmann/json.hpp"
// #include "../data.h"
// #include "../base.h"

namespace aurora::common{
using json = nlohmann::json;
namespace fs = std::filesystem;

//=========================================================
// 文件系统操作
//=========================================================
bool IsDirExist(const std::string &path);
bool DeleteFile(const std::string &path);
bool DeleteFiles(const std::vector<std::string>& inputFilePaths);
void saveDataToFile(const std::string &data, const std::string &filePath);
bool ensureDirectoryExists(const std::string &path);
bool ForceCreateDirRecursive(const std::string &path);
std::string FindFilesWithAllSuffix(const std::string& filePath);
bool RenameFile(const std::string& old_path, const std::string& new_path);
std::string RenameRecordFile(const std::string& path);
// bool CopyFileToFolderWithStructure(const std::string& source_file, const std::string& source_path, const std::string& tar_path);
std::string readFileToString(const std::string& filePath);
std::time_t GetFileCreationTime(const std::string& filepath);
std::vector<std::string> GetFilesInTimeRange(const std::string& path, uint64_t start_time_ms, uint64_t end_time_ms);

//=========================================================
// 时间处理
//=========================================================
uint64_t GetCurrentTimestamp();
uint64_t GetCurrentTimestampNs();
uint64_t GetCurrentTimestampMs();
long long getTime();
std::string get14DigitTimestamp();
long generateTimestamp();
std::string getCurrentTimeFormatted();
std::string TimestampNs2Str(uint64_t timestamp);
uint64_t FileTimeToMs(const fs::file_time_type& filetime);
std::string UnixSecondsToString(uint64_t unix_seconds, const std::string& format_str = "%Y-%m-%d-%H:%M:%S");
uint64_t MonoTime();
double ToSecond(uint64_t nanoseconds_);
std::string getCurrentDateTimeString();

//=========================================================
// 字符串处理
//=========================================================
std::string Vin();
std::string ReplaceSubstring(std::string str, const std::string& from, const std::string& to);
std::string trim(const std::string &s);
std::vector<std::string> split(const std::string& s, char delimiter);

//=========================================================
// JSON处理
//=========================================================
json ParseJsonFromString(const std::string &resp);
void jsonFormater(json& infoJson, json& statusJson);

//=========================================================
// 业务相关
//=========================================================
// std::string MakeRecorderFileName(const std::string& triggerId, const std::string& businessType, uint64_t triggerTimestamp);
// TBussiness GetBusinessType(const std::string &bt);
// std::string getRandMsgID();
// std::string getTokenContent(const std::string& devId, const std::string& vin);
// CPUData readCPUData();
// double calculateCPUPercentage(const CPUData& oldData, const CPUData& newData);
// void getMemData(SysInfo& sys_info);
// void getSpaceData(SysInfo& sys_info, const char *path);

}

#endif 
