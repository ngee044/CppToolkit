#pragma once

#include <memory>

namespace GameNetwork
{
    class GameSessionManager;
    
    class DisconnectionHandler
    {
    public:
        DisconnectionHandler();
        ~DisconnectionHandler();
        
        auto set_session_manager(std::shared_ptr<GameSessionManager> manager) -> void;
        
    private:
        std::weak_ptr<GameSessionManager> session_manager_;
    };
}
