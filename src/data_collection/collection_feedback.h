// collection_feedback.h
#ifndef COLLECTION_FEEDBACK_H
#define COLLECTION_FEEDBACK_H

#include <vector>
#include <memory>
#include <functional>
#include <deque>
#include "common/utils/utils.h"
#include "common/types.h"  // For DataPoint
#include "rl_planning_infer/core/auto_planner.h"

namespace aurora::collector {

/**
 * @brief 采集结果反馈结构
 *
 * 将采集执行的结果封装为反馈信号，供AutoPlanner学习和调整
 */
struct CollectionResult {
    // 计划信息
    std::vector<Point> planned_path;        // 计划的路径
    Point planned_goal;                      // 计划的目标

    // 执行信息
    std::vector<Point> executed_path;        // 实际执行的路径
    std::vector<DataPoint> collected_data;   // 实际采集的数据
    size_t attempted_points;                 // 尝试采集的点数
    size_t successful_points;                // 成功采集的点数

    // 环境变化信息
    std::vector<Point> discovered_obstacles; // 发现的新障碍物
    std::vector<Point> sparse_areas_found;   // 发现的稀疏区域
    double actual_cost;                      // 实际执行成本

    // 数据质量评估
    double data_quality_score;               // 数据质量评分 [0-1]
    double data_value;                       // 数据价值（稀疏度奖励）
    double novelty_score;                    // 新颖性评分

    // 时间信息
    double planning_time;                    // 规划耗时
    double execution_time;                   // 执行耗时
    uint64_t timestamp;                      // 时间戳

    CollectionResult()
        : attempted_points(0)
        , successful_points(0)
        , actual_cost(0.0)
        , data_quality_score(0.0)
        , data_value(0.0)
        , novelty_score(0.0)
        , planning_time(0.0)
        , execution_time(0.0)
        , timestamp(0) {}
};

/**
 * @brief 环境状态变化事件
 */
struct EnvironmentChange {
    enum ChangeType {
        NEW_OBSTACLE,          // 新障碍物
        OBSTACLE_CLEARED,      // 障碍物清除
        SPARSE_AREA_DETECTED,  // 发现稀疏区域
        HIGH_DENSITY_DETECTED, // 发现高密度区域
        COVERAGE_TARGET_MET,   // 覆盖率目标达成
        SENSOR_FAILURE         // 传感器故障
    };

    ChangeType type;
    Point location;
    double severity;           // 严重程度 [0-1]
    uint64_t timestamp;

    EnvironmentChange(ChangeType t = SENSOR_FAILURE, Point loc = Point(),
                     double s = 0.0, uint64_t ts = 0)
        : type(t), location(loc), severity(s), timestamp(ts) {}
};

/**
 * @brief 经验元数据（用于上传云端PPO训练）
 *
 * 存储完整的 (state, action, reward, next_state) 元组，
 * 使云端能够进行有效的 PPO 策略梯度更新。
 *
 * 状态向量使用 float16 (uint16_t) 量化存储以平衡精度与带宽：
 * - 75维 × 2字节 = 150字节（原 double 600字节，压缩 4x）
 * - 量化公式：uint16 = (float + 1.0) * 32767 / 2.0，精度 ~3e-5
 *
 * 总大小：~628字节/条（3-DOF）或 ~646字节/条（18-DOF）
 * 1000条/batch ≈ 628-646KB，带宽可接受
 */
struct ExperienceMetadata {
    static constexpr int MAX_STATE_DIM = 78;   // 支持扩展状态维度
    static constexpr int MAX_ACTION_DIM = 18;  // 最大动作维度

    // 状态向量（量化为 float16 存储）
    std::vector<uint16_t> state;        // 当前状态 [state_dim]
    std::vector<uint16_t> next_state;   // 下一状态 [state_dim]

    // 动作向量（量化为 float16 存储）
    std::vector<uint16_t> action;       // 实际动作 [action_dim]

    // 元信息
    float reward;                       // 奖励值 (4字节)
    uint8_t env_change_type;            // 环境变化类型
    uint8_t env_change_severity;        // 严重程度 [0-100]
    uint16_t state_dim;                 // 实际状态维度
    uint16_t action_dim;                // 实际动作维度
    uint64_t timestamp;                 // 时间戳 (8字节)

    ExperienceMetadata()
        : reward(0.0f), env_change_type(0),
          env_change_severity(0), state_dim(0), action_dim(0), timestamp(0) {}

    // 量化工具：float → uint16_t (范围 [-1, 1] → [0, 65535])
    static uint16_t quantize(float value) {
        float clamped = std::max(-1.0f, std::min(1.0f, value));
        return static_cast<uint16_t>((clamped + 1.0f) * 32767.5f);
    }

