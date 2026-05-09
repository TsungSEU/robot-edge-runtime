// platform_adapter.cpp
#include "platform_adapter.h"
#include <iostream>
#include <sstream>
#include <cmath>

namespace aurora::platform {

PlatformAdapter::PlatformAdapter() {
    // 初始化默认配置
    config_.type = PlatformType::UNKNOWN;
    config_.cpu_cores = 1;
    config_.total_memory_mb = 1024;
    config_.has_neon = false;
    config_.has_avx2 = false;
    config_.has_avx512 = false;
    config_.cpu_freq_mhz = 1000.0;

    // 检测平台特性
    detectCpuCores();
    detectMemory();
    detectCpuFrequency();
    detectPlatform();
    detectSIMDSupport();
}

void PlatformAdapter::detectPlatform() {
    // 通过/proc/cpuinfo检测CPU架构
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;

    bool is_arm = false;
    bool is_x86 = false;

    while (std::getline(cpuinfo, line)) {
        // 检查CPU架构
        if (line.find("model name") != std::string::npos) {
            if (line.find("ARM") != std::string::npos ||
                line.find("aarch64") != std::string::npos) {
                is_arm = true;
            } else {
                is_x86 = true;
            }
        }
    }

    // 根据架构和核心数判断平台类型
    if (is_arm) {
        if (config_.cpu_cores >= 4) {
            config_.type = PlatformType::ARM64_LINUX;  // 高端ARM设备
        } else {
            config_.type = PlatformType::ARM_V8;        // 低端嵌入式设备
        }
    } else if (is_x86) {
        config_.type = PlatformType::X86_64_LINUX;
    } else {
        config_.type = PlatformType::UNKNOWN;
    }
}

void PlatformAdapter::detectSIMDSupport() {
    // 检测x86 SIMD支持
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;

    while (std::getline(cpuinfo, line)) {
        if (line.find("flags") != std::string::npos) {
            config_.has_avx2 = (line.find("avx2") != std::string::npos);
            config_.has_avx512 = (line.find("avx512") != std::string::npos);
            break;
        }
    }

    // ARM NEON支持
    if (config_.type == PlatformType::ARM64_LINUX ||
        config_.type == PlatformType::ARM_V8) {
        config_.has_neon = true;  // ARM64默认支持NEON
    }
}

void PlatformAdapter::detectCpuCores() {
    // 尝试使用标准库获取CPU核心数
    config_.cpu_cores = std::thread::hardware_concurrency();

    // 如果获取失败，从/proc/cpuinfo读取
    if (config_.cpu_cores == 0) {
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        int cores = 0;

        while (std::getline(cpuinfo, line)) {
            if (line.find("processor") != std::string::npos) {
                cores++;
            }
        }

        config_.cpu_cores = cores > 0 ? cores : 1;
    }
}

void PlatformAdapter::detectMemory() {
    std::ifstream meminfo("/proc/meminfo");
    std::string key;
    size_t value;

    while (meminfo >> key >> value) {
        if (key == "MemTotal:") {
            config_.total_memory_mb = value / 1024;  // KB -> MB
            break;
        }
    }
}

void PlatformAdapter::detectCpuFrequency() {
    // 尝试从/proc/cpuinfo读取CPU频率
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;

    while (std::getline(cpuinfo, line)) {
        if (line.find("cpu MHz") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string freq_str = line.substr(pos + 1);
                std::istringstream iss(freq_str);
                iss >> config_.cpu_freq_mhz;
                break;
            }
        }
    }

    // 如果读取失败，从/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq读取
    if (config_.cpu_freq_mhz == 0) {
        std::ifstream freq_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (freq_file.good()) {
            int freq_khz;
            freq_file >> freq_khz;
            config_.cpu_freq_mhz = freq_khz / 1000.0;
        }
    }

    // 默认值
    if (config_.cpu_freq_mhz == 0) {
        config_.cpu_freq_mhz = 1000.0;
    }
}

void PlatformAdapter::printInfo() const {
    std::cout << "=== Platform Information ===" << std::endl;
    std::cout << "Platform Type: " << getPlatformName() << std::endl;
    std::cout << "CPU Cores: " << config_.cpu_cores << std::endl;
    std::cout << "Total Memory: " << config_.total_memory_mb << " MB" << std::endl;
    std::cout << "CPU Frequency: " << config_.cpu_freq_mhz << " MHz" << std::endl;
    std::cout << "SIMD Support: " << (hasSIMDSupport() ? "Yes" : "No") << std::endl;
    std::cout << "  - NEON: " << (config_.has_neon ? "Yes" : "No") << std::endl;
    std::cout << "  - AVX2: " << (config_.has_avx2 ? "Yes" : "No") << std::endl;
    std::cout << "  - AVX-512: " << (config_.has_avx512 ? "Yes" : "No") << std::endl;
    std::cout << "Optimal Thread Count: " << getOptimalThreadCount() << std::endl;
    std::cout << "Use Quantized Model: " << (useQuantizedModel() ? "Yes" : "No") << std::endl;
    std::cout << "Recommended Backend: " << getRecommendedInferenceBackend() << std::endl;
    std::cout << "============================" << std::endl;
}

} // namespace aurora::platform
