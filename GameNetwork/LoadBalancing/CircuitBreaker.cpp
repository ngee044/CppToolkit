#include "CircuitBreaker.h"

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <Logger.h>
#include <algorithm>

using namespace Utilities;

namespace GameNetwork
{
	namespace LoadBalancing
	{
		CircuitBreaker::CircuitBreaker(const std::string& name, const CircuitBreakerConfig& config)
			: name_(name)
			, config_(config)
			, state_change_time_(std::chrono::steady_clock::now())
		{
			Logger::handle().write(LogTypes::Information, fmt::format("Circuit breaker created: {}", name_));
		}
        
		CircuitBreaker::~CircuitBreaker() = default;
        
		auto CircuitBreaker::allow_request() -> bool
		{
			auto current_state = state_.load();
            
			switch (current_state)
			{
				case CircuitState::Closed:
					return true;
                    
				case CircuitState::Open:
					{
						// Check if we should transition to half-open
						if (should_attempt_reset())
						{
							transition_to(CircuitState::HalfOpen);
							return true;
						}
						return false;
					}
                    
				case CircuitState::HalfOpen:
					// Allow limited requests in half-open state
					return true;
                    
				default:
					return false;
			}
		}
        
		auto CircuitBreaker::record_success(std::chrono::milliseconds duration) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			// Add to request window
			request_window_.push_back({true, duration, std::chrono::steady_clock::now()});
			cleanup_old_records();
            
			stats_.successful_requests++;
			stats_.total_requests++;
            
			consecutive_failures_ = 0;
			consecutive_successes_++;
            
			auto current_state = state_.load();
            
			// Check state transitions
			if (current_state == CircuitState::HalfOpen)
			{
				if (consecutive_successes_ >= config_.success_threshold)
				{
					transition_to(CircuitState::Closed);
				}
			}
            
			update_statistics();
		}
        
		auto CircuitBreaker::record_failure(std::chrono::milliseconds duration, const std::string& error) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			// Add to request window
			request_window_.push_back({false, duration, std::chrono::steady_clock::now()});
			cleanup_old_records();
            
			stats_.failed_requests++;
			stats_.total_requests++;
			last_failure_time_ = std::chrono::steady_clock::now();
            
			consecutive_successes_ = 0;
			consecutive_failures_++;
            
			Logger::handle().write(LogTypes::Error, fmt::format("Circuit breaker {} recorded failure: {}", name_, error));
            
			// Check if we should open the circuit
			if (should_trip())
			{
				transition_to(CircuitState::Open);
			}
			else if (state_ == CircuitState::HalfOpen)
			{
				// Single failure in half-open state reopens the circuit
				transition_to(CircuitState::Open);
			}
            
