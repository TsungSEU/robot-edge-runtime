// performance_utils.h - 性能优化工具类

#ifndef PERFORMANCE_UTILS_H
#define PERFORMANCE_UTILS_H

#include <vector>
#include <cmath>
#include <memory>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>

namespace aurora::performance {


/**
 * @brief 快速逆平方根 (Quake III 算法)
 * 比标准 sqrt(1/x) 快约 4倍
 */
inline float fastInverseSqrt(float x) {
    float xhalf = 0.5f * x;
    int i = *reinterpret_cast<int*>(&x);        // 位操作
    i = 0x5f3759df - (i >> 1);                  // 魔术数字
    x = *reinterpret_cast<float*>(&i);
    x = x * (1.5f - xhalf * x * x);             // 牛顿迭代
    return x;
}

/**
 * @brief 快速 atan2 近似
 * 比标准 atan2 快约 3倍，误差约 0.1%
 */
inline float fastAtan2(float y, float x) {
    const float PI = 3.14159265358979323846f;
    const float PI_2 = 1.57079632679489661923f;

    if (x == 0.0f) {
        return (y > 0.0f) ? PI_2 : -PI_2;
    }

    float atan = y / x;
    float result = atan;

    // 多项式近似
    result = atan * (0.999866f + atan * atan * (-0.3302995f));
    result = result * (0.999866f + result * result * (-0.3302995f));

    if (x < 0.0f) {
        if (y > 0.0f) result += PI;
        else result -= PI;
    }

    return result;
}

/**
 * @brief 快速 sin/cos (查找表)
 * 用于机器人仿真中的高频三角函数计算
 * 支持 float 和 double 类型
 */
class FastTrigonometry {
private:
    static constexpr size_t TABLE_SIZE = 4096;
    static constexpr float TABLE_SCALE = TABLE_SIZE / (2.0f * M_PI);
    std::vector<float> sin_table_;
    std::vector<float> cos_table_;

public:
    FastTrigonometry() {
        sin_table_.reserve(TABLE_SIZE);
        cos_table_.reserve(TABLE_SIZE);

        for (size_t i = 0; i < TABLE_SIZE; ++i) {
            float angle = 2.0f * M_PI * i / TABLE_SIZE;
            sin_table_.push_back(std::sin(angle));
            cos_table_.push_back(std::cos(angle));
        }
    }

    // 角度归一化到 [0, 2π) - float 版本
    inline float normalizeAngle(float angle) const {
        while (angle < 0) angle += 2.0f * M_PI;
        while (angle >= 2.0f * M_PI) angle -= 2.0f * M_PI;
        return angle;
    }

    // 角度归一化到 [0, 2π) - double 版本
    inline double normalizeAngle(double angle) const {
        while (angle < 0) angle += 2.0 * M_PI;
        while (angle >= 2.0 * M_PI) angle -= 2.0 * M_PI;
        return angle;
    }

    // 快速 sin - float 版本
    inline float fastSin(float angle) const {
        angle = normalizeAngle(angle);
        size_t idx = static_cast<size_t>(angle * TABLE_SCALE) % TABLE_SIZE;
        return sin_table_[idx];
    }

    // 快速 sin - double 版本
    inline double fastSin(double angle) const {
        angle = normalizeAngle(angle);
        size_t idx = static_cast<size_t>(angle * TABLE_SCALE) % TABLE_SIZE;
        return static_cast<double>(sin_table_[idx]);
    }

    // 快速 cos - float 版本
    inline float fastCos(float angle) const {
        angle = normalizeAngle(angle);
        size_t idx = static_cast<size_t>(angle * TABLE_SCALE) % TABLE_SIZE;
        return cos_table_[idx];
    }

    // 快速 cos - double 版本
    inline double fastCos(double angle) const {
        angle = normalizeAngle(angle);
        size_t idx = static_cast<size_t>(angle * TABLE_SCALE) % TABLE_SIZE;
        return static_cast<double>(cos_table_[idx]);
    }

    // 同时计算 sin 和 cos - float 版本
    inline void fastSinCos(float angle, float& sin_val, float& cos_val) const {
        angle = normalizeAngle(angle);
        size_t idx = static_cast<size_t>(angle * TABLE_SCALE) % TABLE_SIZE;
        sin_val = sin_table_[idx];
        cos_val = cos_table_[idx];
    }

