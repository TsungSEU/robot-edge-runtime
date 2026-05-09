// platform_adapter.h
#ifndef PLATFORM_ADAPTER_H
#define PLATFORM_ADAPTER_H

#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace aurora::platform {

/**
 * @brief 支持的平台类型
 */
enum class PlatformType {
    X86_64_LINUX,     // x86_64 Linux (工作站/服务器)
    ARM64_LINUX,      // ARM64 Linux (树莓派/Jetson)
    ARM_V8,           // ARM v8 (嵌入式设备)
    UNKNOWN
};

/**
 * @brief 平台配置结构
 */
struct PlatformConfig {
    PlatformType type;
    int cpu_cores;
    size_t total_memory_mb;
    bool has_neon;       // ARM NEON支持
    bool has_avx2;       // x86 AVX2支持
    bool has_avx512;     // x86 AVX-512支持
    double cpu_freq_mhz; // CPU频率 (MHz)
};

/**
 * @brief 平台适配器 - 提供跨平台的硬件抽象
 *
 * 负责检测运行平台特性，并提供平台特定的优化建议
 */
class PlatformAdapter {
private:
    PlatformConfig config_;

    /**
     * @brief 检测CPU架构
     */
    void detectPlatform();

    /**
     * @brief 检测SIMD指令集支持
     */
    void detectSIMDSupport();

    /**
     * @brief 检测CPU核心数
     */
    void detectCpuCores();

    /**
     * @brief 检测内存大小
     */
    void detectMemory();

    /**
     * @brief 检测CPU频率
     */
    void detectCpuFrequency();

public:
    /**
     * @brief 构造函数，自动检测平台
     */
    PlatformAdapter();

    /**
     * @brief 获取平台类型
     */
    PlatformType getPlatform() const { return config_.type; }

    /**
     * @brief 获取CPU核心数
     */
    int getCpuCores() const { return config_.cpu_cores; }

    /**
     * @brief 获取总内存大小（MB）
     */
    size_t getTotalMemoryMB() const { return config_.total_memory_mb; }

    /**
     * @brief 是否支持SIMD指令集
     */
    bool hasSIMDSupport() const {
        return config_.has_neon || config_.has_avx2 || config_.has_avx512;
    }

    /**
     * @brief 是否支持NEON (ARM)
     */
    bool hasNeonSupport() const { return config_.has_neon; }

    /**
     * @brief 是否支持AVX2 (x86)
     */
    bool hasAvx2Support() const { return config_.has_avx2; }

    /**
     * @brief 获取平台名称
     */
    std::string getPlatformName() const {
        switch (config_.type) {
            case PlatformType::X86_64_LINUX: return "x86_64-linux";
            case PlatformType::ARM64_LINUX: return "arm64-linux";
            case PlatformType::ARM_V8: return "arm-v8";
            default: return "unknown";
        }
    }

    /**
     * @brief 获取优化的线程数（根据平台特性）
     *
     * ARM设备通常功耗敏感，使用较少线程
     * x86设备可以使用更多线程
     */
    int getOptimalThreadCount() const {
        if (config_.type == PlatformType::ARM64_LINUX ||
            config_.type == PlatformType::ARM_V8) {
            return std::min(config_.cpu_cores, 2);  // ARM设备限制为2线程
        } else {
            return std::min(config_.cpu_cores, 4);  // x86设备最多4线程
        }
    }

    /**
     * @brief 是否使用量化模型
     *
     * ARM设备推荐使用量化模型以提升性能和降低功耗
     */
    bool useQuantizedModel() const {
        return config_.type == PlatformType::ARM64_LINUX ||
               config_.type == PlatformType::ARM_V8;
    }

    /**
     * @brief 是否启用大页内存
     *
     * 仅x86设备且内存充足时启用
     */
    bool enableLargePages() const {
        return config_.type == PlatformType::X86_64_LINUX &&
               config_.total_memory_mb > 2048;
    }

    /**
     * @brief 获取推荐的推理后端
     */
    std::string getRecommendedInferenceBackend() const {
        if (config_.type == PlatformType::X86_64_LINUX) {
            if (config_.has_avx512) {
                return "avx512";  // 最佳性能
            } else if (config_.has_avx2) {
                return "avx2";    // 次优性能
            } else {
                return "default";
            }
        } else if (config_.type == PlatformType::ARM64_LINUX ||
                   config_.type == PlatformType::ARM_V8) {
            return "neon";  // ARM NEON优化
        } else {
            return "default";
        }
    }

    /**
     * @brief 获取平台配置
     */
    const PlatformConfig& getConfig() const { return config_; }

    /**
     * @brief 打印平台信息
     */
    void printInfo() const;
};

} // namespace aurora::platform

#endif // PLATFORM_ADAPTER_H
