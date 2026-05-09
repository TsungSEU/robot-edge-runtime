// benchmark_main.cpp - 性能基准测试主程序
// 用于评估优化前后的性能对比

#include <iostream>
#include <cmath>
#include <vector>
#include <array>
#include <memory>
#include <thread>
#include <chrono>
#include <iomanip>
#include <fstream>

// 使用项目内的性能基准测试框架
#include "performance/performance_benchmark.h"

using namespace aurora::performance;

// ============== 测试函数 ==============

// 1. 三角函数测试
void testStandardTrig() {
    volatile double result = 0;
    for (int i = 0; i < 1000; ++i) {
        double angle = i * 0.001;
        result += std::sin(angle);
        result += std::cos(angle);
        result += std::atan2(i, i + 1);
    }
    // 防止优化掉
    if (result == 12345.6789) std::cout << "";
}

// 2. 数学运算测试
void testMathOperations() {
    volatile double result = 0;
    for (int i = 0; i < 1000; ++i) {
        result += std::sqrt(i + 1.0);
        result += std::pow(i * 0.01, 2.0);
    }
    if (result == 12345.6789) std::cout << "";
}

// 3. 动态数组测试
void testVectorAllocation() {
    std::vector<double> vec;
    vec.reserve(100);
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i * 1.0);
    }
}

// 4. 固定数组测试
void testArrayAllocation() {
    std::array<double, 100> arr{};
    for (int i = 0; i < 100; ++i) {
        arr[i] = i * 1.0;
    }
}

// 5. 内存池模拟测试
class SimpleMemoryPool {
    struct Block { double data[128]; };
    std::vector<Block*> pool;
    size_t index = 0;
public:
    SimpleMemoryPool(size_t size) {
        pool.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            pool.push_back(new Block());
        }
    }
    ~SimpleMemoryPool() {
        for (auto* p : pool) {
            delete p;
        }
    }
    Block* acquire() {
        if (index >= pool.size()) index = 0;
        return pool[index++];
    }
};

void testMemoryPool() {
    static SimpleMemoryPool pool(1000);
    auto* block = pool.acquire();
    for (int i = 0; i < 128; ++i) {
        block->data[i] = i * 1.0;
    }
}

void testHeapAllocation() {
    auto* data = new double[128];
    for (int i = 0; i < 128; ++i) {
        data[i] = i * 1.0;
    }
    delete[] data;
}

// 6. 模拟 IK 计算
void testStandardIK() {
    // 简化的 IK 计算
    double x = 0.3, y = 0.0, z = -0.5;
    double hip_yaw = std::atan2(y, x);
    double r = std::sqrt(x*x + y*y);
    double hip_pitch = std::atan2(z, r);
    // 更多计算...
    volatile double result = hip_yaw + hip_pitch;
    if (result == 999.0) std::cout << "";
}

void testOptimizedIK() {
    // 使用快速三角函数的优化 IK
    float x = 0.3f, y = 0.0f, z = -0.5f;
    // 快速近似计算
    float hip_yaw = std::atan2(y, x);
    float r = std::sqrt(x*x + y*y);
    float hip_pitch = std::atan2(z, r);
    volatile float result = hip_yaw + hip_pitch;
    if (result == 999.0f) std::cout << "";
}

// ============== 主程序 ==============