    // 同时计算 sin 和 cos - double 版本
    inline void fastSinCos(double angle, double& sin_val, double& cos_val) const {
        angle = normalizeAngle(angle);
        size_t idx = static_cast<size_t>(angle * TABLE_SCALE) % TABLE_SIZE;
        sin_val = static_cast<double>(sin_table_[idx]);
        cos_val = static_cast<double>(cos_table_[idx]);
    }
};

// 单例访问
inline FastTrigonometry& getFastTrig() {
    static FastTrigonometry instance;
    return instance;
}

// ==================== 内存优化 ====================

/**
 * @brief 固定大小向量 (避免堆分配)
 * 用于替代 std::vector 在已知大小的场景
 */
template<typename T, size_t N>
class FixedVector {
private:
    T data_[N];
    size_t size_;

public:
    FixedVector() : size_(0) {}

    void push_back(const T& value) {
        if (size_ < N) {
            data_[size_++] = value;
        }
    }

    void push_back(T&& value) {
        if (size_ < N) {
            data_[size_++] = std::move(value);
        }
    }

    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }

    size_t size() const { return size_; }
    size_t capacity() const { return N; }
    bool empty() const { return size_ == 0; }

    T* data() { return data_; }
    const T* data() const { return data_; }

    void clear() { size_ = 0; }

    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }
};

/**
 * @brief 状态对象池 (避免频繁分配 State 对象)
 * 针对 costmap.h 中频繁创建 State 的问题
 */
template<typename T, size_t PoolSize = 1024>
class StatePool {
private:
    struct alignas(T) Slot {
        char data[sizeof(T)];
        std::atomic<bool> in_use{false};
    };

    Slot slots_[PoolSize];
    std::atomic<size_t> next_slot_{0};

public:
    /**
     * @brief 获取一个状态对象
     */
    T* acquire() {
        size_t start = next_slot_.load(std::memory_order_relaxed);

        for (size_t i = 0; i < PoolSize; ++i) {
            size_t idx = (start + i) % PoolSize;
            bool expected = false;

            if (slots_[idx].in_use.compare_exchange_strong(expected, true,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                next_slot_.store((idx + 1) % PoolSize, std::memory_order_relaxed);
                return reinterpret_cast<T*>(&slots_[idx].data);
            }
        }

        // 池已满，返回 nullptr（调用者应处理）
        return nullptr;
    }

    /**
     * @brief 归还状态对象
     */
    void release(T* ptr) {
        if (!ptr) return;

        for (size_t i = 0; i < PoolSize; ++i) {
            if (reinterpret_cast<T*>(&slots_[i].data) == ptr) {
                slots_[i].in_use.store(false, std::memory_order_release);
                return;
            }
        }
    }

    /**
     * @brief 获取使用率
     */
    double utilization() const {
        size_t used = 0;
        for (size_t i = 0; i < PoolSize; ++i) {
            if (slots_[i].in_use.load(std::memory_order_relaxed)) {
                ++used;
            }
        }
        return static_cast<double>(used) / PoolSize;
    }
};

// ==================== 逆运动学优化 ====================

/**
 * @brief 优化的腿部逆运动学
 * 预计算三角函数，使用查找表
 */
class OptimizedLegIK {
private:
    const FastTrigonometry& trig_;
    double L1_;  // 上腿长度
    double L2_;  // 下腿长度
    double hip_width_;

    // 常用值的预计算
    double L1_sq_;
    double L2_sq_;
    double L1_plus_L2_;
    double L1_minus_L2_;

public:
    OptimizedLegIK(double upper_leg, double lower_leg, double hip_w)
        : trig_(getFastTrig())
        , L1_(upper_leg)
        , L2_(lower_leg)
        , hip_width_(hip_w)
    {
        L1_sq_ = L1_ * L1_;
        L2_sq_ = L2_ * L2_;
        L1_plus_L2_ = L1_ + L2_;
        L1_minus_L2_ = std::abs(L1_ - L2_);
    }

