// run_benchmarks.cpp - 性能基准测试执行程序
// 编译: g++ -std=c++17 -O3 -o run_benchmarks run_benchmarks.cpp

#include "benchmark_runner.cpp"
#include "../src/common/performance_utils.h"
#include <cmath>
#include <vector>
#include <random>
#include <iostream>
#include <mutex>
#include <thread>

using namespace aurora::benchmark;
using namespace aurora::performance;

// ==================== 测试用例 ====================

void benchmark_math_functions() {
    BenchmarkSuite suite;

    std::vector<double> angles;
    for (int i = 0; i < 1000; ++i) {
        angles.push_back(i * 2 * M_PI / 1000);
    }

    // 标准三角函数
    suite.addBenchmark("std::sin (1000x)", [&]() {
        volatile double sum = 0;
        for (double angle : angles) {
            sum += std::sin(angle);
        }
        (void)sum;
    }, 10000);

    // 快速三角函数
    suite.addBenchmark("fastSin (1000x)", [&]() {
        volatile double sum = 0;
        auto& trig = getFastTrig();
        for (double angle : angles) {
            sum += trig.fastSin(angle);
        }
        (void)sum;
    }, 10000);

    // 标准 atan2
    suite.addBenchmark("std::atan2 (1000x)", [&]() {
        volatile double sum = 0;
        for (int i = 0; i < 1000; ++i) {
            sum += std::atan2(i, i + 1);
        }
        (void)sum;
    }, 10000);

    // 快速 atan2
    suite.addBenchmark("fastAtan2 (1000x)", [&]() {
        volatile double sum = 0;
        for (int i = 0; i < 1000; ++i) {
            sum += fastAtan2(i, i + 1);
        }
        (void)sum;
    }, 10000);

    // sqrt vs fastInverseSqrt
    suite.addBenchmark("std::sqrt (1000x)", [&]() {
        volatile double sum = 0;
        for (int i = 1; i < 1001; ++i) {
            sum += std::sqrt(static_cast<double>(i));
        }
        (void)sum;
    }, 10000);

    suite.addBenchmark("fastInverseSqrt (1000x)", [&]() {
        volatile double sum = 0;
        for (int i = 1; i < 1001; ++i) {
            sum += fastInverseSqrt(static_cast<float>(i));
        }
        (void)sum;
    }, 10000);

    suite.runAll();
    suite.exportToCsv("tools/performance/results/math_benchmarks.csv");
}

void benchmark_memory_operations() {
    BenchmarkSuite suite;

    constexpr size_t VEC_SIZE = 1000;

    // std::vector push_back
    suite.addBenchmark("std::vector push_back (1000)", [&]() {
        std::vector<double> vec;
        vec.reserve(VEC_SIZE);
        for (size_t i = 0; i < VEC_SIZE; ++i) {
            vec.push_back(static_cast<double>(i));
        }
    }, 10000);

    // FixedVector push_back
    suite.addBenchmark("FixedVector push_back (1000)", [&]() {
        FixedVector<double, 1000> vec;
        for (size_t i = 0; i < VEC_SIZE; ++i) {
            vec.push_back(static_cast<double>(i));
        }
    }, 10000);

    // State allocation (without pool)
    suite.addBenchmark("State allocation (no pool)", [&]() {
        std::vector<std::vector<double>> states;
        states.reserve(100);
        for (int i = 0; i < 100; ++i) {
            states.emplace_back(24);
            for (int j = 0; j < 24; ++j) {
                states.back()[j] = j * 0.1;
            }
        }
    }, 1000);

    // Memory pool allocation
    suite.addBenchmark("StatePool acquire/release", [&]() {
        StatePool<std::array<double, 24>, 1024> pool;
        std::vector<std::array<double, 24>*> states;
        states.reserve(100);
        for (int i = 0; i < 100; ++i) {
            auto* state = pool.acquire();
            if (state) {
                for (int j = 0; j < 24; ++j) {
                    (*state)[j] = j * 0.1;
                }
                states.push_back(state);
            }
        }
        for (auto* state : states) {
            pool.release(state);
        }
    }, 1000);

    suite.runAll();
    suite.exportToCsv("tools/performance/results/memory_benchmarks.csv");
}

