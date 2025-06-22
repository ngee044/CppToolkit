#pragma once

#include "../GameNetworkConstants.h"
#include "../../Network/NetworkSession.h"

#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <chrono>
#include <optional>
#include <tuple>
#include <future>
#include <atomic>

namespace GameNetwork
{
    class GamePacket;
    
    class GameConnection : public std::enable_shared_from_this<GameConnection>
    {
    public:
        GameConnection(const std::string& connection_id);
        virtual ~GameConnection();
        
        // Network session management
        auto attach_network_session(std::shared_ptr<Network::NetworkSession> session) -> void;
        auto detach_network_session() -> void;
        auto is_connected() const -> bool;
        
        // Connection info
        auto connection_id() const -> std::string;
        auto session_id() const -> uint64_t;  // Returns session ID if bound to a session
        auto account_id() const -> std::string;
        auto set_account_id(const std::string& account_id) -> void;
        
        // State management
        auto state() const -> ConnectionState;
        auto set_state(ConnectionState new_state) -> void;
        
        // Authentication
        auto authenticate(const std::string& account_id, const std::string& session_token) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto is_authenticated() const -> bool;
        
        // Packet handling
        auto send_packet(const GamePacket& packet, PacketPriority priority = PacketPriority::Normal) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto register_packet_handler(std::function<void(const GamePacket&)> handler) -> void;
        
        // Heartbeat
        auto update_last_activity() -> void;
        auto last_activity_time() const -> std::chrono::steady_clock::time_point;
        auto is_timeout() const -> bool;
        
        // Reconnection
        auto enable_auto_reconnect(bool enable) -> void;
        auto is_auto_reconnect_enabled() const -> bool;
        auto attempt_reconnect() -> std::tuple<bool, std::optional<std::string>>;
        auto reset_reconnect_attempts() -> void;
        auto get_reconnect_attempts() const -> uint32_t;
        
        // Error handling
        auto on_connection_lost(const std::string& reason) -> void;
        auto register_connection_lost_handler(std::function<void(const std::string&)> handler) -> void;
        auto register_reconnect_success_handler(std::function<void()> handler) -> void;
        
        // Statistics
        auto bytes_sent() const -> uint64_t;
        auto bytes_received() const -> uint64_t;
        auto packets_sent() const -> uint64_t;
        auto packets_received() const -> uint64_t;
        
    private:
        auto setup_network_callbacks() -> void;
        auto on_network_message(const std::string& message) -> std::tuple<bool, std::optional<std::string>>;
        auto on_network_binary(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
        auto schedule_reconnect() -> void;
        auto cancel_reconnect() -> void;
        
    private:
        mutable std::mutex mutex_;
        
        std::string connection_id_;
        std::string account_id_;
        std::string session_token_;
        ConnectionState state_;
        
        std::shared_ptr<Network::NetworkSession> network_session_;
        std::function<void(const GamePacket&)> packet_handler_;
        
        std::chrono::steady_clock::time_point last_activity_;
        
        // Reconnection
        bool auto_reconnect_enabled_;
        uint32_t reconnect_attempts_;
        std::chrono::milliseconds current_reconnect_delay_;
        std::future<void> reconnect_timer_;
        std::atomic<bool> reconnect_scheduled_;
        
        // Error handlers
        std::function<void(const std::string&)> connection_lost_handler_;
        std::function<void()> reconnect_success_handler_;
        
        // Network info for reconnection
        std::string last_server_address_;
        uint16_t last_server_port_;
        
        // Statistics
        uint64_t bytes_sent_;
        uint64_t bytes_received_;
        uint64_t packets_sent_;
        uint64_t packets_received_;
    };
}
