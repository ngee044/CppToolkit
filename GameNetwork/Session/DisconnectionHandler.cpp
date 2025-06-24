#include "DisconnectionHandler.h"

namespace GameNetwork
{
    DisconnectionHandler::DisconnectionHandler() = default;
    DisconnectionHandler::~DisconnectionHandler() = default;
    
    auto DisconnectionHandler::set_session_manager(std::shared_ptr<GameSessionManager> manager) -> void
    {
        session_manager_ = manager;
    }
}
