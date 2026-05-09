// memory_pool.h
#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <stdexcept>

namespace aurora::memory {

/**
 * @brief 通用内存池实现
 *
 * 预分配固定大小的内存块，避免频繁的内存分配/释放
 * 适用于频繁创建和销毁的临时对象
 */
template<typename T>
class MemoryPool {
private:
    /**
     * @brief 内存块结构
     */
    struct Block {
        std::unique_ptr<T[]> data;
        size_t size;
        std::atomic<bool> in_use;
        
        // 显式定义构造函数以确保正确的初始化
        Block() : size(0), in_use(false) {}
        
        // 禁止拷贝构造和拷贝赋值（因为包含 atomic）
        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
        
        // 允许移动构造和移动赋值
        Block(Block&& other) noexcept 
            : data(std::move(other.data)), 
              size(other.size), 
              in_use(other.in_use.load()) {
            other.size = 0;
            other.in_use.store(false);
        }
        
        Block& operator=(Block&& other) noexcept {
            if (this != &other) {
                data = std::move(other.data);
                size = other.size;
                in_use.store(other.in_use.load());
                other.size = 0;
                other.in_use.store(false);
            }
            return *this;
        }
    };

    std::vector<Block> blocks_;
    size_t block_size_;
    size_t max_blocks_;
    mutable std::mutex mutex_;
    std::atomic<size_t> allocated_count_;

public:
    /**
     * @brief 构造函数
     *
     * @param block_size 每个块的元素数量
     * @param max_blocks 最大块数量
     */
    MemoryPool(size_t block_size, size_t max_blocks = 10)
        : block_size_(block_size), max_blocks_(max_blocks), allocated_count_(0) {
        if (block_size == 0) {
            throw std::invalid_argument("Block size must be greater than 0");
        }
        if (max_blocks == 0) {
            throw std::invalid_argument("Max blocks must be greater than 0");
        }
        blocks_.reserve(max_blocks);
    }
    
    // 禁止拷贝构造和拷贝赋值
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    
    // 允许移动构造和移动赋值
    MemoryPool(MemoryPool&& other) noexcept
        : blocks_(std::move(other.blocks_)),
          block_size_(other.block_size_),
          max_blocks_(other.max_blocks_),
          allocated_count_(other.allocated_count_.load()) {
        // 移动后原对象应处于有效但未指定的状态
        other.block_size_ = 0;
        other.max_blocks_ = 0;
        other.allocated_count_.store(0);
    }
    
    MemoryPool& operator=(MemoryPool&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(mutex_);
            std::lock_guard<std::mutex> other_lock(other.mutex_);
            
            blocks_ = std::move(other.blocks_);
            block_size_ = other.block_size_;
            max_blocks_ = other.max_blocks_;
            allocated_count_.store(other.allocated_count_.load());
            
            // 重置源对象
            other.block_size_ = 0;
            other.max_blocks_ = 0;
            other.allocated_count_.store(0);
        }
        return *this;
    }

    /**
     * @brief 获取一个内存块
     *
     * 如果池中有空闲块则复用，否则创建新块（如果未达到上限）
     *
     * @return 指向内存块的指针
     */
    T* acquire() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 查找可用的块
        for (auto& block : blocks_) {
            bool expected = false;
            if (block.in_use.compare_exchange_strong(expected, true)) {
                allocated_count_.fetch_add(1, std::memory_order_relaxed);
                return block.data.get();
            }
        }

        // 没有可用块，创建新块
        if (blocks_.size() < max_blocks_) {
            try {
                Block new_block;
                new_block.data = std::make_unique<T[]>(block_size_);
                new_block.size = block_size_;
                new_block.in_use.store(true, std::memory_order_relaxed);
                blocks_.push_back(std::move(new_block));
                allocated_count_.fetch_add(1, std::memory_order_relaxed);
                return blocks_.back().data.get();
            } catch (const std::bad_alloc&) {
                // 内存分配失败，抛出异常而不是返回临时分配
                throw std::runtime_error("Failed to allocate memory block");
            }
        }

