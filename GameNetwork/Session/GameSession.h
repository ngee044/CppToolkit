#pragma once

#include "../GameNetworkConstants.h"
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
        auto id() const -> uint64_t;  // Returns hashed session ID
        auto session_id() const -> std::string;
        auto get_session_id() const -> std::string { return session_id(); }  // Alias for compatibility
        auto session_id_hash() const -> uint64_t;
        auto account_id() const -> std::string;
        auto get_account_id() const -> std::string;  // Alias for compatibility
        auto set_account_id(const std::string& id) -> void;
        
        // Connection management
        auto bind_connection(std::shared_ptr<GameConnection> connection) -> void;
        auto unbind_connection() -> void;
        auto current_connection() const -> std::shared_ptr<GameConnection>;
        auto connection() const -> std::shared_ptr<GameConnection>;  // Alias for compatibility
        auto is_online() const -> bool;
        auto is_connected() const -> bool;  // Alias for compatibility
        auto is_active() const -> bool;
        
        // State management
        auto state() const -> SessionConnectionState;
        auto set_state(SessionConnectionState new_state) -> void;
        
        // Character management
        auto load_character(uint64_t character_id) -> std::tuple<bool, std::optional<std::string>>;
        auto current_character() const -> std::shared_ptr<Character>;
        auto save_character() -> std::tuple<bool, std::optional<std::string>>;
        auto get_entity_id() const -> uint64_t;
        
        // Location management
        auto current_location() const -> Location;
        auto location() const -> Location;  // Alias for compatibility
        auto move_to(const Location& location) -> void;
        auto teleport_to(const Location& location) -> void;
        
        // Channel management
        auto enter_channel(uint32_t channel_id) -> std::tuple<bool, std::optional<std::string>>;
        auto leave_channel() -> void;
        auto current_channel_id() const -> uint32_t;
        auto get_channel_id() const -> uint32_t;  // Alias for compatibility
        
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
        
        // Security token management
        auto set_session_token(const std::string& token) -> void;
        auto get_session_token() const -> std::string;
        auto clear_session_token() -> void;
        
        // Session limits
        auto set_kicked_by_duplicate_login(bool kicked) -> void;
        auto was_kicked_by_duplicate_login() const -> bool;
        
        // Connection status
        auto get_player_location() const -> std::optional<Location>;
        
        // Packet sending
        auto send_packet(const std::vector<uint8_t>& packet_data) -> bool;
        
    private:
        auto start_grace_period_timer() -> void;
        auto stop_grace_period_timer() -> void;
        
    private:
        mutable std::mutex mutex_;
        
        // Identification
        std::string session_id_;
        std::string account_id_;
        
        // State
        SessionConnectionState state_;
        
        // Connection
        std::shared_ptr<GameConnection> connection_;
        
        // Game data
        std::shared_ptr<Character> character_;
        Location current_location_;
        
        // Timing
        std::chrono::steady_clock::time_point created_time_;
        std::chrono::steady_clock::time_point last_activity_;
        std::chrono::steady_clock::time_point disconnected_time_;
        
        // Channel
        uint32_t channel_id_;
        
        // Session data storage
        std::unordered_map<std::string, std::any> custom_data_;
        
        // Security
        std::string session_token_;
        bool kicked_by_duplicate_login_;
        
        // Grace period timer
        std::future<void> grace_timer_;
    };
}
