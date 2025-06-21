#pragma once

#include "../GameNetworkConstants.h"
#include "../Network/NetworkSession.h"

#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <chrono>
#include <optional>
#include <tuple>

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
        
        // Statistics
        auto bytes_sent() const -> uint64_t;
        auto bytes_received() const -> uint64_t;
        auto packets_sent() const -> uint64_t;
        auto packets_received() const -> uint64_t;
        
    private:
        auto setup_network_callbacks() -> void;
        auto on_network_message(const std::string& message) -> std::tuple<bool, std::optional<std::string>>;
        auto on_network_binary(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
        
    private:
        mutable std::mutex mutex_;
        
        std::string connection_id_;
        std::string account_id_;
        std::string session_token_;
        ConnectionState state_;
        
        std::shared_ptr<Network::NetworkSession> network_session_;
        std::function<void(const GamePacket&)> packet_handler_;
        
        std::chrono::steady_clock::time_point last_activity_;
        
        // Statistics
        uint64_t bytes_sent_;
        uint64_t bytes_received_;
        uint64_t packets_sent_;
        uint64_t packets_received_;
    };
}
