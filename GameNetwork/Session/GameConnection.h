#pragma once

#include "../GameNetworkConstants.h"
#include "../../Network/NetworkSession.h"
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <vector>

namespace GameNetwork
{
    class GameSession;
    
    class GameConnection : public std::enable_shared_from_this<GameConnection>
    {
    public:
        GameConnection(std::shared_ptr<Network::NetworkSession> network_session);
        virtual ~GameConnection();
        
        // Session binding
        auto bind_session(std::shared_ptr<GameSession> session) -> void;
        auto unbind_session() -> void;
        auto get_session() const -> std::shared_ptr<GameSession>;
        
        // Network session
        auto get_network_session() const -> std::shared_ptr<Network::NetworkSession>;
        
        // Connection info
        auto get_connection_id() const -> std::string;
        auto get_remote_address() const -> std::string;
        auto get_connection_time() const -> std::chrono::steady_clock::time_point;
        
        // Connection state
        auto is_connected() const -> bool;
        auto is_timeout() const -> bool;
        auto disconnect() -> void;
        
        // Send data
        auto send(const std::string& data) -> bool;
        auto send_binary(const std::vector<uint8_t>& data) -> bool;
        
    private:
        std::shared_ptr<Network::NetworkSession> network_session_;
        std::weak_ptr<GameSession> game_session_;
        std::chrono::steady_clock::time_point connection_time_;
        mutable std::mutex mutex_;
    };
}
