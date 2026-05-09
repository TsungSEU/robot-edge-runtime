// performance_benchmark.h - 性能基准测试框架
// 用于评估优化前后的性能对比

#ifndef PERFORMANCE_BENCHMARK_H
#define PERFORMANCE_BENCHMARK_H

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>

namespace aurora::performance {

/**
 * @brief 性能计时器 - 用于测量代码块执行时间
 */
class BenchmarkTimer {
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point end_;
    bool stopped_;

public:
    explicit BenchmarkTimer(std::string name)
        : name_(std::move(name)), stopped_(false) {
        start_ = std::chrono::high_resolution_clock::now();
    }

    ~BenchmarkTimer() {
        if (!stopped_) {
            stop();
        }
    }

    void stop() {
        end_ = std::chrono::high_resolution_clock::now();
        stopped_ = true;
    }

    // 返回微秒
    long long elapsedMicroseconds() const {
        auto end = stopped_ ? end_ : std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
    }

    // 返回毫秒
    double elapsedMilliseconds() const {
        return elapsedMicroseconds() / 1000.0;
    }

    // 返回秒
    double elapsedSeconds() const {
        return elapsedMilliseconds() / 1000.0;
    }
};

/**
 * @brief 统计数据
 */
struct Statistics {
    double min;
    double max;
    double mean;
    double median;
    double std_dev;
    double percentile_95;
    double percentile_99;
    size_t count;

    Statistics() : min(0), max(0), mean(0), median(0), std_dev(0), percentile_95(0), percentile_99(0), count(0) {}

    void print(const std::string& name = "") const {
        if (!name.empty()) {
            std::cout << "=== " << name << " ===" << std::endl;
        }
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Count:     " << count << std::endl;
        std::cout << "  Min:       " << min << " ms" << std::endl;
        std::cout << "  Max:       " << max << " ms" << std::endl;
        std::cout << "  Mean:      " << mean << " ms" << std::endl;
        std::cout << "  Median:    " << median << " ms" << std::endl;
        std::cout << "  Std Dev:   " << std_dev << " ms" << std::endl;
        std::cout << "  95th %:    " << percentile_95 << " ms" << std::endl;
        std::cout << "  99th %:    " << percentile_99 << " ms" << std::endl;
    }

    // 计算两个统计数据的对比
    static Statistics compare(const std::string& name, const Statistics& before, const Statistics& after) {
        Statistics diff;
        diff.count = after.count;
        diff.min = ((after.min - before.min) / before.min) * 100.0;
        diff.max = ((after.max - before.max) / before.max) * 100.0;
        diff.mean = ((after.mean - before.mean) / before.mean) * 100.0;
        diff.median = ((after.median - before.median) / before.median) * 100.0;

        std::cout << "\n========== " << name << " - Performance Comparison ==========\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Metric     Before      After        Change\n";
        std::cout << "  --------   --------    --------    ------\n";
        std::cout << "  Min        " << before.min << " ms    " << after.min << " ms    "
                  << std::setw(6) << diff.min << "%\n";
        std::cout << "  Max        " << before.max << " ms    " << after.max << " ms    "
                  << std::setw(6) << diff.max << "%\n";
        std::cout << "  Mean       " << before.mean << " ms    " << after.mean << " ms    "
                  << std::setw(6) << diff.mean << "%\n";
        std::cout << "  Median     " << before.median << " ms    " << after.median << " ms    "
                  << std::setw(6) << diff.median << "%\n";
        std::cout << "  Std Dev    " << before.std_dev << " ms    " << after.std_dev << " ms    "
                  << std::setw(6) << ((after.std_dev - before.std_dev) / before.std_dev * 100.0) << "%\n";
        std::cout << "=========================================================\n";

        return diff;
    }
};

/**
 * @brief 基准测试结果
 */
struct BenchmarkResult {
    std::string name;
    Statistics stats;
    size_t iterations;
    double total_time_sec;
    double ops_per_second;

    void print() const {
        stats.print(name);
        std::cout << "  Total Time: " << total_time_sec << " s" << std::endl;
        std::cout << "  Throughput: " << ops_per_second << " ops/sec" << std::endl;
    }
};

/**
 * @brief 基准测试工具类
 */
class Benchmark {
public:
    /**
     * @brief 运行基准测试
     * @param name 测试名称
     * @param func 要测试的函数
     * @param iterations 迭代次数
     * @param warmup 预热迭代次数
     */
    static BenchmarkResult run(const std::string& name,
                               std::function<void()> func,
                               size_t iterations = 1000,
                               size_t warmup = 100) {
        BenchmarkResult result;
        result.name = name;
        result.iterations = iterations;

        // 预热
        for (size_t i = 0; i < warmup; ++i) {
            func();
        }

        // 收集数据
        std::vector<double> samples;
        samples.reserve(iterations);

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < iterations; ++i) {
            BenchmarkTimer timer("temp");
            func();
            timer.stop();
            samples.push_back(timer.elapsedMilliseconds());
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.total_time_sec = std::chrono::duration<double>(end - start).count();
        result.ops_per_second = iterations / result.total_time_sec;

        // 计算统计数据
        result.stats = calculateStats(samples);

        return result;
    }