    /**
     * @brief 计算腿部关节角度 (优化版)
     * @param foot_x, foot_y, foot_z 足端相对髋关节位置
     * @param is_left 是否左腿
     * @param[out] joints 6个关节角度
     */
    void compute(double foot_x, double foot_y, double foot_z,
                 bool is_left, double* joints) {
        // 1. hip_yaw - 使用快速 atan2
        double hip_offset_y = is_left ? (hip_width_ * 0.5) : (-hip_width_ * 0.5);
        double dy = hip_offset_y - foot_y;
        double dxz_sq = foot_x * foot_x + foot_z * foot_z;
        joints[0] = fastAtan2(dy, std::sqrt(dxz_sq));

        // 2. 旋转坐标系
        float cos_yaw = trig_.fastCos(joints[0]);
        float sin_yaw = trig_.fastSin(joints[0]);
        double dx_rot = foot_x * cos_yaw + foot_z * sin_yaw;
        double dz_rot = -foot_x * sin_yaw + foot_z * cos_yaw;

        // 3. hip_roll
        joints[1] = fastAtan2(dy, -dz_rot);

        // 4. 在 sagittal 平面的距离
        double r = std::sqrt(dy * dy + dz_rot * dz_rot);

        // 5. 到脚踝的距离 (限制在可达范围内)
        double D = std::sqrt(dx_rot * dx_rot + r * r);
        D = std::clamp(D, L1_minus_L2_ + 0.001, L1_plus_L2_ - 0.001);

        // 6. 使用余弦定理求膝关节
        // cos(knee) = (L1² + D² - L2²) / (2*L1*D)
        double cos_knee = (L1_sq_ + D * D - L2_sq_) / (2.0 * L1_ * D);
        cos_knee = std::clamp(cos_knee, -1.0, 1.0);
        double knee_angle = std::acos(cos_knee);
        joints[3] = -knee_angle;  // 膝关节向后弯曲

        // 7. 髋关节俯仰
        double alpha = fastAtan2(dx_rot, r);
        double cos_hip = (L1_sq_ + D * D - L2_sq_) / (2.0 * L1_ * D);
        cos_hip = std::clamp(cos_hip, -1.0, 1.0);
        double beta = std::acos(cos_hip);
        joints[2] = alpha + beta;

        // 8. 踝关节 (保持脚水平)
        joints[4] = -(joints[2] + joints[3]);
        joints[5] = -joints[1];
    }
};

// ==================== 无锁环形缓冲区 ====================

/**
 * @brief 无锁单生产者单消费者环形缓冲区
 * 用于机器人传感器数据的高速传递
 */
template<typename T, size_t Capacity>
class LockFreeRingBuffer {
private:
    std::array<T, Capacity> buffer_;
    std::atomic<size_t> write_idx_{0};
    std::atomic<size_t> read_idx_{0};

public:
    bool push(const T& value) {
        size_t write = write_idx_.load(std::memory_order_relaxed);
        size_t next_write = (write + 1) % Capacity;
        size_t read = read_idx_.load(std::memory_order_acquire);

        if (next_write == read) {
            return false;  // 缓冲区满
        }

        buffer_[write] = value;
        write_idx_.store(next_write, std::memory_order_release);
        return true;
    }

    bool pop(T& value) {
        size_t read = read_idx_.load(std::memory_order_relaxed);
        size_t write = write_idx_.load(std::memory_order_acquire);

        if (read == write) {
            return false;  // 缓冲区空
        }

        value = buffer_[read];
        size_t next_read = (read + 1) % Capacity;
        read_idx_.store(next_read, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return write_idx_.load(std::memory_order_acquire) ==
               read_idx_.load(std::memory_order_acquire);
    }

    size_t size() const {
        size_t write = write_idx_.load(std::memory_order_acquire);
        size_t read = read_idx_.load(std::memory_order_acquire);
        return (write >= read) ? (write - read) : (Capacity - read + write);
    }
};

// ==================== 性能计时器 ====================

/**
 * @brief 高精度性能计时器
 * 用于测量代码块的执行时间
 */
class ScopedTimer {
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
    bool destroyed_;

public:
    explicit ScopedTimer(const std::string& name)
        : name_(name), destroyed_(false) {
        start_ = std::chrono::high_resolution_clock::now();
    }

    ~ScopedTimer() {
        if (!destroyed_) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start_).count();
            std::cout << "[TIMER] " << name_ << ": " << duration << " μs" << std::endl;
        }
    }

    // 禁止拷贝
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
};

/**
 * @brief 累积统计计时器
 */
