#include <deque>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

namespace aurora::collector {
class SignalSmoother {
public:
    /**
     * @param sampling_period_s 采样周期（秒）
     * @param response_time_s   期望响应时间（秒）
     * @param use_ema           是否使用 EMA，否则使用 SMA
     */
    SignalSmoother(double sampling_period_s, double response_time_s, bool use_ema = true)
        : sampling_period_s_(sampling_period_s),
          response_time_s_(response_time_s),
          use_ema_(use_ema),
          ema_initialized_(false),
          ema_(0.0)
    {
        // 计算 EMA α 或 SMA 窗口
        if(use_ema_) {
            // EMA 等效窗口 N_eff = 2/α - 1 → α = 2 / (N_eff + 1)
            size_t N_eff = static_cast<size_t>(std::round(response_time_s_ / sampling_period_s_));
            ema_alpha_ = 2.0 / (N_eff + 1.0);
        } else {
            window_size_ = static_cast<size_t>(std::round(response_time_s_ / sampling_period_s_));
            if(window_size_ < 1) window_size_ = 1;
        }
    }

    // 推入新数据
    void push(double value) {
        if(use_ema_) {
            if(!ema_initialized_) {
                ema_ = value;
                ema_initialized_ = true;
            } else {
                ema_ = ema_alpha_ * value + (1 - ema_alpha_) * ema_;
            }
        } else {
            buffer_.push_back(value);
            if(buffer_.size() > window_size_)
                buffer_.pop_front();
        }
    }

    // 获取平滑值
    double get() const {
        if(use_ema_) {
            return ema_;
        } else {
            if(buffer_.empty()) return 0.0;
            double sum = std::accumulate(buffer_.begin(), buffer_.end(), 0.0);
            return sum / buffer_.size();
        }
    }

    // 获取中值滤波
    double getMedian() const {
        if(buffer_.empty()) return 0.0;
        std::vector<double> tmp(buffer_.begin(), buffer_.end());
        std::sort(tmp.begin(), tmp.end());
        size_t mid = tmp.size() / 2;
        if(tmp.size() % 2 == 0)
            return 0.5 * (tmp[mid-1] + tmp[mid]);
        else
            return tmp[mid];
    }

    // 重置历史数据
    void reset() {
        buffer_.clear();
        ema_initialized_ = false;
        ema_ = 0.0;
    }

private:
    double sampling_period_s_;
    double response_time_s_;
    bool use_ema_;

    // EMA
    bool ema_initialized_;
    double ema_;
    double ema_alpha_;

    // SMA / Median
    size_t window_size_;
    std::deque<double> buffer_;
};
}
