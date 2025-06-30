#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <functional>
#include <optional>
#include <deque>

namespace GameNetwork
{
	namespace LoadBalancing
	{
		// Circuit breaker states
		enum class CircuitState
		{
			Closed,      // Normal operation
			Open,        // Failing, reject all requests
			HalfOpen     // Testing if service recovered
		};
		
		// Circuit breaker configuration
		struct CircuitBreakerConfig
		{
			uint32_t failure_threshold{5};                    // Failures to open circuit
			float failure_rate_threshold{0.5f};               // Failure rate to open circuit
			std::chrono::seconds timeout{60};                 // Time before trying half-open
			uint32_t success_threshold{3};                    // Successes to close from half-open
			std::chrono::seconds window_size{60};             // Rolling window for stats
			uint32_t min_requests{10};                        // Min requests before calculating rate
			std::chrono::milliseconds slow_call_duration{3000}; // Threshold for slow calls
			float slow_call_rate_threshold{0.5f};            // Slow call rate to open circuit
		};
		
		// Request result for circuit breaker
		struct RequestResult
		{
			bool success;
			std::chrono::milliseconds duration;
			std::optional<std::string> error_message;
			std::chrono::steady_clock::time_point timestamp;
		};
		
		// Circuit breaker statistics
		struct CircuitBreakerStats
		{
			uint32_t total_requests{0};
			uint32_t failed_requests{0};
			uint32_t successful_requests{0};
			uint32_t rejected_requests{0};
			uint32_t slow_requests{0};
			float failure_rate{0.0f};
			float slow_call_rate{0.0f};
			std::chrono::steady_clock::time_point last_failure_time;
			std::chrono::steady_clock::time_point last_state_change;
			CircuitState current_state{CircuitState::Closed};
		};
		
		// Circuit breaker implementation
		class CircuitBreaker
		{
		public:
			CircuitBreaker(const std::string& name, const CircuitBreakerConfig& config = {});
			~CircuitBreaker();
			
			// Execute function with circuit breaker protection
			template<typename Func>
			auto execute(Func&& func) -> std::optional<typename std::invoke_result_t<Func>>
			{
				if (!allow_request())
				{
					record_rejected();
					return std::nullopt;
				}
				
				auto start_time = std::chrono::steady_clock::now();
				
				try
				{
					auto result = func();
					
					auto end_time = std::chrono::steady_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
						end_time - start_time);
					
					record_success(duration);
					return result;
				}
				catch (const std::exception& e)
				{
					auto end_time = std::chrono::steady_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
						end_time - start_time);
					
					record_failure(duration, e.what());
					return std::nullopt;
				}
			}
			
			// Manual state management
			auto trip() -> void;  // Force open
			auto reset() -> void; // Force closed
			auto test() -> void;  // Force half-open
			
			// Get current state
			auto get_state() const -> CircuitState { return state_.load(); }
			auto get_state_name() const -> std::string;
			
			// Get statistics
			auto get_stats() const -> CircuitBreakerStats;
			auto reset_stats() -> void;
			
			// Configuration
			auto set_config(const CircuitBreakerConfig& config) -> void;
			auto get_config() const -> const CircuitBreakerConfig& { return config_; }
			
			// Callbacks
			using StateChangeCallback = std::function<void(const std::string&, CircuitState, CircuitState)>;
			auto on_state_change(StateChangeCallback callback) -> void { state_change_callback_ = callback; }
			
		private:
			std::string name_;
			CircuitBreakerConfig config_;
			
			std::atomic<CircuitState> state_{CircuitState::Closed};
			std::atomic<uint32_t> consecutive_successes_{0};
			std::atomic<uint32_t> consecutive_failures_{0};
			
			// Rolling window for request tracking
			struct RequestRecord
			{
				bool success;
				std::chrono::milliseconds duration;
				std::chrono::steady_clock::time_point timestamp;
			};
			
			mutable std::mutex mutex_;
			std::deque<RequestRecord> request_window_;
			std::chrono::steady_clock::time_point last_failure_time_;
			std::chrono::steady_clock::time_point state_change_time_;
			
			CircuitBreakerStats stats_;
			StateChangeCallback state_change_callback_;
			
			// Internal methods
			auto allow_request() -> bool;
			auto record_success(std::chrono::milliseconds duration) -> void;
			auto record_failure(std::chrono::milliseconds duration, const std::string& error) -> void;
			auto record_rejected() -> void;
			
			auto should_trip() -> bool;
			auto should_attempt_reset() -> bool;
			auto transition_to(CircuitState new_state) -> void;
			
			auto update_statistics() -> void;
			auto cleanup_old_records() -> void;
		};
		
		// Circuit breaker manager for multiple services
		class CircuitBreakerManager
		{
		public:
			CircuitBreakerManager();
			~CircuitBreakerManager();
			
			// Get or create circuit breaker for a service
			auto get_circuit_breaker(const std::string& service_name, 
									const CircuitBreakerConfig& config = {}) 
				-> std::shared_ptr<CircuitBreaker>;
			
			// Remove circuit breaker
			auto remove_circuit_breaker(const std::string& service_name) -> void;
			
			// Get all circuit breakers
			auto get_all_circuit_breakers() const 
				-> std::vector<std::pair<std::string, std::shared_ptr<CircuitBreaker>>>;
			
			// Global state change callback
			using GlobalStateChangeCallback = std::function<void(const std::string&, CircuitState, CircuitState)>;
			auto on_any_state_change(GlobalStateChangeCallback callback) -> void;
			
			// Trip all circuit breakers (emergency)
			auto trip_all() -> void;
			auto reset_all() -> void;
			
		private:
			mutable std::mutex mutex_;
			std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> circuit_breakers_;
			GlobalStateChangeCallback global_callback_;
		};
		
	}
}
