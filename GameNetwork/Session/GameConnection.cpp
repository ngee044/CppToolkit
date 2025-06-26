#include "GameConnection.h"
#include <GameSession.h>
#include <Logger.h>

namespace GameNetwork
{
    GameConnection::GameConnection(std::shared_ptr<Network::NetworkSession> network_session)
        : network_session_(network_session)
        , connection_time_(std::chrono::steady_clock::now())
    {
    }

    GameConnection::~GameConnection()
    {
        disconnect();
    }

    auto GameConnection::bind_session(std::shared_ptr<GameSession> session) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        game_session_ = session;
    }

    auto GameConnection::unbind_session() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        game_session_.reset();
    }

    auto GameConnection::get_session() const -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return game_session_.lock();
    }

    auto GameConnection::get_network_session() const -> std::shared_ptr<Network::NetworkSession>
    {
        return network_session_;
    }

    auto GameConnection::get_connection_id() const -> std::string
    {
        if (network_session_)
        {
            return network_session_->get_id();
        }
        return "";
    }

    auto GameConnection::id() const -> std::string
    {
        return get_connection_id();
    }

    auto GameConnection::get_remote_address() const -> std::string
    {
        if (network_session_)
        {
            return network_session_->get_remote_address();
        }
        return "";
    }

    auto GameConnection::get_connection_time() const -> std::chrono::steady_clock::time_point
    {
        return connection_time_;
    }

    auto GameConnection::is_connected() const -> bool
    {
        return network_session_ && network_session_->is_connected();
    }

    auto GameConnection::is_timeout() const -> bool
    {
        // Check if connection has timed out based on last activity
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - connection_time_);
        
        // Use a reasonable timeout value (e.g., 5 minutes)
        constexpr auto connection_timeout = std::chrono::seconds(300);
        return elapsed > connection_timeout;
    }

    auto GameConnection::disconnect() -> void
    {
        if (network_session_)
        {
            network_session_->disconnect();
        }
    }

    auto GameConnection::send(const std::string& data) -> bool
    {
        if (!is_connected())
        {
            return false;
        }
        
        auto [success, error] = network_session_->send_message(data);
        return success;
    }

    auto GameConnection::send_binary(const std::vector<uint8_t>& data) -> bool
    {
        if (!is_connected())
        {
            return false;
        }
        
        auto [success, error] = network_session_->send_binary(data, "");
        return success;
    }
}
