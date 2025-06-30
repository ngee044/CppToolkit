#include "RateLimiter.h"

#include <algorithm>

namespace GameNetwork
{
	RateLimiter::RateLimiter()
		: cleanup_running_(false)
		, redis_sync_enabled_(false)
	{
		// Initialize default global config
		global_config_ = {
			100,  // 100 requests
			std::chrono::milliseconds(1000),  // per second
			150,  // burst size
			RateLimitType::TokenBucket
		};
        
		// Create global limiter
		global_token_bucket_ = std::make_unique<TokenBucket>();
		global_token_bucket_->capacity = global_config_.burst_size;
		global_token_bucket_->tokens = global_config_.burst_size;
		global_token_bucket_->refill_rate = global_config_.max_requests;
		global_token_bucket_->last_refill = std::chrono::steady_clock::now();
        
		// Initialize stats
		stats_ = {};
        
		// Start cleanup thread
		cleanup_interval_ = std::chrono::seconds(60);
		cleanup_running_ = true;
		cleanup_thread_ = std::async(std::launch::async, [this]()
		{
			while (cleanup_running_)
			{
				std::this_thread::sleep_for(cleanup_interval_);
				cleanup_expired_entries();
			}
		});
	}
    
	RateLimiter::~RateLimiter()
	{
		cleanup_running_ = false;
		if (cleanup_thread_.valid())
		{
			cleanup_thread_.wait();
		}
	}
    
	auto RateLimiter::check_rate_limit(const std::string& session_id,
									   const std::string& action) -> bool
	{
		return check_rate_limit_with_cost(session_id, 1, action);
	}
    
	auto RateLimiter::check_rate_limit_with_cost(const std::string& session_id,
												  uint32_t cost,
												  const std::string& action) -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		stats_.total_requests++;
        
		// Check if exempted
		if (exempted_sessions_.find(session_id) != exempted_sessions_.end())
		{
			stats_.exempted_requests++;
			stats_.allowed_requests++;
			return true;
		}
        
		// Check session-specific limits first
		auto session_it = session_limits_.find(session_id);
		if (session_it != session_limits_.end())
		{
			// Check penalty
			if (session_it->second.penalty_factor > 0.0f &&
				std::chrono::steady_clock::now() < session_it->second.penalty_until)
			{
				cost = static_cast<uint32_t>(cost * session_it->second.penalty_factor);
			}
            
			bool allowed = false;
            
			// Check action-specific limit
			auto action_it = session_it->second.token_buckets.find(action);
			if (action_it != session_it->second.token_buckets.end())
			{
				allowed = action_it->second->try_consume(cost);
			}
			else
			{
				// Use default session limit
				auto default_it = session_it->second.token_buckets.find("default");
				if (default_it != session_it->second.token_buckets.end())
				{
					allowed = default_it->second->try_consume(cost);
				}
			}
            
			if (!allowed)
			{
				stats_.denied_requests++;
				stats_.denials_by_action[action]++;
				stats_.denials_by_session[session_id]++;
				return false;
			}
		}
        
		// Check action-specific limits
		auto action_it = action_limits_.find(action);
		if (action_it != action_limits_.end())
		{
			// Create limiter if not exists
			// (Implementation simplified for brevity)
		}
        
		// Check global limit
		if (global_token_bucket_ && !global_token_bucket_->try_consume(cost))
		{
			stats_.denied_requests++;
			stats_.denials_by_action[action]++;
			return false;
		}
        