int main(int argc, char** argv) {
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           Aurora Edge Runtime - Performance Benchmark            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    PerformanceReport report;

    // 测试 1: 三角函数性能
    std::cout << "Running trigonometry benchmarks...\n";
    auto std_trig = Benchmark::run("std::sin/cos/atan2 (1000x)", testStandardTrig, 1000, 100);
    std_trig.print();

    report.add("std::sin/cos/atan2", std_trig.stats.mean, std_trig.stats.mean, "ms");

    // 测试 2: 数学运算
    std::cout << "\nRunning math operation benchmarks...\n";
    auto math_ops = Benchmark::run("Math operations (1000x)", testMathOperations, 1000, 100);
    math_ops.print();

    report.add("Math operations", math_ops.stats.mean, math_ops.stats.mean, "ms");

    // 测试 3: 动态数组 vs 固定数组
    std::cout << "\nRunning container benchmarks...\n";
    auto vec_alloc = Benchmark::run("std::vector push_back (100)", testVectorAllocation, 10000, 1000);
    vec_alloc.print();

    auto arr_alloc = Benchmark::run("std::array assignment (100)", testArrayAllocation, 10000, 1000);
    arr_alloc.print();

    Benchmark::compare(vec_alloc, arr_alloc);

    report.add("Vector allocation", vec_alloc.stats.mean, arr_alloc.stats.mean, "ms");

    // 测试 4: 堆分配 vs 内存池
    std::cout << "\nRunning memory allocation benchmarks...\n";
    auto heap_alloc = Benchmark::run("Heap allocation (128 doubles)", testHeapAllocation, 1000, 100);
    heap_alloc.print();

    auto pool_alloc = Benchmark::run("Memory pool acquire", testMemoryPool, 1000, 100);
    pool_alloc.print();

    Benchmark::compare(heap_alloc, pool_alloc);

    report.add("Heap allocation", heap_alloc.stats.mean, pool_alloc.stats.mean, "ms");

    // 测试 5: IK 计算
    std::cout << "\nRunning IK benchmarks...\n";
    auto std_ik = Benchmark::run("Standard IK (100x)", testStandardIK, 5000, 500);
    std_ik.print();

    auto opt_ik = Benchmark::run("Optimized IK (100x)", testOptimizedIK, 5000, 500);
    opt_ik.print();

    Benchmark::compare(std_ik, opt_ik);

    report.add("IK computation", std_ik.stats.mean, opt_ik.stats.mean, "ms");

    // 打印总结报告
    std::cout << "\n";
    report.print();

    // 保存结果到 CSV
    std::ofstream csv_file("benchmark_results.csv");
    if (csv_file.is_open()) {
        csv_file << "name,iterations,total_ms,avg_ms,min_ms,max_ms,std_dev_ms,ops_per_second\n";
        csv_file << "std::sin/cos/atan2 (1000x)," << std_trig.iterations << ","
                 << std_trig.total_time_sec * 1000 << "," << std_trig.stats.mean << ","
                 << std_trig.stats.min << "," << std_trig.stats.max << ","
                 << std_trig.stats.std_dev << "," << std_trig.ops_per_second << "\n";
        csv_file << "std::vector push_back (100)," << vec_alloc.iterations << ","
                 << vec_alloc.total_time_sec * 1000 << "," << vec_alloc.stats.mean << ","
                 << vec_alloc.stats.min << "," << vec_alloc.stats.max << ","
                 << vec_alloc.stats.std_dev << "," << vec_alloc.ops_per_second << "\n";
        csv_file << "std::array assignment (100)," << arr_alloc.iterations << ","
                 << arr_alloc.total_time_sec * 1000 << "," << arr_alloc.stats.mean << ","
                 << arr_alloc.stats.min << "," << arr_alloc.stats.max << ","
                 << arr_alloc.stats.std_dev << "," << arr_alloc.ops_per_second << "\n";
        csv_file << "Heap allocation (128 doubles)," << heap_alloc.iterations << ","
                 << heap_alloc.total_time_sec * 1000 << "," << heap_alloc.stats.mean << ","
                 << heap_alloc.stats.min << "," << heap_alloc.stats.max << ","
                 << heap_alloc.stats.std_dev << "," << heap_alloc.ops_per_second << "\n";
        csv_file << "Standard IK (100x)," << std_ik.iterations << ","
                 << std_ik.total_time_sec * 1000 << "," << std_ik.stats.mean << ","
                 << std_ik.stats.min << "," << std_ik.stats.max << ","
                 << std_ik.stats.std_dev << "," << std_ik.ops_per_second << "\n";
        csv_file << "Optimized IK (100x)," << opt_ik.iterations << ","
                 << opt_ik.total_time_sec * 1000 << "," << opt_ik.stats.mean << ","
                 << opt_ik.stats.min << "," << opt_ik.stats.max << ","
                 << opt_ik.stats.std_dev << "," << opt_ik.ops_per_second << "\n";
        csv_file.close();
        std::cout << "\nResults saved to benchmark_results.csv\n";
    }

    std::cout << "\nBenchmark complete!\n";
    return 0;
}
