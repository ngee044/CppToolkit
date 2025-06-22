#pragma once

#include <LoadBalancer.h>
#include <SystemMonitor.h>
#include <chrono>
#include <vector>
#include <deque>
#include <unordered_map>
#include <memory>
#include <tuple>
#include <optional>

namespace GameNetwork
{
    namespace LoadBalancing
    {
        // Historical metrics for pattern learning
        struct HistoricalMetrics
        {
            std::chrono::system_clock::time_point timestamp;
            std::chrono::hours hour_of_day;
            std::chrono::days day_of_week;
            uint32_t player_count;
            float average_load;
            float peak_load;
            uint32_t server_count;
            std::vector<float> server_loads;
        };

        // Load prediction result
        struct LoadPrediction
        {
            std::chrono::system_clock::time_point prediction_time;
            float predicted_load;
            float confidence;
            uint32_t predicted_player_count;
            uint32_t recommended_server_count;
        };

        // Scaling decision
        struct ScalingDecision
        {
            enum class Action
            {
                None,
                ScaleUp,
                ScaleDown,
                Rebalance
            };
            
            Action action;
            uint32_t target_server_count;
            std::string reason;
            float urgency; // 0.0 to 1.0
        };
        class PredictiveLoadBalancer : public LoadBalancer
        {
        public:
            PredictiveLoadBalancer();
            virtual ~PredictiveLoadBalancer() = default;

            // Pattern learning
            auto learn_load_patterns(const std::vector<HistoricalMetrics>& history) -> void;
            auto add_historical_data(const HistoricalMetrics& metrics) -> void;
            auto train_prediction_model() -> std::tuple<bool, std::optional<std::string>>;

            // Load prediction
            auto predict_future_load(std::chrono::minutes ahead) -> LoadPrediction;
            auto predict_player_count(std::chrono::system_clock::time_point time) -> uint32_t;
            auto get_load_forecast(std::chrono::hours duration) -> std::vector<LoadPrediction>;

            // Proactive scaling
            auto proactive_scaling() -> ScalingDecision;
            auto should_scale_up() const -> bool;
            auto should_scale_down() const -> bool;
            auto calculate_optimal_server_count(float predicted_load) const -> uint32_t;

            // Pattern analysis
            auto get_peak_hours() const -> std::vector<std::chrono::hours>;
            auto get_weekly_pattern() const -> std::vector<float>; // 7 days
            auto get_daily_pattern() const -> std::vector<float>;  // 24 hours

            // Configuration
            struct PredictionConfig
            {
                uint32_t min_historical_days = 7;
                float scale_up_threshold = 0.75f;
                float scale_down_threshold = 0.25f;
                std::chrono::minutes prediction_window = std::chrono::minutes(30);
                float confidence_threshold = 0.7f;
                bool enable_seasonal_adjustment = true;
            };

            auto configure(const PredictionConfig& config) -> void;
        private:
            // Time series analysis
            auto decompose_time_series() -> void;
            auto calculate_trend() -> std::vector<float>;
            auto calculate_seasonality() -> std::vector<float>;
            auto remove_outliers(std::vector<float>& data) -> void;

            // Pattern matching
            auto find_similar_historical_period(std::chrono::system_clock::time_point time) 
                -> std::optional<HistoricalMetrics>;
            auto calculate_pattern_similarity(const std::vector<float>& pattern1,
                                            const std::vector<float>& pattern2) -> float;

            // Simple ML model (moving average with seasonality)
            struct PredictionModel
            {
                std::vector<float> hourly_coefficients;  // 24 hours
                std::vector<float> daily_coefficients;   // 7 days
                std::vector<float> trend_coefficients;
                float base_load;
                float volatility;
            };

            auto update_model() -> void;
            auto apply_model(std::chrono::system_clock::time_point time) const -> float;

        private:
            PredictionConfig config_;
            PredictionModel model_;
            
            // Historical data storage
            std::deque<HistoricalMetrics> historical_data_;
            
            // Pattern storage
            std::vector<float> daily_pattern_;
            std::vector<float> weekly_pattern_;
            std::vector<std::chrono::hours> peak_hours_;
            
            // Model state
            std::chrono::system_clock::time_point last_training_time_;
            bool model_trained_{false};
            
            mutable std::mutex prediction_mutex_;
        };
    }
}