    // 反量化：uint16_t → float
    static float dequantize(uint16_t value) {
        return static_cast<float>(value) / 32767.5f - 1.0f;
    }

    // 从 float 向量设置状态
    void setState(const std::vector<double>& s) {
        state_dim = static_cast<uint16_t>(std::min(static_cast<int>(s.size()), MAX_STATE_DIM));
        state.resize(state_dim);
        for (uint16_t i = 0; i < state_dim; ++i) {
            state[i] = quantize(static_cast<float>(s[i]));
        }
    }

    // 从 float 向量设置下一状态
    void setNextState(const std::vector<double>& ns) {
        size_t dim = std::min(static_cast<int>(ns.size()), MAX_STATE_DIM);
        next_state.resize(dim);
        // state_dim 已在 setState 中设置，此处不重复赋值
        for (size_t i = 0; i < dim; ++i) {
            next_state[i] = quantize(static_cast<float>(ns[i]));
        }
    }

    // 从 float 向量设置动作
    void setAction(const std::vector<double>& a) {
        action_dim = static_cast<uint16_t>(std::min(static_cast<int>(a.size()), MAX_ACTION_DIM));
        action.resize(action_dim);
        for (uint16_t i = 0; i < action_dim; ++i) {
            action[i] = quantize(static_cast<float>(a[i]));
        }
    }

    // 反量化状态向量
    std::vector<double> getStateVector() const {
        std::vector<double> result(state_dim);
        for (uint16_t i = 0; i < state_dim; ++i) {
            result[i] = static_cast<double>(dequantize(state[i]));
        }
        return result;
    }

    // 反量化下一状态向量
    std::vector<double> getNextStateVector() const {
        std::vector<double> result(state.size());
        for (size_t i = 0; i < state.size(); ++i) {
            result[i] = static_cast<double>(dequantize(next_state[i]));
        }
        return result;
    }

    // 反量化动作向量
    std::vector<double> getActionVector() const {
        std::vector<double> result(action_dim);
        for (uint16_t i = 0; i < action_dim; ++i) {
            result[i] = static_cast<double>(dequantize(action[i]));
        }
        return result;
    }
};

/**
 * @brief CollectionFeedback - 采集反馈管理器（边缘端轻量级版本）
 *
 * 职责：
 * 1. 收集采集执行的结果
 * 2. 评估数据质量和价值（轻量级）
 * 3. 检测环境变化
 * 4. 生成经验元数据（不存储完整状态）
 * 5. 触发动态重规划
 * 6. 上传经验到云端（不进行本地训练）
 *
 * 设计原则：
 * - 轻量级：奖励计算<1ms，内存占用<10MB
 * - 不进行在线训练：避免占用CPU资源
 * - 只收集元数据：压缩比>10x
 */
class CollectionFeedback {
public:
    using ReplanCallback = std::function<void(const EnvironmentChange&)>;
    using UploadCallback = std::function<void(const std::vector<ExperienceMetadata>&)>;

    explicit CollectionFeedback(size_t upload_threshold = 1000);
    ~CollectionFeedback() = default;

    // ========== 反馈处理接口 ==========

    /**
     * @brief 提交采集结果
     * @param result 采集结果
     *
     * 此方法会：
     * 1. 评估数据质量和价值（<1ms）
     * 2. 计算奖励信号
     * 3. 检测环境变化
     * 4. 生成经验元数据（不存储完整状态）
     * 5. 判断是否需要重规划
     * 6. 存储到本地缓冲区（不立即上传）
     */
    void submitCollectionResult(const CollectionResult& result);

    /**
     * @brief 计算奖励（轻量级版本）
     * @param result 采集结果
     * @return 奖励值
     *
     * 奖励组成：
     * - 数据价值奖励（稀疏区域、高价值数据）
     * - 覆盖率奖励
     * - 效率奖励（路径效率、时间效率）
     * - 惩罚（重复访问、障碍物碰撞）
     *
     * 性能：<1ms
     */
    double computeReward(const CollectionResult& result) const;

    /**
     * @brief 检测环境变化
     * @param result 采集结果
     * @return 检测到的环境变化列表
     */
    std::vector<EnvironmentChange> detectEnvironmentChanges(
        const CollectionResult& result) const;

