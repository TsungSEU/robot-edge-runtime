#include "file_roller.h"
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

#include "common/utils/sRegex.h"
#include "common/utils/utils.h"

namespace aurora::collector {

namespace fs = std::filesystem;

FileRoller::FileRoller(){
    auto appconfig = AppConfig::getInstance().GetConfig();
    auto it  = appconfig.dataStorage.storagePaths.find("bagPath");
    if (it != appconfig.dataStorage.storagePaths.end()) {
        bagPath = it->second;
    }
    bagPath = bagPath.empty() ? "./data" : bagPath;

    if (!common::ensureDirectoryExists(bagPath)) {
        std::cerr << "Warning: Failed to create bagpath: " << bagPath << std::endl;
    }

    std::cout << "Creating FileRoller with path : " << bagPath  << "\n";

}

std::vector<std::string> FileRoller::getSortedCompressedFiles() const {
    auto appconfig = AppConfig::getInstance().GetConfig();
    std::vector<std::string> files;

    try {
        if (!fs::exists(bagPath) || !fs::is_directory(bagPath)) {
            std::cerr << "Directory does not exist or is not a directory: " << bagPath << std::endl;
            return files;
        }

        std::string pattern = appconfig.dataUpload.filenameRegex;
        for (const auto& entry : fs::directory_iterator(bagPath)) {
            if (entry.is_regular_file()) {
                const std::string filename = entry.path().filename().string();
                if (common::IsMatch(filename, pattern)) {
                    files.push_back(entry.path().string());
                }
            }
        }

        std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
            return fs::last_write_time(a) < fs::last_write_time(b);
        });
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error accessing directory: " << e.what() << std::endl;
    }

    return files;
}

int FileRoller::rollFiles() {
    auto appconfig = AppConfig::getInstance().GetConfig();
    auto files = getSortedCompressedFiles();
    int deletedCount = 0;

    std::cout << "files to delete count = " << files.size() <<std::endl;

    // 如果文件数量超过阈值，删除最早的文件
    while (files.size() > appconfig.dataStorage.rollingDeleteThreshold) {
        try {
            const std::string& oldestFile = files.front();
            if (fs::remove(oldestFile)) {
                std::cout << "Deleted old file: " << oldestFile << std::endl;
                fs::path oldestFile_path(oldestFile);
                std::string encFile = appconfig.dataStorage.storagePaths["encPath"] + "/" + oldestFile_path.filename().string() + ".enc";
                std::cout << "Deleted old enc file: " << encFile << std::endl;
                if(fs::exists(encFile)){
                    fs::remove(encFile);
                }
                deletedCount++;
            } else {
                std::cerr << "Failed to delete file: " << oldestFile << std::endl;
            }
            files.erase(files.begin());
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error deleting file: " << e.what() << std::endl;
            break;
        }
    }

    return deletedCount;
}

}
