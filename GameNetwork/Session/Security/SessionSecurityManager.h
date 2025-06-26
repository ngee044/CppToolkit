#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <tuple>
#include <optional>
#include <random>

namespace GameNetwork
{
    namespace Security
    {
        struct SessionToken
        {
            std::string token;
            std::string session_id;
            std::string client_ip;
            std::string device_id;
            std::chrono::steady_clock::time_point created_time;
            std::chrono::steady_clock::time_point last_validated;
            uint32_t validation_count;
        };

        struct SessionSecurityPolicy
        {
            // Token settings
            std::chrono::seconds token_lifetime{3600};  // 1 hour
            std::chrono::seconds token_refresh_interval{300};  // 5 minutes
            bool require_ip_match{true};
            bool require_device_match{true};
            
            // Session limits
            uint32_t max_concurrent_sessions_per_account{1};
            bool allow_kick_previous_session{true};
            
            // Timeout settings
            std::chrono::seconds idle_timeout{1800};  // 30 minutes
            std::chrono::seconds absolute_timeout{86400};  // 24 hours
            std::chrono::seconds grace_period{60};  // 1 minute after disconnect
            
            // Security features
            bool enable_session_hijacking_protection{true};
            uint32_t max_failed_validations{5};
            std::chrono::seconds lockout_duration{300};  // 5 minutes
        };

        class SessionSecurityManager
        {
        public:
            SessionSecurityManager();
            ~SessionSecurityManager();

            // Initialize with policy
            auto initialize(const SessionSecurityPolicy& policy) 
                -> std::tuple<bool, std::optional<std::string>>;

            // Token management
            auto generate_session_token(const std::string& session_id, 
                                       const std::string& client_ip,
                                       const std::string& device_id) 
                -> std::tuple<std::string, std::optional<std::string>>;

            auto validate_session_token(const std::string& token,
                                       const std::string& session_id,
                                       const std::string& client_ip,
                                       const std::string& device_id) 
                -> std::tuple<bool, std::optional<std::string>>;

            auto refresh_session_token(const std::string& old_token) 
                -> std::tuple<std::string, std::optional<std::string>>;

            auto revoke_session_token(const std::string& token) -> void;

            // Session hijacking prevention
            auto detect_hijacking_attempt(const std::string& session_id,
                                         const std::string& client_ip,
                                         const std::string& device_id) 
                -> bool;

            auto record_validation_failure(const std::string& session_id) -> void;
            auto is_session_locked(const std::string& session_id) const -> bool;

            // Concurrent session management
            auto check_concurrent_sessions(const std::string& account_id) 
                -> std::vector<std::string>;  // Returns active session IDs

            auto register_active_session(const std::string& account_id,
                                        const std::string& session_id) 
                -> std::tuple<bool, std::optional<std::string>>;

            auto unregister_active_session(const std::string& account_id,
                                          const std::string& session_id) -> void;

            auto get_session_count(const std::string& account_id) const -> size_t;

            // Timeout management
            auto update_session_activity(const std::string& session_id) -> void;
            
            auto check_session_timeout(const std::string& session_id) 
                -> std::tuple<bool, std::chrono::seconds>;  // Returns (is_timeout, remaining_time)

            auto get_idle_sessions(std::chrono::seconds idle_threshold) const 
                -> std::vector<std::string>;

            // Policy management
            auto update_policy(const SessionSecurityPolicy& policy) -> void;
            auto get_policy() const -> SessionSecurityPolicy;

            // Cleanup
            auto cleanup_expired_tokens() -> size_t;
            auto cleanup_inactive_sessions() -> size_t;

        private:
            auto generate_secure_token() -> std::string;
            auto hash_token(const std::string& token) const -> std::string;
            auto verify_token_constraints(const SessionToken& token,
                                         const std::string& client_ip,
                                         const std::string& device_id) const -> bool;

        private:
            mutable std::mutex mutex_;
            SessionSecurityPolicy policy_;
            
            // Token storage (hashed token -> token data)
            std::unordered_map<std::string, SessionToken> tokens_;
            
            // Session to token mapping
            std::unordered_map<std::string, std::string> session_tokens_;
            
            // Concurrent session tracking (account_id -> set of session_ids)
            std::unordered_map<std::string, std::unordered_set<std::string>> active_sessions_;
            
            // Session activity tracking
            std::unordered_map<std::string, std::chrono::steady_clock::time_point> session_activity_;
            std::unordered_map<std::string, std::chrono::steady_clock::time_point> session_start_time_;
            
            // Failed validation tracking
            std::unordered_map<std::string, uint32_t> failed_validations_;
            std::unordered_map<std::string, std::chrono::steady_clock::time_point> lockout_times_;
            
            // Random number generator for tokens
            std::mt19937_64 rng_;
            std::uniform_int_distribution<uint64_t> dist_;
        };
    }
}