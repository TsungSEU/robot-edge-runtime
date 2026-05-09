#pragma once

#include <pthread.h>
#include <vector>
#include <string>
#include <stdexcept>

namespace aurora::common {

/**
 * @brief CPU 亲和性（绑核）工具类
 *
 * 支持在代码层面设置线程的 CPU 亲和性，用于优化实时性能。
 * 适用于对性能要求高的关键线程（如推理线程、控制线程等）。
 *
 * 使用示例:
 * @code
 *   // 绑定到单个 CPU
 *   CpuAffinity::setCurrentThread(0);
 *
 *   // 绑定到多个 CPU (0,2,4)
 *   CpuAffinity::setCurrentThread({0, 2, 4});
 *
 *   // 绑定到 CPU 范围 (2-5)
 *   CpuAffinity::setCurrentThreadRange(2, 5);
 *
 *   // 从环境变量读取 (DCP_CPU_AFFINITY=2-5 或 0,2,4)
 *   CpuAffinity::setCurrentThreadFromEnv("DCP_CPU_AFFINITY");
 * @endcode
 */
class CpuAffinity {
public:
    /**
     * @brief CPU 集合类型
     */
    using CpuSet = cpu_set_t;

    /**
     * @brief 将当前线程绑定到指定的 CPU 核心
     * @param cpu_id CPU 核心编号 (从 0 开始)
     * @return true 成功, false 失败
     */
    static bool setCurrentThread(int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        return setCurrentThread(cpuset);
    }

