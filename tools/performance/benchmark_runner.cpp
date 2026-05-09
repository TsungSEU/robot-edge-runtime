// benchmark_runner.cpp - 性能基准测试框架
// 用于测量核心组件的执行时间和资源使用

#ifndef BENCHMARK_RUNNER_H
#define BENCHMARK_RUNNER_H

#include <chrono>
#include <functional>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <memory>
#include <fstream>
#include <sstream>
#include <cmath>
#include <unistd.h>

namespace aurora::benchmark {

/**
 * @brief 基准测试结果
 */
struct BenchmarkResult {
    std::string name;
    size_t iterations;
    double total_time_ms;
    double avg_time_ms;
    double min_time_ms;
    double max_time_ms;
    double std_dev_ms;
    size_t ops_per_second;

    void print() const {
        std::cout << std::left << std::setw(40) << name
                  << std::right << std::setw(10) << std::fixed << std::setprecision(3) << avg_time_ms << " ms"
                  << std::setw(10) << std::fixed << std::setprecision(3) << min_time_ms << " ms"
                  << std::setw(10) << std::fixed << std::setprecision(3) << max_time_ms << " ms"
                  << std::setw(12) << std::fixed << std::setprecision(0) << ops_per_second << " ops/s"
                  << std::endl;
    }

    std::string toCsv() const {
        std::ostringstream ss;
        ss << name << "," << iterations << "," << total_time_ms << ","
           << avg_time_ms << "," << min_time_ms << "," << max_time_ms << ","
           << std_dev_ms << "," << ops_per_second;
        return ss.str();
    }
};

/**
 * @brief 基准测试套件
 */
class BenchmarkSuite {
private:
    struct Benchmark {
        std::string name;
        std::function<void()> setup;
        std::function<void()> benchmark_func;
        std::function<void()> teardown;
        size_t iterations;
        size_t warmup_iterations;
    };

    std::vector<Benchmark> benchmarks_;
    std::vector<BenchmarkResult> results_;

    /**
     * @brief 计算标准差
     */
    static double calculateStdDev(const std::vector<double>& times, double mean) {
        if (times.size() <= 1) return 0.0;
        double sum_sq_diff = 0.0;
        for (double t : times) {
            double diff = t - mean;
            sum_sq_diff += diff * diff;
        }
        return std::sqrt(sum_sq_diff / (times.size() - 1));
    }

public:
    BenchmarkSuite() = default;

    /**
     * @brief 添加基准测试
     */
    void addBenchmark(const std::string& name,
                     std::function<void()> benchmark_func,
                     size_t iterations = 100,
                     size_t warmup_iterations = 10,
                     std::function<void()> setup = nullptr,
                     std::function<void()> teardown = nullptr) {
        benchmarks_.push_back({
            name, setup, benchmark_func, teardown,
            iterations, warmup_iterations
        });
    }

    /**
     * @brief 运行单个基准测试
     */
    BenchmarkResult runBenchmark(const Benchmark& bench) {
        using namespace std::chrono;

        std::vector<double> times;
        times.reserve(bench.iterations);

        // 预热
        for (size_t i = 0; i < bench.warmup_iterations; ++i) {
            if (bench.setup) bench.setup();
            bench.benchmark_func();
            if (bench.teardown) bench.teardown();
        }

        // 正式测试
        for (size_t i = 0; i < bench.iterations; ++i) {
            if (bench.setup) bench.setup();

            auto start = high_resolution_clock::now();
            bench.benchmark_func();
            auto end = high_resolution_clock::now();

            if (bench.teardown) bench.teardown();

            double time_ms = duration<double, std::milli>(end - start).count();
            times.push_back(time_ms);
        }

        // 计算统计数据
        double total_time = std::accumulate(times.begin(), times.end(), 0.0);
        double avg_time = total_time / times.size();
        double min_time = *std::min_element(times.begin(), times.end());
        double max_time = *std::max_element(times.begin(), times.end());
        double std_dev = calculateStdDev(times, avg_time);
        size_t ops_per_sec = (avg_time > 0) ? static_cast<size_t>(1000.0 / avg_time) : 0;

        return {
            bench.name,
            bench.iterations,
            total_time,
            avg_time,
            min_time,
            max_time,
            std_dev,
            ops_per_sec
        };
    }