    /**
     * @brief 生成经验元数据（完整 s,a,r,s' 元组）
     * @param result 采集结果
     * @param reward 计算的奖励
     * @param env_changes 环境变化
     * @param state_vector 当前状态向量（归一化后）
     * @param action_vector 执行的动作向量（归一化后）
     * @param next_state_vector 下一状态向量（归一化后）
     * @return 经验元数据
     */
    ExperienceMetadata generateMetadata(
        const CollectionResult& result, double reward,
        const std::vector<EnvironmentChange>& env_changes,
        const std::vector<double>& state_vector = {},
        const std::vector<double>& action_vector = {},
        const std::vector<double>& next_state_vector = {}) const;

    /**
     * @brief 判断是否需要重规划
     * @param result 采集结果
     * @param env_changes 环境变化
     * @return true 如果需要重规划
     */
    bool shouldReplan(const CollectionResult& result,
                     const std::vector<EnvironmentChange>& env_changes) const;

    // ========== 经验管理接口 ==========

    /**
     * @brief 上传经验到云端
     *
     * 当缓冲区达到阈值时自动上传，或手动触发
     */
    void uploadExperiences();

    /**
     * @brief 设置上传阈值
     * @param threshold 触发上传的元数据数量
     */
    void setUploadThreshold(size_t threshold) { upload_threshold_ = threshold; }

    /**
     * @brief 获取缓冲区大小
     */
    size_t getBufferSize() const { return experience_buffer_.size(); }

    /**
     * @brief 清空缓冲区
     */
    void clearBuffer() { experience_buffer_.clear(); }

    // ========== 回调接口 ==========

    /**
     * @brief 设置重规划回调
     */
    void setReplanCallback(ReplanCallback callback) { replan_callback_ = callback; }

    /**
     * @brief 设置上传回调（用于连接云端上传模块）
     */
    void setUploadCallback(UploadCallback callback) { upload_callback_ = callback; }

    // ========== 配置接口 ==========

    /**
     * @brief 设置当前 RL 状态和动作（由 planner 调用，用于构建完整经验元组）
     * @param state 当前归一化状态向量
     * @param action 当前归一化动作向量
     */
    void setCurrentExperience(const std::vector<double>& state,
                              const std::vector<double>& action);

    /**
     * @brief 设置奖励权重
     */
    void setRewardWeights(double data_value, double coverage, double efficiency,
                         double repetition_penalty, double collision_penalty);

    /**
     * @brief 设置重规划阈值
     * @param reward_drop_threshold 奖励下降阈值（低于此值触发重规划）
     * @param env_change_severity_threshold 环境变化严重程度阈值
     * @param failure_rate_threshold 失败率阈值
     */
    void setReplanThresholds(double reward_drop_threshold,
                            double env_change_severity_threshold,
                            double failure_rate_threshold);

    // ========== 统计接口 ==========

    /**
     * @brief 获取奖励统计
     */
    struct RewardStats {
        double total_reward;
        double average_reward;
        double min_reward;
        double max_reward;
        size_t count;

        RewardStats() : total_reward(0), average_reward(0),
                       min_reward(0), max_reward(0), count(0) {}
    };

    RewardStats getRewardStats() const { return reward_stats_; }

    /**
     * @brief 获取重规划统计
     */
    size_t getTotalReplans() const { return total_replans_; }

private:
    // 配置参数
    struct RewardWeights {
        double data_value_weight;
        double coverage_weight;
        double efficiency_weight;
        double repetition_penalty_weight;
        double collision_penalty_weight;
    } reward_weights_;

    struct ReplanThresholds {
        double reward_drop_threshold;
        double env_change_severity_threshold;
        double failure_rate_threshold;
    } replan_thresholds_;

    // 回调函数
    ReplanCallback replan_callback_;
    UploadCallback upload_callback_;     // 云端上传回调（替代 experience_callback）

    // 经验缓冲区（边缘端，轻量级）
    std::deque<ExperienceMetadata> experience_buffer_;
    size_t upload_threshold_;             // 上传阈值（默认1000条）
    size_t total_uploaded_;               // 已上传的经验数量

    // 统计信息
    RewardStats reward_stats_;
    size_t total_replans_;
    double last_reward_;

    // 滑动窗口用于趋势分析
    static constexpr size_t REWARD_WINDOW_SIZE = 10;
    std::vector<double> reward_window_;

    // 哈希函数用于生成动作摘要（保留向后兼容）
    uint32_t hashPath(const std::vector<Point>& path) const;

    // 上一条经验的状态向量（用于构建 next_state）
    std::vector<double> last_state_vector_;

    // 当前经验的状态和动作向量
    std::vector<double> current_state_vector_;
    std::vector<double> current_action_vector_;
};

} // namespace aurora

#endif // COLLECTION_FEEDBACK_H