    /**
     * @brief 将当前线程绑定到多个指定的 CPU 核心
     * @param cpu_ids CPU 核心编号列表
     * @return true 成功, false 失败
     */
    static bool setCurrentThread(const std::vector<int>& cpu_ids) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int cpu_id : cpu_ids) {
            CPU_SET(cpu_id, &cpuset);
        }
        return setCurrentThread(cpuset);
    }

    /**
     * @brief 将当前线程绑定到 CPU 核心范围 [start, end]
     * @param start 起始 CPU 编号
     * @param end 结束 CPU 编号 (包含)
     * @return true 成功, false 失败
     */
    static bool setCurrentThreadRange(int start, int end) {
        if (start < 0 || end < start) {
            return false;
        }
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int cpu = start; cpu <= end; ++cpu) {
            CPU_SET(cpu, &cpuset);
        }
        return setCurrentThread(cpuset);
    }

    /**
     * @brief 将指定线程绑定到 CPU 集合
     * @param thread 线程句柄
     * @param cpuset CPU 集合
     * @return true 成功, false 失败
     */
    static bool setThread(pthread_t thread, const CpuSet& cpuset) {
        int rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            // 静默失败，避免日志刷屏
            return false;
        }
        return true;
    }

    /**
     * @brief 将当前线程绑定到 CPU 集合
     * @param cpuset CPU 集合
     * @return true 成功, false 失败
     */
    static bool setCurrentThread(const CpuSet& cpuset) {
        return setThread(pthread_self(), cpuset);
    }

    /**
     * @brief 从环境变量解析 CPU 亲和性配置并应用到当前线程
     *
     * 支持的格式:
     * - 单个核心: "0"
     * - 多个不连续核心: "0,2,4"
     * - 范围: "0-3"
     * - 混合: "0-3,6-7"
     *
     * @param env_var 环境变量名称
     * @return true 成功, false 失败或环境变量未设置
     */
    static bool setCurrentThreadFromEnv(const char* env_var) {
        const char* affinity_str = std::getenv(env_var);
        if (!affinity_str || affinity_str[0] == '\0') {
            return false;
        }

        std::vector<int> cpus = parseAffinityString(affinity_str);
        if (cpus.empty()) {
            return false;
        }

        return setCurrentThread(cpus);
    }

    /**
     * @brief 获取当前线程的 CPU 亲和性
     * @param cpuset 输出参数，存储当前的 CPU 集合
     * @return true 成功, false 失败
     */
    static bool getCurrentThread(CpuSet& cpuset) {
        int rc = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        return rc == 0;
    }

    /**
     * @brief 获取当前线程绑定的 CPU 核心列表
     * @return CPU 核心编号列表
     */
    static std::vector<int> getCurrentThreadCpus() {
        cpu_set_t cpuset;
        if (!getCurrentThread(cpuset)) {
            return {};
        }

        std::vector<int> cpus;
        int cpu_count = CPU_COUNT(&cpuset);
        for (int cpu = 0; cpu < CPU_SETSIZE && cpus.size() < cpu_count; ++cpu) {
            if (CPU_ISSET(cpu, &cpuset)) {
                cpus.push_back(cpu);
            }
        }
        return cpus;
    }

    /**
     * @brief 获取系统 CPU 总数
     * @return CPU 核心数
     */
    static int getSystemCpuCount() {
        return sysconf(_SC_NPROCESSORS_ONLN);
    }

    /**
     * @brief 清除当前线程的 CPU 亲和性（允许在所有 CPU 上运行）
     * @return true 成功, false 失败
     */
    static bool clearCurrentThread() {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int cpu = 0; cpu < getSystemCpuCount(); ++cpu) {
            CPU_SET(cpu, &cpuset);
        }
        return setCurrentThread(cpuset);
    }

    /**
     * @brief 解析 CPU 亲和性字符串
     *
     * 支持的格式:
     * - 单个核心: "0"
     * - 多个不连续核心: "0,2,4"
     * - 范围: "0-3"
     * - 混合: "0-3,6-7"
     *
     * @param str 亲和性字符串
     * @return CPU 核心编号列表
     */
    static std::vector<int> parseAffinityString(const std::string& str) {
        std::vector<int> cpus;
        std::string token;
        size_t start = 0;
        size_t end = 0;

        while ((end = str.find(',', start)) != std::string::npos) {
            token = str.substr(start, end - start);
            parseToken(token, cpus);
            start = end + 1;
        }
        parseToken(str.substr(start), cpus);

        return cpus;
    }

    /**
     * @brief 打印 CPU 集合的信息
     * @param cpuset CPU 集合
     * @return 格式化字符串 (如 "0-3,6")
     */
    static std::string cpusetToString(const CpuSet& cpuset) {
        std::string result;
        bool in_range = false;
        int range_start = -1;
        int prev_cpu = -1;

        for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (CPU_ISSET(cpu, &cpuset)) {
                if (range_start == -1) {
                    range_start = cpu;
                } else if (cpu != prev_cpu + 1) {
                    // 不连续，结束之前的范围
                    result += formatRange(range_start, prev_cpu);
                    range_start = cpu;
                }
                prev_cpu = cpu;
            } else if (range_start != -1) {
                // 结束范围
                result += formatRange(range_start, prev_cpu);
                range_start = -1;
                prev_cpu = -1;
            }
        }

        // 处理最后一个范围
        if (range_start != -1) {
            result += formatRange(range_start, prev_cpu);
        }

        // 移除末尾的逗号
        if (!result.empty() && result.back() == ',') {
            result.pop_back();
        }

        return result.empty() ? "none" : result;
    }

private:
    static void parseToken(const std::string& token, std::vector<int>& cpus) {
        if (token.empty()) return;

        size_t dash_pos = token.find('-');
        if (dash_pos != std::string::npos) {
            // 范围格式: start-end
            int start = std::stoi(token.substr(0, dash_pos));
            int end = std::stoi(token.substr(dash_pos + 1));
            for (int cpu = start; cpu <= end; ++cpu) {
                cpus.push_back(cpu);
            }
        } else {
            // 单个数字
            cpus.push_back(std::stoi(token));
        }
    }

    static std::string formatRange(int start, int end) {
        if (start == end) {
            return std::to_string(start) + ",";
        } else if (end == start + 1) {
            return std::to_string(start) + "," + std::to_string(end) + ",";
        } else {
            return std::to_string(start) + "-" + std::to_string(end) + ",";
        }
    }
};

} // namespace aurora::common
