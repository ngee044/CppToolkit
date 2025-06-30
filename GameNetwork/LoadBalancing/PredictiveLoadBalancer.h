#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <deque>
#include <memory>

namespace GameNetwork
{
	struct PredictionMetrics
	{
		float predicted_load;
		float predicted_response_time;
		float confidence_level;
		std::chrono::steady_clock::time_point prediction_time;
	};

	struct ServerLoadHistory
	{
		std::deque<float> load_history;
		std::deque<float> response_time_history;
		std::deque<std::chrono::steady_clock::time_point> timestamps;
		float trend_coefficient;
	};

	class PredictiveLoadBalancer
	{
	public:
		PredictiveLoadBalancer();
		~PredictiveLoadBalancer() = default;

		// Load prediction methods
		auto predict_server_load(const std::string& server_id, 
								std::chrono::seconds future_time) -> PredictionMetrics;
		auto update_server_metrics(const std::string& server_id, float current_load, float response_time) -> void;
        
		// Best server selection with prediction
		auto select_best_server_predictive(const std::vector<std::string>& available_servers, std::chrono::seconds look_ahead = std::chrono::seconds(30)) -> std::string;

		// Configuration
		auto set_history_window_size(size_t window_size) -> void;
		auto set_prediction_algorithm(const std::string& algorithm) -> void;

		// Analytics
		auto get_prediction_accuracy() -> float;
		auto get_server_trends() -> std::unordered_map<std::string, float>;

	private:
		auto calculate_linear_trend(const std::deque<float>& values, const std::deque<std::chrono::steady_clock::time_point>& times) -> float;
		auto apply_exponential_smoothing(const std::deque<float>& values, float alpha = 0.3f) -> float;

	private:
		mutable std::mutex mutex_;
		std::unordered_map<std::string, ServerLoadHistory> server_histories_;
		size_t history_window_size_{100};
		std::string prediction_algorithm_{"linear_trend"};
        
		// Accuracy tracking
		std::deque<float> prediction_errors_;
		float total_predictions_{0};
		float correct_predictions_{0};
	};
}