        // 达到最大块数，抛出异常而不是返回临时分配
        throw std::runtime_error("Memory pool exhausted, maximum blocks reached");
    }

    /**
     * @brief 释放一个内存块
     *
     * @param ptr 要释放的内存块指针
     */
    void release(T* ptr) {
        if (!ptr) return; // 安全检查
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 查找是否是池中的内存
        for (auto& block : blocks_) {
            if (block.data.get() == ptr) {
                block.in_use.store(false, std::memory_order_release);
                allocated_count_.fetch_sub(1, std::memory_order_relaxed);
                return;
            }
        }
        
        // 注意：由于现在 acquire() 不会返回临时分配的内存，
        // 这里理论上不应该执行到。但为了安全起见保留这个检查。
        // 如果确实需要支持外部分配的内存，应该使用不同的接口。
    }

    /**
     * @brief 获取当前池的大小（块数量）
     */
    size_t getPoolSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return blocks_.size();
    }

    /**
     * @brief 获取正在使用的块数量
     */
    size_t getUsedBlocks() const {
        return allocated_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取可用块数量
     */
    size_t getAvailableBlocks() const {
        size_t pool_size = getPoolSize();
        size_t used_blocks = getUsedBlocks();
        return pool_size >= used_blocks ? pool_size - used_blocks : 0;
    }

    /**
     * @brief 获取内存使用率
     */
    double getUtilizationRatio() const {
        size_t total = getPoolSize();
        if (total == 0) return 0.0;
        return static_cast<double>(getUsedBlocks()) / total;
    }
    
    /**
     * @brief 预分配指定数量的块
     * 
     * @param count 要预分配的块数量
     */
    void preallocate(size_t count) {
        if (count == 0) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        size_t current_size = blocks_.size();
        size_t target_size = std::min(current_size + count, max_blocks_);
        
        for (size_t i = current_size; i < target_size; ++i) {
            try {
                Block new_block;
                new_block.data = std::make_unique<T[]>(block_size_);
                new_block.size = block_size_;
                new_block.in_use.store(false, std::memory_order_relaxed);
                blocks_.push_back(std::move(new_block));
            } catch (const std::bad_alloc&) {
                // 预分配失败，记录日志或处理错误
                break;
            }
        }
    }
    
    /**
     * @brief 检查内存池是否已满
     */
    bool isFull() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return blocks_.size() >= max_blocks_;
    }
    
    /**
     * @brief 获取块大小
     */
    size_t getBlockSize() const { return block_size_; }
    
    /**
     * @brief 获取最大块数
     */
    size_t getMaxBlocks() const { return max_blocks_; }

    /**
     * @brief 清空内存池（释放所有未使用的块）
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 计算当前使用的块数量
        size_t used_count = 0;
        for (const auto& block : blocks_) {
            if (block.in_use.load(std::memory_order_acquire)) {
                used_count++;
            }
        }
        
        // 更新分配计数
        allocated_count_.store(used_count, std::memory_order_relaxed);

        // 保留已使用的块，释放未使用的块
        auto new_end = std::remove_if(blocks_.begin(), blocks_.end(),
            [](const Block& block) {
                return !block.in_use.load(std::memory_order_acquire);
            });
        blocks_.erase(new_end, blocks_.end());
    }
};

/**
 * @brief 推理缓冲区池
 *
 * 专门用于推理的内存池，管理状态向量和输出向量的内存
 */
class InferenceBufferPool {
private:
    MemoryPool<float> state_pool_;
    MemoryPool<float> output_pool_;
    size_t state_dim_;
    size_t action_dim_;

public:
    /**
     * @brief 构造函数
     *
     * @param state_dim 状态维度
     * @param action_dim 动作维度
     * @param max_buffers 最大缓冲区数量
     */
    InferenceBufferPool(size_t state_dim, size_t action_dim, size_t max_buffers = 5)
        : state_pool_(state_dim, max_buffers),
          output_pool_(action_dim, max_buffers),
          state_dim_(state_dim),
          action_dim_(action_dim) {}

    /**
     * @brief 缓冲区对
     */
    struct BufferPair {
        float* state;
        float* output;
        InferenceBufferPool* pool;
    };

    /**
     * @brief 获取一组缓冲区
     */
    BufferPair acquireBuffers() {
        return {
            state_pool_.acquire(),
            output_pool_.acquire(),
            this
        };
    }

    /**
     * @brief 释放一组缓冲区
     */
    void releaseBuffers(BufferPair& buffers) {
        state_pool_.release(buffers.state);
        output_pool_.release(buffers.output);
    }

    /**
     * @brief 获取状态维度
     */
    size_t getStateDim() const { return state_dim_; }

    /**
     * @brief 获取动作维度
     */
    size_t getActionDim() const { return action_dim_; }

    /**
     * @brief 获取使用率
     */
    double getUtilizationRatio() const {
        return (state_pool_.getUtilizationRatio() +
                output_pool_.getUtilizationRatio()) / 2.0;
    }
};

/**
 * @brief RAII包装器 - 自动管理缓冲区生命周期
 */
class ScopedInferenceBuffer {
    InferenceBufferPool::BufferPair buffers_;

public:
    /**
     * @brief 构造函数 - 获取缓冲区
     */
    explicit ScopedInferenceBuffer(InferenceBufferPool& pool)
        : buffers_(pool.acquireBuffers()) {}

    /**
     * @brief 析构函数 - 自动释放缓冲区
     */
    ~ScopedInferenceBuffer() {
        if (buffers_.pool) {
            buffers_.pool->releaseBuffers(buffers_);
        }
    }

    /**
     * @brief 禁止拷贝
     */
    ScopedInferenceBuffer(const ScopedInferenceBuffer&) = delete;
    ScopedInferenceBuffer& operator=(const ScopedInferenceBuffer&) = delete;

    /**
     * @brief 允许移动
     */
    ScopedInferenceBuffer(ScopedInferenceBuffer&& other) noexcept
        : buffers_(other.buffers_) {
        other.buffers_.pool = nullptr;
    }

    ScopedInferenceBuffer& operator=(ScopedInferenceBuffer&& other) noexcept {
        if (this != &other) {
            if (buffers_.pool) {
                buffers_.pool->releaseBuffers(buffers_);
            }
            buffers_ = other.buffers_;
            other.buffers_.pool = nullptr;
        }
        return *this;
    }

    /**
     * @brief 获取状态缓冲区指针
     */
    float* state() const { return buffers_.state; }

    /**
     * @brief 获取输出缓冲区指针
     */
    float* output() const { return buffers_.output; }

    /**
     * @brief 获取缓冲区所属的池
     */
    InferenceBufferPool* getPool() const { return buffers_.pool; }
};

} // namespace aurora::memory

#endif // MEMORY_POOL_H
