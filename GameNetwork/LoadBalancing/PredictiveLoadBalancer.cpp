#include "PredictiveLoadBalancer.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>
#include <numeric>
#include <cmath>

using namespace Utilities;

namespace GameNetwork
{
    PredictiveLoadBalancer::PredictiveLoadBalancer()
    {
        Logger::handle().write(LogTypes::Information, "PredictiveLoadBalancer initialized");
    }

    auto PredictiveLoadBalancer::predict_server_load(const std::string& server_id, 
                                                    std::chrono::seconds future_time) -> PredictionMetrics
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        PredictionMetrics metrics{};
        
        auto it = server_histories_.find(server_id);
        if (it == server_histories_.end() || it->second.load_history.empty())
        {
            metrics.confidence_level = 0.0f;
            return metrics;
        }
        
        const auto& history = it->second;
        
        if (prediction_algorithm_ == "linear_trend")
        {
            float trend = calculate_linear_trend(history.load_history, history.timestamps);
            float current_load = history.load_history.back();
            
            metrics.predicted_load = current_load + (trend * future_time.count());
            metrics.confidence_level = std::min(1.0f, static_cast<float>(history.load_history.size()) / history_window_size_);
        }
        else if (prediction_algorithm_ == "exponential_smoothing")
        {
            metrics.predicted_load = apply_exponential_smoothing(history.load_history);
            metrics.confidence_level = std::min(0.8f, static_cast<float>(history.load_history.size()) / (history_window_size_ * 0.5f));
        }
        
        metrics.prediction_time = std::chrono::steady_clock::now();
        total_predictions_++;
        
        return metrics;
    }

    auto PredictiveLoadBalancer::update_server_metrics(const std::string& server_id, float current_load, float response_time) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& history = server_histories_[server_id];
        auto now = std::chrono::steady_clock::now();
        
        // Add new data
        history.load_history.push_back(current_load);
        history.response_time_history.push_back(response_time);
        history.timestamps.push_back(now);
        
        // Maintain window size
        if (history.load_history.size() > history_window_size_)
        {
            history.load_history.pop_front();
            history.response_time_history.pop_front();
            history.timestamps.pop_front();
        }
        
        // Update trend coefficient
        if (history.load_history.size() >= 2)
        {
            history.trend_coefficient = calculate_linear_trend(history.load_history, history.timestamps);
        }
    }

    auto PredictiveLoadBalancer::select_best_server_predictive(const std::vector<std::string>& available_servers, std::chrono::seconds look_ahead) -> std::string
    {
        if (available_servers.empty())
        {
            return "";
        }
        
        std::string best_server = available_servers[0];
        float best_predicted_load = std::numeric_limits<float>::max();
        
        for (const auto& server_id : available_servers)
        {
            auto prediction = predict_server_load(server_id, look_ahead);
            
            // Consider both predicted load and confidence
            float weighted_load = prediction.predicted_load / std::max(0.1f, prediction.confidence_level);
            
            if (weighted_load < best_predicted_load)
            {
                best_predicted_load = weighted_load;
                best_server = server_id;
            }
        }
        
        Logger::handle().write(LogTypes::Debug, fmt::format(
            "Selected server {} with predicted load: {}", best_server, best_predicted_load));
        
        return best_server;
    }

    auto PredictiveLoadBalancer::set_history_window_size(size_t window_size) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        history_window_size_ = window_size;
    }

    auto PredictiveLoadBalancer::set_prediction_algorithm(const std::string& algorithm) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        prediction_algorithm_ = algorithm;
    }

    auto PredictiveLoadBalancer::get_prediction_accuracy() -> float
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_predictions_ > 0 ? (correct_predictions_ / total_predictions_) : 0.0f;
    }

    auto PredictiveLoadBalancer::get_server_trends() -> std::unordered_map<std::string, float>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::unordered_map<std::string, float> trends;
        for (const auto& [server_id, history] : server_histories_)
        {
            trends[server_id] = history.trend_coefficient;
        }
        return trends;
    }

    auto PredictiveLoadBalancer::calculate_linear_trend(const std::deque<float>& values, const std::deque<std::chrono::steady_clock::time_point>& times) -> float
    {
        if (values.size() < 2) return 0.0f;
        
        size_t n = values.size();
        float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
        
        for (size_t i = 0; i < n; ++i)
        {
            float x = static_cast<float>(i);  // Time index
            float y = values[i];              // Load value
            
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
        }
        
        float slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
        return slope;
    }

    auto PredictiveLoadBalancer::apply_exponential_smoothing(const std::deque<float>& values, float alpha) -> float
    {
        if (values.empty()) return 0.0f;
        if (values.size() == 1) return values[0];
        
        float smoothed = values[0];
        for (size_t i = 1; i < values.size(); ++i)
        {
            smoothed = alpha * values[i] + (1 - alpha) * smoothed;
        }
        
        return smoothed;
    }
}
