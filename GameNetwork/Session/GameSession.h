#pragma once

#include "GameNetworkConstants.h"
#include "GameConnection.h"

#include <memory>
#include <string>
#include <mutex>
#include <chrono>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <any>
#include <future>

namespace GameNetwork
{
    class Character;
    
    class GameSession : public std::enable_shared_from_this<GameSession>
    {
    public:
        GameSession(const std::string& session_id, const std::string& account_id);
        virtual ~GameSession();
        
        // Session identification
        auto session_id() const -> std::string;
        auto account_id() const -> std::string;
        
        // Connection management
        auto bind_connection(std::shared_ptr<GameConnection> connection) -> void;
        auto unbind_connection() -> void;
        auto current_connection() const -> std::shared_ptr<GameConnection>;
        auto is_online() const -> bool;
        
        // State management
        auto state() const -> SessionState;
        auto set_state(SessionState new_state) -> void;
        
        // Character management
        auto load_character(uint64_t character_id) -> std::tuple<bool, std::optional<std::string>>;
        auto current_character() const -> std::shared_ptr<Character>;
        auto save_character() -> std::tuple<bool, std::optional<std::string>>;
        
        // Location management
        auto current_location() const -> Location;
        auto move_to(const Location& location) -> void;
        auto teleport_to(const Location& location) -> void;
        
        // Channel management
        auto enter_channel(uint32_t channel_id) -> std::tuple<bool, std::optional<std::string>>;
        auto leave_channel() -> void;
        auto current_channel_id() const -> uint32_t;
        
        // Session persistence
        auto save_state() -> std::tuple<bool, std::optional<std::string>>;
        auto restore_state() -> std::tuple<bool, std::optional<std::string>>;
        
        // Timeout management
        auto update_last_activity() -> void;
        auto last_activity_time() const -> std::chrono::steady_clock::time_point;
        auto is_timeout() const -> bool;
        auto remaining_grace_period() const -> std::chrono::seconds;
        
        // Session data
        template<typename T>
        auto set_data(const std::string& key, const T& value) -> void;
        
        template<typename T>
        auto get_data(const std::string& key) const -> std::optional<T>;
        
        auto remove_data(const std::string& key) -> void;
        
    private:
        auto start_grace_period_timer() -> void;
        auto stop_grace_period_timer() -> void;
        
    private:
        mutable std::mutex mutex_;
        
        // Identification
        std::string session_id_;
        std::string account_id_;
        
        // State
        SessionState state_;
        
        // Connection
        std::shared_ptr<GameConnection> connection_;
        
        // Game data
        std::shared_ptr<Character> character_;
        Location current_location_;
        
        // Timing
        std::chrono::steady_clock::time_point created_time_;
        std::chrono::steady_clock::time_point last_activity_;
        std::chrono::steady_clock::time_point disconnected_time_;
        
        // Session data storage
        std::unordered_map<std::string, std::any> session_data_;
        
        // Grace period timer
        std::future<void> grace_period_timer_;
    };
}
