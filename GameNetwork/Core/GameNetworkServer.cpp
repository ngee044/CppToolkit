#include "GameNetworkServer.h"
#include "../Packet/MessageDispatcher.h"

namespace GameNetwork
{
    GameNetworkServer::GameNetworkServer(const ServerConfig& config)
        : config_(config)
        , is_running_(false)
    {
        // TODO: Implement
    }
    
    GameNetworkServer::~GameNetworkServer()
    {
        stop();
    }
    
    auto GameNetworkServer::start() -> std::tuple<bool, std::optional<std::string>>
    {
        is_running_ = true;
        // TODO: Implement actual server start
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::stop() -> std::tuple<bool, std::optional<std::string>>
    {
        is_running_ = false;
        // TODO: Implement actual server stop
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::is_running() const -> bool
    {
        return is_running_;
    }
    
    auto GameNetworkServer::session_manager() -> std::shared_ptr<GameSessionManager>
    {
        if (!session_manager_)
        {
            session_manager_ = std::make_shared<GameSessionManager>();
        }
        return session_manager_;
    }
    
    auto GameNetworkServer::message_dispatcher() -> std::shared_ptr<MessageDispatcher>
    {
        if (!message_dispatcher_)
        {
            message_dispatcher_ = std::make_shared<MessageDispatcher>();
        }
        return message_dispatcher_;
    }
    
    auto GameNetworkServer::get_stats() const -> ServerStats
    {
        return stats_;
    }
}