    /**
     * @brief 对比两个基准测试结果
     */
    static void compare(const BenchmarkResult& before, const BenchmarkResult& after) {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║              Performance Comparison: " << before.name << "                    ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Metric        Before              After                Improvement       ║\n";
        std::cout << "║  ─────────────────────────────────────────────────────────────────      ║\n";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "║  Mean (ms)     " << std::setw(10) << before.stats.mean
                  << "           " << std::setw(10) << after.stats.mean
                  << "           " << std::setw(8) << ((after.stats.mean - before.stats.mean) / before.stats.mean * 100.0)
                  << "%         ║\n";

        std::cout << "║  Median (ms)  " << std::setw(10) << before.stats.median
                  << "           " << std::setw(10) << after.stats.median
                  << "           " << std::setw(8) << ((after.stats.median - before.stats.median) / before.stats.median * 100.0)
                  << "%         ║\n";

        std::cout << "║  Min (ms)     " << std::setw(10) << before.stats.min
                  << "           " << std::setw(10) << after.stats.min
                  << "           " << std::setw(8) << ((after.stats.min - before.stats.min) / before.stats.min * 100.0)
                  << "%         ║\n";

        std::cout << "║  Max (ms)     " << std::setw(10) << before.stats.max
                  << "           " << std::setw(10) << after.stats.max
                  << "           " << std::setw(8) << ((after.stats.max - before.stats.max) / before.stats.max * 100.0)
                  << "%         ║\n";

        std::cout << "║  95th % (ms)  " << std::setw(10) << before.stats.percentile_95
                  << "           " << std::setw(10) << after.stats.percentile_95
                  << "           " << std::setw(8) << ((after.stats.percentile_95 - before.stats.percentile_95) / before.stats.percentile_95 * 100.0)
                  << "%         ║\n";

        std::cout << "║  ─────────────────────────────────────────────────────────────────      ║\n";

        std::cout << "║  Throughput    " << std::setw(10) << before.ops_per_second
                  << " ops/s    " << std::setw(10) << after.ops_per_second
                  << " ops/s    " << std::setw(8) << ((after.ops_per_second - before.ops_per_second) / before.ops_per_second * 100.0)
                  << "%         ║\n";

        std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    }

private:
    static Statistics calculateStats(const std::vector<double>& samples) {
        Statistics stats;
        if (samples.empty()) return stats;

        stats.count = samples.size();
        stats.min = *std::min_element(samples.begin(), samples.end());
        stats.max = *std::max_element(samples.begin(), samples.end());

        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        stats.mean = sum / samples.size();

        // 标准差
        double sq_sum = 0.0;
        for (double val : samples) {
            sq_sum += (val - stats.mean) * (val - stats.mean);
        }
        stats.std_dev = std::sqrt(sq_sum / samples.size());

        // 中位数和百分位数
        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());

        stats.median = sorted[sorted.size() / 2];
        stats.percentile_95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
        stats.percentile_99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];

        return stats;
    }
};

/**
 * @brief 性能对比报告
 */
class PerformanceReport {
public:
    struct Comparison {
        std::string metric_name;
        double before_value;
        double after_value;
        std::string unit;
        double improvement_percent;
    };

    std::vector<Comparison> comparisons;

    void add(const std::string& metric, double before, double after, const std::string& unit = "ms") {
        double improvement = ((after - before) / before) * 100.0;
        comparisons.push_back({metric, before, after, unit, improvement});
    }

    void print() const {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                  Performance Optimization Report                                 ║\n";
        std::cout << "║              ======================================                                ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Metric                    Before        After          Improvement       ║\n";
        std::cout << "║  ──────────────────────────────────────────────────────────────────       ║\n";

        for (const auto& comp : comparisons) {
            std::string arrow = (comp.improvement_percent < 0) ? "↑" : "↓";
            std::string color = (comp.improvement_percent < 0) ? " [IMPROVEMENT]" : "[REGRESSION]";

            std::cout << "║  " << std::left << std::setw(24) << comp.metric_name
                      << "  " << std::fixed << std::setprecision(2)
                      << std::setw(10) << comp.before_value << " " << comp.unit
                      << "  " << std::setw(10) << comp.after_value << " " << comp.unit
                      << "      " << arrow << " "
                      << std::setw(6) << std::abs(comp.improvement_percent) << "%"
                      << std::right << std::setw(14) << color
                      << "  ║\n";
        }

        std::cout << "╠══════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Legend: ↑ = Lower is better (time/latency), ↓ = Higher is better (throughput)        ║\n";
        std::cout << "║          [IMPROVEMENT] = Performance got better, [REGRESSION] = Performance got worse     ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════════════╝\n";
    }

    // 保存到文件
    void saveToFile(const std::string& filepath) const {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            return;
        }

        file << "Performance Optimization Report\n";
        file << "================================\n\n";
        file << "Metric,Before,After,Unit,Improvement (%)\n";
        for (const auto& comp : comparisons) {
            file << comp.metric_name << ","
                  << comp.before_value << ","
                  << comp.after_value << ","
                  << comp.unit << ","
                  << comp.improvement_percent << "\n";
        }

        file << "\nSummary:\n";
        double total_improvement = 0;
        int improvement_count = 0;
        for (const auto& comp : comparisons) {
            if (comp.improvement_percent < 0) {  // 优化意味着数值减少（延迟）或增加（吞吐量）
                total_improvement += std::abs(comp.improvement_percent);
                improvement_count++;
            }
        }
        file << "Average Improvement: " << (total_improvement / improvement_count) << "%\n";
    }
};

// 便捷宏
#define BENCHMARK_SCOPE(name) \
    dcp::performance::BenchmarkTimer _bench_timer(name)

#define BENCHMARK_FUNC(name, func, iterations) \
    dcp::performance::Benchmark::run(name, func, iterations)

#define BENCHMARK_COMPARE(before, after) \
    dcp::performance::Benchmark::compare(before, after)

} // namespace aurora::performance
#endif // PERFORMANCE_BENCHMARK_H