    /**
     * @brief 运行所有基准测试
     */
    void runAll() {
        results_.clear();
        results_.reserve(benchmarks_.size());

        std::cout << "\n========================================\n";
        std::cout << "   性能基准测试\n";
        std::cout << "========================================\n\n";

        std::cout << std::left << std::setw(40) << "测试名称"
                  << std::right << std::setw(10) << "平均"
                  << std::setw(10) << "最小"
                  << std::setw(10) << "最大"
                  << std::setw(12) << "吞吐量"
                  << std::endl;
        std::cout << std::string(82, '-') << std::endl;

        for (const auto& bench : benchmarks_) {
            auto result = runBenchmark(bench);
            results_.push_back(result);
            result.print();
        }

        std::cout << std::string(82, '-') << std::endl;
        std::cout << "\n测试完成！共 " << results_.size() << " 项测试\n" << std::endl;
    }

    /**
     * @brief 导出结果为CSV
     */
    void exportToCsv(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << std::endl;
            return;
        }

        file << "name,iterations,total_ms,avg_ms,min_ms,max_ms,std_dev_ms,ops_per_second\n";
        for (const auto& result : results_) {
            file << result.toCsv() << "\n";
        }

        file.close();
        std::cout << "结果已导出到: " << filename << std::endl;
    }

    /**
     * @brief 导出结果为Markdown
     */
    void exportToMarkdown(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << std::endl;
            return;
        }

        file << "# 性能基准测试报告\n\n";
        file << "**生成时间**: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
        file << "## 测试结果\n\n";
        file << "| 测试名称 | 迭代次数 | 平均时间 | 最小时间 | 最大时间 | 吞吐量 |\n";
        file << "|---------|---------|---------|---------|---------|--------|\n";

        for (const auto& result : results_) {
            file << "| " << result.name << " | " << result.iterations << " | "
                 << std::fixed << std::setprecision(3) << result.avg_time_ms << " ms | "
                 << result.min_time_ms << " ms | " << result.max_time_ms << " ms | "
                 << result.ops_per_second << " ops/s |\n";
        }

        file.close();
        std::cout << "报告已导出到: " << filename << std::endl;
    }

    const std::vector<BenchmarkResult>& getResults() const {
        return results_;
    }
};

/**
 * @brief 内存使用基准测试
 */
class MemoryBenchmark {
public:
    struct MemoryStats {
        size_t peak_rss_kb;
        size_t current_rss_kb;
        size_t page_faults;
    };

    static MemoryStats getCurrentMemoryUsage() {
        MemoryStats stats = {0, 0, 0};
        std::ifstream statm("/proc/self/statm");
        std::ifstream status("/proc/self/status");

        // 从statm读取页面数
        if (statm.is_open()) {
            size_t pages, rss;
            statm >> pages >> rss;
            stats.current_rss_kb = rss * (sysconf(_SC_PAGESIZE) / 1024);
            statm.close();
        }

        // 从status读取VmPeak
        if (status.is_open()) {
            std::string line;
            while (std::getline(status, line)) {
                if (line.find("VmPeak:") == 0) {
                    std::istringstream iss(line);
                    std::string key;
                    size_t value;
                    std::string unit;
                    iss >> key >> value >> unit;
                    stats.peak_rss_kb = value;
                    break;
                }
            }
            status.close();
        }

        return stats;
    }
};

/**
 * @brief 微基准测试辅助宏
 */
#define BENCHMARK(suite, name, func, iterations) \
    suite.addBenchmark(name, func, iterations)

#define BENCHMARK_WITH_SETUP(suite, name, setup, func, teardown, iterations) \
    suite.addBenchmark(name, func, iterations, 10, setup, teardown)

} // namespace aurora::benchmark

#endif // BENCHMARK_RUNNER_H
