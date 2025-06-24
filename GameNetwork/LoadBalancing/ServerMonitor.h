#pragma once

#include <string>
#include <memory>

namespace GameNetwork
{
    class ServerMonitor
    {
    public:
        ServerMonitor();
        ~ServerMonitor();
        
        auto set_server_id(const std::string& server_id) -> void;
        
    private:
        std::string server_id_;
    };
}