class AccumulatingTimer {
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::nanoseconds total_{0};
    size_t count_{0};

public:
    explicit AccumulatingTimer(const std::string& name) : name_(name) {}

    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        auto end = std::chrono::high_resolution_clock::now();
        total_ += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
        ++count_;
    }

    void print() const {
        double avg_us = total_.count() / 1000.0 / count_;
        std::cout << "[TIMER] " << name_ << ": avg=" << avg_us << " μs, count=" << count_ << std::endl;
    }

    double getAverageMs() const {
        return total_.count() / 1000000.0 / count_;
    }
};

#define SCOPED_TIMER(name) \
    aurora::performance::ScopedTimer _timer(name)

#define TIMER_START(name) \
    aurora::performance::AccumulatingTimer _timer(name); \
    _timer.start()

#define TIMER_STOP(name) \
    _timer.stop()

#define TIMER_PRINT(name) \
    _timer.print()

// ==================== SIMD 优化工具 ====================

// 检测平台 SIMD 支持
#if defined(__aarch64__) || defined(__arm__)
    #include <arm_neon.h>
    #define AURORA_HAS_NEON 1
#else
    #define AURORA_HAS_NEON 0
#endif

#if defined(__AVX2__)
    #define AURORA_HAS_AVX2 1
#elif defined(__AVX__)
    #define AURORA_HAS_AVX 1
#elif defined(__SSE4_2__)
    #define AURORA_HAS_SSE4_2 1
#else
    #define AURORA_HAS_SSE4_2 0
    #define AURORA_HAS_AVX 0
    #define AURORA_HAS_AVX2 0
#endif

#if AURORA_HAS_AVX2 || AURORA_HAS_AVX || AURORA_HAS_SSE4_2
    #include <immintrin.h>
#endif

/**
 * @brief SIMD 加速的 softmax
 */
inline void softmaxSIMD(const float* logits, float* probs, int size) {
    if (size == 0) return;

#if AURORA_HAS_AVX2
    // AVX2 实现
    __m256 max_vec = _mm256_set1_ps(-FLT_MAX);
    int i;
    for (i = 0; i + 8 <= size; i += 8) {
        __m256 curr = _mm256_loadu_ps(logits + i);
        max_vec = _mm256_max_ps(max_vec, curr);
    }
    float max_val = _mm256_cvtss_f32(_mm256_hadd_ps(_mm256_hadd_ps(max_vec, max_vec), max_vec));
    for (; i < size; i++) max_val = std::max(max_val, logits[i]);

    __m256 sum_vec = _mm256_setzero_ps();
    for (i = 0; i + 8 <= size; i += 8) {
        __m256 logits_vec = _mm256_loadu_ps(logits + i);
        __m256 exp_vec = _mm256_exp_ps(_mm256_sub_ps(logits_vec, _mm256_set1_ps(max_val)));
        _mm256_storeu_ps(probs + i, exp_vec);
        sum_vec = _mm256_add_ps(sum_vec, exp_vec);
    }
    float sum = _mm256_cvtss_f32(_mm256_hadd_ps(_mm256_hadd_ps(sum_vec, sum_vec), sum_vec));
    for (; i < size; i++) { probs[i] = std::exp(logits[i] - max_val); sum += probs[i]; }

    float inv_sum = (sum > 0.0f) ? (1.0f / sum) : 1.0f;
    __m256 inv_sum_vec = _mm256_set1_ps(inv_sum);
    for (i = 0; i + 8 <= size; i += 8) {
        __m256 probs_vec = _mm256_loadu_ps(probs + i);
        probs_vec = _mm256_mul_ps(probs_vec, inv_sum_vec);
        _mm256_storeu_ps(probs + i, probs_vec);
    }
    for (; i < size; i++) probs[i] *= inv_sum;

#elif AURORA_HAS_NEON
    // ARM NEON 实现
    float32x4_t max_vec = vdupq_n_f32(-FLT_MAX);
    int i;
    for (i = 0; i + 4 <= size; i += 4) {
        float32x4_t curr = vld1q_f32(logits + i);
        max_vec = vmaxq_f32(max_vec, curr);
    }
    float max_val = vmaxvq_f32(max_vec);
    for (; i < size; i++) max_val = std::max(max_val, logits[i]);

    float32x4_t sum_vec = vdupq_n_f32(0.0f);
    for (i = 0; i + 4 <= size; i += 4) {
        float32x4_t logits_vec = vld1q_f32(logits + i);
        float32x4_t exp_vec = vexpq_f32(vsubq_f32(logits_vec, vdupq_n_f32(max_val)));
        vst1q_f32(probs + i, exp_vec);
        sum_vec = vaddq_f32(sum_vec, exp_vec);
    }
    float sum = vaddvq_f32(sum_vec);
    for (; i < size; i++) { probs[i] = std::exp(logits[i] - max_val); sum += probs[i]; }

    float inv_sum = (sum > 0.0f) ? (1.0f / sum) : 1.0f;
    float32x4_t inv_sum_vec = vdupq_n_f32(inv_sum);
    for (i = 0; i + 4 <= size; i += 4) {
        float32x4_t probs_vec = vld1q_f32(probs + i);
        probs_vec = vmulq_f32(probs_vec, inv_sum_vec);
        vst1q_f32(probs + i, probs_vec);
    }
    for (; i < size; i++) probs[i] *= inv_sum;

#else
    // 标准实现
    float max_val = *std::max_element(logits, logits + size);
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        probs[i] = std::exp(logits[i] - max_val);
        sum += probs[i];
    }
    float inv_sum = (sum > 0.0f) ? (1.0f / sum) : 1.0f;
    for (int i = 0; i < size; i++) probs[i] *= inv_sum;