void benchmark_robot_ik() {
    BenchmarkSuite suite;

    // 标准IK
    auto standard_ik = [](double x, double y, double z, bool is_left) -> std::vector<double> {
        std::vector<double> joints(6, 0.0);
        const double L1 = 0.375;
        const double L2 = 0.385;
        const double hip_w = 0.1;

        double hip_offset_y = is_left ? (hip_w / 2.0) : (-hip_w / 2.0);
        double dy = hip_offset_y - y;
        double dx = x;
        double dz = z;

        joints[0] = std::atan2(dy, std::sqrt(dx*dx + dz*dz));

        double cos_yaw = std::cos(joints[0]);
        double sin_yaw = std::sin(joints[0]);
        double dx_rot = dx * cos_yaw + dz * sin_yaw;
        double dz_rot = -dx * sin_yaw + dz * cos_yaw;

        joints[1] = std::atan2(dy, -dz_rot);

        double r = std::sqrt(dy*dy + dz_rot*dz_rot);
        double D = std::sqrt(dx_rot*dx_rot + r*r);
        D = std::clamp(D, std::abs(L1 - L2) + 0.001, L1 + L2 - 0.001);

        double cos_knee = (L1*L1 + D*D - L2*L2) / (2.0 * L1 * D);
        cos_knee = std::clamp(cos_knee, -1.0, 1.0);
        double knee_angle = std::acos(cos_knee);
        joints[3] = -knee_angle;

        double alpha = std::atan2(dx_rot, r);
        double cos_hip = (L1*L1 + D*D - L2*L2) / (2.0 * L1 * D);
        cos_hip = std::clamp(cos_hip, -1.0, 1.0);
        double beta = std::acos(cos_hip);
        joints[2] = alpha + beta;

        joints[4] = -(joints[2] + joints[3]);
        joints[5] = -joints[1];

        return joints;
    };

    // 生成测试数据
    std::vector<std::tuple<double, double, double, bool>> test_data;
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist_x(-0.3, 0.3);
    std::uniform_real_distribution<double> dist_y(-0.2, 0.2);
    std::uniform_real_distribution<double> dist_z(-0.8, -0.7);

    for (int i = 0; i < 100; ++i) {
        test_data.push_back({dist_x(rng), dist_y(rng), dist_z(rng), i % 2 == 0});
    }

    // 标准IK
    suite.addBenchmark("Standard IK (100x)", [&]() {
        for (const auto& [x, y, z, is_left] : test_data) {
            volatile auto joints = standard_ik(x, y, z, is_left);
            (void)joints;
        }
    }, 5000);

    // 优化IK
    OptimizedLegIK opt_ik(0.375, 0.385, 0.1);
    suite.addBenchmark("Optimized IK (100x)", [&]() {
        double joints[6];
        for (const auto& [x, y, z, is_left] : test_data) {
            opt_ik.compute(x, y, z, is_left, joints);
            volatile double sum = joints[0] + joints[1];
            (void)sum;
        }
    }, 5000);

    suite.runAll();
    suite.exportToCsv("tools/performance/results/ik_benchmarks.csv");
}

void benchmark_lock_free_vs_mutex() {
    BenchmarkSuite suite;

    constexpr size_t OPS = 1000;

    // 带mutex的队列 (简化版)
    suite.addBenchmark("Mutex-based queue (1000 ops)", [&]() {
        std::vector<int> vec;
        std::mutex mtx;
        for (size_t i = 0; i < OPS; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            vec.push_back(i);
        }
    }, 1000);

    // 无锁环形缓冲区
    suite.addBenchmark("LockFreeRingBuffer (1000 ops)", [&]() {
        LockFreeRingBuffer<int, 2048> buffer;
        for (size_t i = 0; i < OPS; ++i) {
            buffer.push(i);
        }
        int val;
        for (size_t i = 0; i < OPS; ++i) {
            buffer.pop(val);
        }
    }, 1000);

    suite.runAll();
    suite.exportToCsv("tools/performance/results/concurrency_benchmarks.csv");
}

// ==================== 主函数 ====================

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "   Aurora Edge Runtime 基准测试\n";
    std::cout << "========================================\n";

    // 创建结果目录
    system("mkdir -p tools/performance/results");

    std::string test_category = argc > 1 ? argv[1] : "all";

    if (test_category == "all" || test_category == "math") {
        std::cout << "\n========== 数学函数基准测试 ==========\n";
        benchmark_math_functions();
    }

    if (test_category == "all" || test_category == "memory") {
        std::cout << "\n========== 内存操作基准测试 ==========\n";
        benchmark_memory_operations();
    }

    if (test_category == "all" || test_category == "ik") {
        std::cout << "\n========== 逆运动学基准测试 ==========\n";
        benchmark_robot_ik();
    }

    if (test_category == "all" || test_category == "concurrency") {
        std::cout << "\n========== 并发性能基准测试 ==========\n";
        benchmark_lock_free_vs_mutex();
    }

    std::cout << "\n所有基准测试完成！\n";
    std::cout << "结果保存在: tools/performance/results/\n\n";

    return 0;
}
