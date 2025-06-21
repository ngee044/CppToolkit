#pragma once

#include "../GameNetworkConstants.h"

#include <memory>
#include <unordered_map>
#include <chrono>
#include <deque>
#include <optional>
#include <tuple>
#include <atomic>
#include <future>
#include <mutex>
#include <unordered_set>

namespace GameNetwork
{
    enum class RateLimitType
    {
        TokenBucket = 0,     // Allows burst traffic
        SlidingWindow = 1,   // Smooth rate limiting
        FixedWindow = 2,     // Simple but less accurate
        LeakyBucket = 3      // Smooth output rate
    };
    
    struct RateLimitConfig
    {
        uint32_t max_requests;
        std::chrono::milliseconds time_window;
        uint32_t burst_size;  // For token bucket
        RateLimitType type;
    };
    
    class RateLimiter : public std::enable_shared_from_this<RateLimiter>
    {
    public:
        RateLimiter();
        virtual ~RateLimiter();
        
        // Rate limit checking
        auto check_rate_limit(const std::string& session_id,
                              const std::string& action = "default") -> bool;
        auto check_rate_limit_with_cost(const std::string& session_id,
                                        uint32_t cost,
                                        const std::string& action = "default") -> bool;
        
        // Configuration
        auto set_global_limit(const RateLimitConfig& config) -> void;
        auto set_session_limit(const std::string& session_id,
                               const RateLimitConfig& config) -> void;
        auto set_action_limit(const std::string& action,
                              const RateLimitConfig& config) -> void;
        auto remove_session_limit(const std::string& session_id) -> void;
        
        // Dynamic adjustment
        auto adjust_limit_by_trust_score(const std::string& session_id,
                                         float trust_score) -> void;
        auto apply_penalty(const std::string& session_id,
                           float penalty_factor,
                           std::chrono::seconds duration) -> void;
        auto remove_penalty(const std::string& session_id) -> void;
        
        // Bandwidth limiting
        auto check_bandwidth_limit(const std::string& session_id,
                                   size_t bytes) -> bool;
        auto set_bandwidth_limit(const std::string& session_id,
                                 uint64_t bytes_per_second) -> void;
        auto get_current_bandwidth_usage(const std::string& session_id) const -> uint64_t;
        
        // IP-based limiting
        auto check_ip_rate_limit(const std::string& ip_address,
                                 const std::string& action = "default") -> bool;
        auto set_ip_limit(const std::string& ip_address,
                          const RateLimitConfig& config) -> void;
        auto block_ip_temporarily(const std::string& ip_address,
                                  std::chrono::seconds duration) -> void;
        
        // Distributed rate limiting support
        auto sync_with_redis(const std::string& redis_key_prefix) -> void;
        auto get_distributed_count(const std::string& key) const -> uint32_t;
        auto increment_distributed_count(const std::string& key) -> void;
        
        // Exemptions
        auto add_exemption(const std::string& session_id) -> void;
        auto remove_exemption(const std::string& session_id) -> void;
        auto is_exempted(const std::string& session_id) const -> bool;
        
        // Monitoring
        auto get_remaining_quota(const std::string& session_id,
                                 const std::string& action = "default") const 
            -> std::tuple<uint32_t, std::chrono::milliseconds>;
        auto is_rate_limited(const std::string& session_id) const -> bool;
        auto get_rate_limited_sessions() const -> std::vector<std::string>;
        
        // Cleanup
        auto cleanup_expired_entries() -> size_t;
        auto set_cleanup_interval(std::chrono::seconds interval) -> void;
        
        // Statistics
        struct RateLimiterStats
        {
            uint64_t total_requests;
            uint64_t allowed_requests;
            uint64_t denied_requests;
            uint64_t bandwidth_limited_requests;
            std::unordered_map<std::string, uint64_t> denials_by_action;
            std::unordered_map<std::string, uint64_t> denials_by_session;
            uint64_t ip_blocks;
            uint64_t exempted_requests;
        };
        
        auto get_stats() const -> RateLimiterStats;
        auto reset_stats() -> void;
        
    private:
        struct TokenBucket
        {
            double tokens;
            uint32_t capacity;
            uint32_t refill_rate;
            std::chrono::steady_clock::time_point last_refill;
            
            auto try_consume(uint32_t tokens_requested) -> bool;
            auto refill() -> void;
        };
        
        struct SlidingWindow
        {
            std::deque<std::chrono::steady_clock::time_point> timestamps;
            uint32_t max_requests;
            std::chrono::milliseconds window_size;
            
            auto try_add_request() -> bool;
            auto cleanup() -> void;
        };
        
        struct BandwidthTracker
        {
            std::deque<std::pair<std::chrono::steady_clock::time_point, size_t>> usage;
            uint64_t bytes_per_second_limit;
            
            auto try_consume_bandwidth(size_t bytes) -> bool;
            auto get_current_usage() const -> uint64_t;
            auto cleanup() -> void;
        };
        
        struct SessionLimitData
        {
            std::unordered_map<std::string, std::unique_ptr<TokenBucket>> token_buckets;
            std::unordered_map<std::string, std::unique_ptr<SlidingWindow>> sliding_windows;
            std::unique_ptr<BandwidthTracker> bandwidth_tracker;
            RateLimitConfig config;
            float penalty_factor;
            std::chrono::steady_clock::time_point penalty_until;
        };
        
        auto create_rate_limiter(const RateLimitConfig& config) -> void*;
        auto check_limit_internal(void* limiter, 
                                  RateLimitType type,
                                  uint32_t cost) -> bool;
        
    private:
        mutable std::mutex mutex_;
        
        // Global limits
        RateLimitConfig global_config_;
        std::unique_ptr<TokenBucket> global_token_bucket_;
        std::unique_ptr<SlidingWindow> global_sliding_window_;
        
        // Per-session limits
        std::unordered_map<std::string, SessionLimitData> session_limits_;
        
        // Per-action limits
        std::unordered_map<std::string, RateLimitConfig> action_limits_;
        
        // IP-based limits
        std::unordered_map<std::string, SessionLimitData> ip_limits_;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> ip_blocks_;
        
        // Exemptions
        std::unordered_set<std::string> exempted_sessions_;
        
        // Cleanup
        std::future<void> cleanup_thread_;
        std::atomic<bool> cleanup_running_;
        std::chrono::seconds cleanup_interval_;
        
        // Statistics
        RateLimiterStats stats_;
        
        // Redis integration
        std::string redis_key_prefix_;
        bool redis_sync_enabled_;
    };
}