		stats_.allowed_requests++;
		return true;
	}
    
	auto RateLimiter::TokenBucket::try_consume(uint32_t tokens_requested) -> bool
	{
		refill();
        
		if (tokens >= tokens_requested)
		{
			tokens -= tokens_requested;
			return true;
		}
        
		return false;
	}
    
	auto RateLimiter::TokenBucket::refill() -> void
	{
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refill);
        
		if (elapsed.count() > 0)
		{
			double tokens_to_add = (elapsed.count() / 1000.0) * refill_rate;
			tokens = std::min(static_cast<double>(capacity), tokens + tokens_to_add);
			last_refill = now;
		}
	}
    
	auto RateLimiter::SlidingWindow::try_add_request() -> bool
	{
		cleanup();
        
		if (timestamps.size() < max_requests)
		{
			timestamps.push_back(std::chrono::steady_clock::now());
			return true;
		}
        
		return false;
	}
    
	auto RateLimiter::SlidingWindow::cleanup() -> void
	{
		auto now = std::chrono::steady_clock::now();
		auto cutoff = now - window_size;
        
		timestamps.erase(
			std::remove_if(timestamps.begin(), timestamps.end(),
						   [cutoff](const auto& timestamp)
						   {
							   return timestamp < cutoff;
						   }),
			timestamps.end());
	}
    
	auto RateLimiter::set_global_limit(const RateLimitConfig& config) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		global_config_ = config;
        
		// Recreate global limiter based on type
		if (config.type == RateLimitType::TokenBucket)
		{
			global_token_bucket_ = std::make_unique<TokenBucket>();
			global_token_bucket_->capacity = config.burst_size;
			global_token_bucket_->tokens = config.burst_size;
			global_token_bucket_->refill_rate = config.max_requests;
			global_token_bucket_->last_refill = std::chrono::steady_clock::now();
		}
		else if (config.type == RateLimitType::SlidingWindow)
		{
			global_sliding_window_ = std::make_unique<SlidingWindow>();
			global_sliding_window_->max_requests = config.max_requests;
			global_sliding_window_->window_size = config.time_window;
		}
	}
    
	auto RateLimiter::set_session_limit(const std::string& session_id,
										const RateLimitConfig& config) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto& session_data = session_limits_[session_id];
		session_data.config = config;
        
		// Create default limiter
		if (config.type == RateLimitType::TokenBucket)
		{
			auto bucket = std::make_unique<TokenBucket>();
			bucket->capacity = config.burst_size;
			bucket->tokens = config.burst_size;
			bucket->refill_rate = config.max_requests;
			bucket->last_refill = std::chrono::steady_clock::now();
			session_data.token_buckets["default"] = std::move(bucket);
		}
		else if (config.type == RateLimitType::SlidingWindow)
		{
			auto window = std::make_unique<SlidingWindow>();
			window->max_requests = config.max_requests;
			window->window_size = config.time_window;
			session_data.sliding_windows["default"] = std::move(window);
		}
	}
    
	auto RateLimiter::check_bandwidth_limit(const std::string& session_id,
											size_t bytes) -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = session_limits_.find(session_id);
		if (it != session_limits_.end() && it->second.bandwidth_tracker)
		{
			bool allowed = it->second.bandwidth_tracker->try_consume_bandwidth(bytes);
			if (!allowed)
			{
				stats_.bandwidth_limited_requests++;
			}
			return allowed;
		}
        
		return true;
	}
    
	auto RateLimiter::BandwidthTracker::try_consume_bandwidth(size_t bytes) -> bool
	{
		cleanup();
        
		uint64_t current_usage = get_current_usage();
		if (current_usage + bytes > bytes_per_second_limit)
		{
			return false;
		}
        
		usage.push_back({std::chrono::steady_clock::now(), bytes});
		return true;
	}
    
	auto RateLimiter::BandwidthTracker::get_current_usage() const -> uint64_t
	{
		uint64_t total = 0;
		for (const auto& [timestamp, bytes] : usage)
		{
			total += bytes;
		}
		return total;
	}
    
	auto RateLimiter::BandwidthTracker::cleanup() -> void
	{
		auto now = std::chrono::steady_clock::now();
		auto cutoff = now - std::chrono::seconds(1);
        
		usage.erase(
			std::remove_if(usage.begin(), usage.end(),
						   [cutoff](const auto& entry)
						   {
							   return entry.first < cutoff;
						   }),
			usage.end());
	}
    
	auto RateLimiter::add_exemption(const std::string& session_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		exempted_sessions_.insert(session_id);
	}
    
	auto RateLimiter::remove_exemption(const std::string& session_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		exempted_sessions_.erase(session_id);
	}
    
	auto RateLimiter::is_exempted(const std::string& session_id) const -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return exempted_sessions_.find(session_id) != exempted_sessions_.end();
	}
    
	auto RateLimiter::cleanup_expired_entries() -> size_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		size_t cleaned = 0;
        
		// Clean up IP blocks
		auto now = std::chrono::steady_clock::now();
		for (auto it = ip_blocks_.begin(); it != ip_blocks_.end();)
		{
			if (now > it->second)
			{
				it = ip_blocks_.erase(it);
				cleaned++;
			}
			else
			{
				++it;
			}
		}
        
		// Clean up inactive sessions (simplified)
		// In production, would check last activity time
        
		return cleaned;
	}
    
	auto RateLimiter::get_stats() const -> RateLimiterStats
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return stats_;
	}
    
	auto RateLimiter::reset_stats() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stats_ = {};
	}
}