			update_statistics();
		}
        
		auto CircuitBreaker::record_rejected() -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
			stats_.rejected_requests++;
		}
        
		auto CircuitBreaker::should_trip() -> bool
		{
			if (state_ != CircuitState::Closed)
			{
				return false;
			}
            
			// Check consecutive failures
			if (consecutive_failures_ >= config_.failure_threshold)
			{
				return true;
			}
            
			// Check failure rate
			if (request_window_.size() >= config_.min_requests)
			{
				uint32_t failures = 0;
				uint32_t slow_calls = 0;
                
				for (const auto& record : request_window_)
				{
					if (!record.success) failures++;
					if (record.duration >= config_.slow_call_duration) slow_calls++;
				}
                
				float failure_rate = static_cast<float>(failures) / request_window_.size();
				float slow_call_rate = static_cast<float>(slow_calls) / request_window_.size();
                
				if (failure_rate >= config_.failure_rate_threshold ||
					slow_call_rate >= config_.slow_call_rate_threshold)
				{
					return true;
				}
			}
            
			return false;
		}
        
		auto CircuitBreaker::should_attempt_reset() -> bool
		{
			if (state_ != CircuitState::Open)
			{
				return false;
			}
            
			auto now = std::chrono::steady_clock::now();
			auto time_since_open = std::chrono::duration_cast<std::chrono::seconds>(
				now - state_change_time_);
            
			return time_since_open >= config_.timeout;
		}
        
		auto CircuitBreaker::transition_to(CircuitState new_state) -> void
		{
			auto old_state = state_.exchange(new_state);
            
			if (old_state != new_state)
			{
				state_change_time_ = std::chrono::steady_clock::now();
				consecutive_successes_ = 0;
				consecutive_failures_ = 0;
                
				Logger::handle().write(LogTypes::Information, fmt::format("Circuit breaker {} state changed: {} -> {}", name_, get_state_name(), get_state_name()));
                
				if (state_change_callback_)
				{
					state_change_callback_(name_, old_state, new_state);
				}
			}
		}
        
		auto CircuitBreaker::update_statistics() -> void
		{
			stats_.current_state = state_.load();
			stats_.last_state_change = state_change_time_;
            
			if (!request_window_.empty())
			{
				uint32_t failures = 0;
				uint32_t slow_calls = 0;
                
				for (const auto& record : request_window_)
				{
					if (!record.success) failures++;
					if (record.duration >= config_.slow_call_duration) slow_calls++;
				}
                
				stats_.failure_rate = static_cast<float>(failures) / request_window_.size();
				stats_.slow_call_rate = static_cast<float>(slow_calls) / request_window_.size();
				stats_.slow_requests = slow_calls;
			}
			else
			{
				stats_.failure_rate = 0.0f;
				stats_.slow_call_rate = 0.0f;
			}
		}
        
		auto CircuitBreaker::cleanup_old_records() -> void
		{
			auto now = std::chrono::steady_clock::now();
            
			while (!request_window_.empty())
			{
				auto age = std::chrono::duration_cast<std::chrono::seconds>(
					now - request_window_.front().timestamp);
                
				if (age > config_.window_size)
				{
					request_window_.pop_front();
				}
				else
				{
					break;
				}
			}
		}
        
		auto CircuitBreaker::trip() -> void
		{
			transition_to(CircuitState::Open);
		}
        
		auto CircuitBreaker::reset() -> void
		{
			transition_to(CircuitState::Closed);
		}
        
		auto CircuitBreaker::test() -> void
		{
			transition_to(CircuitState::HalfOpen);
		}
        
		auto CircuitBreaker::get_state_name() const -> std::string
		{
			switch (state_.load())
			{
				case CircuitState::Closed: return "Closed";
				case CircuitState::Open: return "Open";
				case CircuitState::HalfOpen: return "HalfOpen";
				default: return "Unknown";
			}
		}
        
		auto CircuitBreaker::get_stats() const -> CircuitBreakerStats
		{
			std::lock_guard<std::mutex> lock(mutex_);
			return stats_;
		}
        
		auto CircuitBreaker::reset_stats() -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
			stats_ = {};
			stats_.current_state = state_.load();
			stats_.last_state_change = state_change_time_;
			request_window_.clear();
		}
        
		// Circuit Breaker Manager Implementation
		CircuitBreakerManager::CircuitBreakerManager() = default;
		CircuitBreakerManager::~CircuitBreakerManager() = default;
        
		auto CircuitBreakerManager::get_circuit_breaker(const std::string& service_name, const CircuitBreakerConfig& config) 
			-> std::shared_ptr<CircuitBreaker>
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			auto it = circuit_breakers_.find(service_name);
			if (it != circuit_breakers_.end())
			{
				return it->second;
			}
            
			// Create new circuit breaker
			auto cb = std::make_shared<CircuitBreaker>(service_name, config);
            
			// Set up state change callback
			if (global_callback_)
			{
				cb->on_state_change(global_callback_);
			}
            
			circuit_breakers_[service_name] = cb;
			return cb;
		}
        
		auto CircuitBreakerManager::remove_circuit_breaker(const std::string& service_name) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
			circuit_breakers_.erase(service_name);
		}
        
		auto CircuitBreakerManager::get_all_circuit_breakers() const 
			-> std::vector<std::pair<std::string, std::shared_ptr<CircuitBreaker>>>
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			std::vector<std::pair<std::string, std::shared_ptr<CircuitBreaker>>> result;
			for (const auto& [name, cb] : circuit_breakers_)
			{
				result.emplace_back(name, cb);
			}
            
			return result;
		}
        
		auto CircuitBreakerManager::on_any_state_change(GlobalStateChangeCallback callback) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			global_callback_ = callback;
            
			// Update existing circuit breakers
			for (auto& [name, cb] : circuit_breakers_)
			{
				cb->on_state_change(callback);
			}
		}
        
		auto CircuitBreakerManager::trip_all() -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			for (auto& [name, cb] : circuit_breakers_)
			{
				cb->trip();
			}
		}
        
		auto CircuitBreakerManager::reset_all() -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			for (auto& [name, cb] : circuit_breakers_)
			{
				cb->reset();
			}
		}
        
	} // namespace LoadBalancing
} // namespace GameNetwork
