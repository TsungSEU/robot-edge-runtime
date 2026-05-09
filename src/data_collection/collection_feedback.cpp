// collection_feedback.cpp
#include "collection_feedback.h"
#include "common/log/logger.h"
#include <algorithm>
#include <numeric>
#include <functional>

namespace aurora::collector {

CollectionFeedback::CollectionFeedback(size_t upload_threshold)
    : upload_threshold_(upload_threshold)
    , total_uploaded_(0)
    , total_replans_(0)
    , last_reward_(0.0) {

    // 默认奖励权重
    reward_weights_.data_value_weight = 10.0;
    reward_weights_.coverage_weight = 5.0;
    reward_weights_.efficiency_weight = 2.0;
    reward_weights_.repetition_penalty_weight = -5.0;
    reward_weights_.collision_penalty_weight = -20.0;

    // 默认重规划阈值
    replan_thresholds_.reward_drop_threshold = 0.3;  // 奖励下降30%触发重规划
    replan_thresholds_.env_change_severity_threshold = 0.7;  // 环境变化严重度>0.7
    replan_thresholds_.failure_rate_threshold = 0.5;  // 失败率>50%触发重规划

    reward_window_.reserve(REWARD_WINDOW_SIZE);

    AD_INFO(CollectionFeedback, "CollectionFeedback initialized: "
            "upload_threshold=%zu", upload_threshold_);
}

void CollectionFeedback::submitCollectionResult(const CollectionResult& result) {
    AD_INFO(CollectionFeedback, "Submitting collection result: %zu/%zu points collected, %.2f quality",
            result.successful_points, result.attempted_points, result.data_quality_score);

    // 1. 计算奖励（轻量级，<1ms）
    double reward = computeReward(result);
    AD_INFO(CollectionFeedback, "Computed reward: %.2f", reward);

    // 更新奖励统计
    reward_stats_.total_reward += reward;
    reward_stats_.count++;
    reward_stats_.average_reward = reward_stats_.total_reward / reward_stats_.count;
    reward_stats_.min_reward = std::min(reward_stats_.min_reward, reward);
    reward_stats_.max_reward = std::max(reward_stats_.max_reward, reward);

    // 更新滑动窗口
    reward_window_.push_back(reward);
    if (reward_window_.size() > REWARD_WINDOW_SIZE) {
        reward_window_.erase(reward_window_.begin());
    }

    // 2. 检测环境变化（轻量级，<1ms）
    auto env_changes = detectEnvironmentChanges(result);
    if (!env_changes.empty()) {
        AD_INFO(CollectionFeedback, "Detected %zu environment changes", env_changes.size());
    }

    // 3. 生成经验元数据（完整 s, a, r, s' 元组）
    // 使用当前 result 的状态作为 next_state，上一次保存的状态作为 state
    ExperienceMetadata metadata = generateMetadata(result, reward, env_changes,
                                                     current_state_vector_,
                                                     current_action_vector_,
                                                     {});  // next_state 由下次调用时的 state 填入

    // 将当前状态保存为下一次的 next_state
    // 在 generateMetadata 中处理：如果有 last_state_vector_，用它作为 state，
    // 用 current_state_vector_ 作为 next_state

    // 4. 存储到本地缓冲区（不立即上传）
    experience_buffer_.push_back(metadata);
    AD_DEBUG(CollectionFeedback, "Experience buffer size: %zu/%zu",
             experience_buffer_.size(), upload_threshold_);

    // 5. 判断是否需要上传到云端
    if (experience_buffer_.size() >= upload_threshold_) {
        AD_INFO(CollectionFeedback, "Upload threshold reached, uploading experiences...");
        uploadExperiences();
    }

    // 6. 判断是否需要重规划
    if (shouldReplan(result, env_changes)) {
        total_replans_++;

        // 触发重规划回调
        if (replan_callback_ && !env_changes.empty()) {
            replan_callback_(env_changes[0]);  // 使用最显著的变化
            AD_WARN(CollectionFeedback, "Replan triggered (total: %zu)", total_replans_);
        }
    }

    last_reward_ = reward;
}

double CollectionFeedback::computeReward(const CollectionResult& result) const {
    double reward = 0.0;

    // 1. 数据价值奖励（稀疏区域奖励）
    double data_value_reward = result.data_value * reward_weights_.data_value_weight;
    reward += data_value_reward;

    // 2. 覆盖率奖励（基于新颖性）
    double coverage_reward = result.novelty_score * reward_weights_.coverage_weight;
    reward += coverage_reward;

    // 3. 效率奖励（实际成本 vs 预期）
    double efficiency_reward = 0.0;
    if (result.actual_cost > 0) {
        double success_rate = static_cast<double>(result.successful_points) / result.attempted_points;
        efficiency_reward = success_rate * reward_weights_.efficiency_weight;
    }
    reward += efficiency_reward;

    // 4. 数据质量奖励
    double quality_reward = result.data_quality_score * 2.0;  // 权重2
    reward += quality_reward;

    // 5. 惩罚：重复访问（基于路径相似度）
    double repetition_penalty = 0.0;
    if (result.planned_path.size() > 1) {
        // 计算路径重叠度
        double path_overlap = 0.0;
        // TODO: 实现更精确的重叠度计算
        repetition_penalty = path_overlap * reward_weights_.repetition_penalty_weight;
    }
    reward += repetition_penalty;

    // 6. 惩罚：障碍物/失败
    double failure_penalty = 0.0;
    if (result.attempted_points > 0) {
        double failure_rate = 1.0 - (static_cast<double>(result.successful_points) / result.attempted_points);
        failure_penalty = failure_rate * reward_weights_.collision_penalty_weight;
    }
    reward += failure_penalty;

    return reward;
}

std::vector<EnvironmentChange> CollectionFeedback::detectEnvironmentChanges(
    const CollectionResult& result) const {

    std::vector<EnvironmentChange> changes;

    // 1. 检测新障碍物
    for (const auto& obstacle : result.discovered_obstacles) {
        changes.emplace_back(EnvironmentChange::NEW_OBSTACLE, obstacle, 0.8, result.timestamp);
    }

    // 2. 检测稀疏区域
    for (const auto& sparse : result.sparse_areas_found) {
        changes.emplace_back(EnvironmentChange::SPARSE_AREA_DETECTED, sparse, 0.6, result.timestamp);
    }

    // 3. 检测数据质量下降（可能意味着环境变化）
    if (result.data_quality_score < 0.3 && result.successful_points > 0) {
        // 使用当前位置作为变化位置
        Point last_position = result.executed_path.empty() ?
                             Point() : result.executed_path.back();
        changes.emplace_back(EnvironmentChange::SENSOR_FAILURE, last_position, 0.9, result.timestamp);
    }

    // 4. 检测高失败率
    if (result.attempted_points > 0) {
        double failure_rate = 1.0 - (static_cast<double>(result.successful_points) / result.attempted_points);
        if (failure_rate > replan_thresholds_.failure_rate_threshold) {
            Point avg_position;
            if (!result.executed_path.empty()) {
                for (const auto& p : result.executed_path) {
                    avg_position.x += p.x;
                    avg_position.y += p.y;
                }
                avg_position.x /= result.executed_path.size();
                avg_position.y /= result.executed_path.size();
            }
            changes.emplace_back(EnvironmentChange::HIGH_DENSITY_DETECTED,
                               avg_position, failure_rate, result.timestamp);
        }
    }

    return changes;
}

ExperienceMetadata CollectionFeedback::generateMetadata(
    const CollectionResult& result, double reward,
    const std::vector<EnvironmentChange>& env_changes,
    const std::vector<double>& state_vector,
    const std::vector<double>& action_vector,
    const std::vector<double>& next_state_vector) const {

    ExperienceMetadata metadata;

    // 1. 状态向量（完整归一化向量，量化为 float16）
    if (!state_vector.empty()) {
        metadata.setState(state_vector);
    } else if (!result.executed_path.empty()) {
        // 降级：如果没有提供状态向量，从执行路径提取简化的 2D 位置
        std::vector<double> fallback_state(2);
        fallback_state[0] = result.executed_path.back().x;
        fallback_state[1] = result.executed_path.back().y;
        metadata.setState(fallback_state);
    }

    // 2. 动作向量（原始向量而非哈希）
    if (!action_vector.empty()) {
        metadata.setAction(action_vector);
    }

    // 3. 下一状态向量
    if (!next_state_vector.empty()) {
        metadata.setNextState(next_state_vector);
    } else if (!last_state_vector_.empty()) {
        // 使用上一次保存的状态作为 next_state（延迟一个时间步）
        metadata.setNextState(last_state_vector_);
    }

    // 4. 奖励
    metadata.reward = static_cast<float>(reward);

    // 5. 环境变化标记
    if (!env_changes.empty()) {
        metadata.env_change_type = static_cast<uint8_t>(env_changes[0].type);
        metadata.env_change_severity = static_cast<uint8_t>(env_changes[0].severity * 100);
    }

    // 6. 时间戳
    metadata.timestamp = result.timestamp;

    return metadata;
}

void CollectionFeedback::setCurrentExperience(
    const std::vector<double>& state,
    const std::vector<double>& action) {
    last_state_vector_ = state;
    current_state_vector_ = state;
    current_action_vector_ = action;
}

bool CollectionFeedback::shouldReplan(
    const CollectionResult& result,
    const std::vector<EnvironmentChange>& env_changes) const {

    // 1. 检查是否有严重环境变化
    for (const auto& change : env_changes) {
        if (change.severity >= replan_thresholds_.env_change_severity_threshold) {
            AD_INFO(CollectionFeedback, "Replan triggered: severe environment change (severity=%.2f)",
                    change.severity);
            return true;
        }
    }

    // 2. 检查奖励是否显著下降
    if (!reward_window_.empty()) {
        double recent_avg = std::accumulate(reward_window_.begin(), reward_window_.end(), 0.0) /
                          reward_window_.size();

        if (reward_stats_.count > REWARD_WINDOW_SIZE &&
            recent_avg < (reward_stats_.average_reward * (1.0 - replan_thresholds_.reward_drop_threshold))) {
            AD_INFO(CollectionFeedback, "Replan triggered: reward drop (%.2f -> %.2f)",
                    reward_stats_.average_reward, recent_avg);
            return true;
        }
    }

    // 3. 检查失败率
    if (result.attempted_points > 0) {
        double failure_rate = 1.0 - (static_cast<double>(result.successful_points) / result.attempted_points);
        if (failure_rate > replan_thresholds_.failure_rate_threshold) {
            AD_INFO(CollectionFeedback, "Replan triggered: high failure rate (%.2f)",
                    failure_rate);
            return true;
        }
    }

    return false;
}

void CollectionFeedback::uploadExperiences() {
    if (experience_buffer_.empty()) {
        AD_DEBUG(CollectionFeedback, "No experiences to upload");
        return;
    }

    // 转换为vector
    std::vector<ExperienceMetadata> batch(experience_buffer_.begin(), experience_buffer_.end());

    AD_INFO(CollectionFeedback, "Uploading %zu experiences to cloud (total uploaded: %zu)",
            batch.size(), total_uploaded_ + batch.size());

    // 触发上传回调
    if (upload_callback_) {
        upload_callback_(batch);
        total_uploaded_ += batch.size();
        AD_INFO(CollectionFeedback, "Upload callback triggered successfully");
    } else {
        AD_WARN(CollectionFeedback, "No upload callback set, experiences will be discarded");
    }

    // 清空缓冲区
    experience_buffer_.clear();
}

void CollectionFeedback::setRewardWeights(double data_value, double coverage,
                                          double efficiency, double repetition_penalty,
                                          double collision_penalty) {
    reward_weights_.data_value_weight = data_value;
    reward_weights_.coverage_weight = coverage;
    reward_weights_.efficiency_weight = efficiency;
    reward_weights_.repetition_penalty_weight = repetition_penalty;
    reward_weights_.collision_penalty_weight = collision_penalty;

    AD_INFO(CollectionFeedback, "Reward weights updated: data=%.1f, coverage=%.1f, efficiency=%.1f, "
             "repetition=%.1f, collision=%.1f",
             data_value, coverage, efficiency, repetition_penalty, collision_penalty);
}

void CollectionFeedback::setReplanThresholds(double reward_drop_threshold,
                                             double env_change_severity_threshold,
                                             double failure_rate_threshold) {
    replan_thresholds_.reward_drop_threshold = reward_drop_threshold;
    replan_thresholds_.env_change_severity_threshold = env_change_severity_threshold;
    replan_thresholds_.failure_rate_threshold = failure_rate_threshold;

    AD_INFO(CollectionFeedback, "Replan thresholds updated: reward_drop=%.2f, env_severity=%.2f, "
             "failure_rate=%.2f",
             reward_drop_threshold, env_change_severity_threshold, failure_rate_threshold);
}

uint32_t CollectionFeedback::hashPath(const std::vector<Point>& path) const {
    // 简单的哈希函数，用于生成动作摘要
    uint32_t hash = 0;
    for (const auto& point : path) {
        // 结合x和y坐标
        uint32_t combined = (static_cast<uint32_t>(point.x) << 16) |
                           static_cast<uint32_t>(point.y);
        hash ^= combined + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
}

} // namespace aurora
