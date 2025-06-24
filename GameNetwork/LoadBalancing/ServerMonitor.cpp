#include "ServerMonitor.h"

namespace GameNetwork
{
    ServerMonitor::ServerMonitor() = default;
    ServerMonitor::~ServerMonitor() = default;
    
    auto ServerMonitor::set_server_id(const std::string& server_id) -> void
    {
        server_id_ = server_id;
    }
}
