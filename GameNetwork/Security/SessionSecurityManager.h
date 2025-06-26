#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace GameNetwork::Security
{
    struct SessionSecurityPolicy
    {
        uint32_t max_concurrent_sessions_per_account = 1;
        bool allow_kick_previous_session = true;
        uint32_t session_timeout_minutes = 60;
        uint32_t idle_timeout = 30;
        uint32_t token_refresh_interval_minutes = 30;
        bool enable_ip_validation = true;
        bool enable_device_validation = true;
        uint32_t max_failed_attempts = 5;
        uint32_t lockout_duration_minutes = 15;
    };

    class SessionSecurityManager
    {
    public:
        SessionSecurityManager() = default;
        ~SessionSecurityManager() = default;

        // Initialization
        auto initialize(const SessionSecurityPolicy& policy) -> std::tuple<bool, std::optional<std::string>>;
        auto shutdown() -> void;

        // Session management
        auto check_concurrent_sessions(const std::string& account_id) -> std::vector<std::string>;
        auto register_active_session(const std::string& account_id, const std::string& session_id) -> std::tuple<bool, std::optional<std::string>>;
        auto unregister_session(const std::string& session_id) -> void;

        // Token management
        auto generate_session_token(const std::string& session_id, const std::string& client_ip, const std::string& device_id) -> std::tuple<std::string, std::optional<std::string>>;
        auto validate_session_token(const std::string& token, const std::string& session_id, const std::string& client_ip, const std::string& device_id) -> std::tuple<bool, std::optional<std::string>>;
        auto refresh_session_token(const std::string& old_token) -> std::tuple<std::string, std::optional<std::string>>;

        // Security checks
        auto is_session_locked(const std::string& session_id) -> bool;
        auto check_session_timeout(const std::string& session_id) -> std::tuple<bool, uint32_t>;
        auto lock_session(const std::string& session_id, const std::string& reason) -> void;
        auto unlock_session(const std::string& session_id) -> void;

        // Policy access
        auto get_policy() const -> const SessionSecurityPolicy&;
        auto update_policy(const SessionSecurityPolicy& policy) -> void;

        // Cleanup operations
        auto cleanup_expired_tokens() -> uint32_t;
        auto cleanup_inactive_sessions() -> uint32_t;
        auto get_idle_sessions(uint32_t timeout_minutes) -> std::vector<std::string>;

    private:
        struct SessionInfo
        {
            std::string session_id;
            std::string account_id;
            std::string client_ip;
            std::string device_id;
            std::string current_token;
            std::chrono::steady_clock::time_point created_time;
            std::chrono::steady_clock::time_point last_activity;
            bool is_locked = false;
            std::string lock_reason;
            uint32_t failed_attempts = 0;
        };

        SessionSecurityPolicy policy_;
        std::unordered_map<std::string, SessionInfo> active_sessions_;       // session_id -> SessionInfo
        std::unordered_map<std::string, std::vector<std::string>> sessions_by_account_; // account_id -> [session_ids]
        std::unordered_map<std::string, std::string> token_to_session_;      // token -> session_id
        
        mutable std::mutex mutex_;
        bool is_initialized_ = false;

        // Helper methods
        auto generate_token() -> std::string;
        auto is_token_valid(const std::string& token, const SessionInfo& session) -> bool;
        auto cleanup_expired_sessions() -> void;
    };
}