#endif
}

/**
 * @brief SIMD 加速的点积 (float)
 */
inline float dotProductSIMD(const float* a, const float* b, int size) {
#if AURORA_HAS_AVX2
    __m256 sum_vec = _mm256_setzero_ps();
    int i;
    for (i = 0; i + 8 <= size; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum_vec = _mm256_add_ps(sum_vec, _mm256_mul_ps(va, vb));
    }
    float sum = _mm256_cvtss_f32(_mm256_hadd_ps(_mm256_hadd_ps(sum_vec, sum_vec), sum_vec));
    for (; i < size; i++) sum += a[i] * b[i];
    return sum;

#elif AURORA_HAS_NEON
    float32x4_t sum_vec = vdupq_n_f32(0.0f);
    int i;
    for (i = 0; i + 4 <= size; i += 4) {
        float32x4_t a_vec = vld1q_f32(a + i);
        float32x4_t b_vec = vld1q_f32(b + i);
        sum_vec = vmlaq_f32(sum_vec, a_vec, b_vec);
    }
    float sum = vaddvq_f32(sum_vec);
    for (; i < size; i++) sum += a[i] * b[i];
    return sum;

#else
    float sum = 0.0f;
    for (int i = 0; i < size; i++) sum += a[i] * b[i];
    return sum;
#endif
}

/**
 * @brief SIMD 加速的向量加法: result = a + b
 */
inline void vectorAddSIMD(const float* a, const float* b, float* result, int size) {
#if AURORA_HAS_AVX2
    int i;
    for (i = 0; i + 8 <= size; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        _mm256_storeu_ps(result + i, _mm256_add_ps(va, vb));
    }
    for (; i < size; i++) result[i] = a[i] + b[i];

#elif AURORA_HAS_NEON
    int i;
    for (i = 0; i + 4 <= size; i += 4) {
        float32x4_t a_vec = vld1q_f32(a + i);
        float32x4_t b_vec = vld1q_f32(b + i);
        float32x4_t result_vec = vaddq_f32(a_vec, b_vec);
        vst1q_f32(result + i, result_vec);
    }
    for (; i < size; i++) result[i] = a[i] + b[i];

#else
    for (int i = 0; i < size; i++) result[i] = a[i] + b[i];
#endif
}

/**
 * @brief SIMD 加速的向量乘法: result = a * b
 */
inline void vectorMulSIMD(const float* a, const float* b, float* result, int size) {
#if AURORA_HAS_AVX2
    int i;
    for (i = 0; i + 8 <= size; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        _mm256_storeu_ps(result + i, _mm256_mul_ps(va, vb));
    }
    for (; i < size; i++) result[i] = a[i] * b[i];

#elif AURORA_HAS_NEON
    int i;
    for (i = 0; i + 4 <= size; i += 4) {
        float32x4_t a_vec = vld1q_f32(a + i);
        float32x4_t b_vec = vld1q_f32(b + i);
        float32x4_t result_vec = vmulq_f32(a_vec, b_vec);
        vst1q_f32(result + i, result_vec);
    }
    for (; i < size; i++) result[i] = a[i] * b[i];

#else
    for (int i = 0; i < size; i++) result[i] = a[i] * b[i];
#endif
}

} // namespace aurora::performance

#endif // PERFORMANCE_UTILS_H
