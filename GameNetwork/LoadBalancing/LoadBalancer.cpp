#include "LoadBalancer.h"
#include <Logger.h>

using namespace Utilities;

namespace GameNetwork
{
    LoadBalancer::LoadBalancer() = default;
    LoadBalancer::~LoadBalancer() = default;
    
    auto LoadBalancer::set_server_id(const std::string& server_id) -> void
    {
        server_id_ = server_id;
    }
    
    auto LoadBalancer::get_server_list() -> std::vector<ServerInfo>
    {
        std::vector<ServerInfo> server_list;
        for (const auto& [id, server] : servers_)
        {
            server_list.push_back(server);
        }
        return server_list;
    }
    
    auto LoadBalancer::register_server(const ServerInfo& server) -> bool
    {
        try {
            servers_[server.server_id] = server;
            Logger::handle().write(Utilities::LogTypes::Information, "Server registered: " + server.server_id);
            return true;
        }
        catch (const std::exception& e) {
            Logger::handle().write(Utilities::LogTypes::Error, "Failed to register server: " + std::string(e.what()));
            return false;
        }
    }
    
    auto LoadBalancer::unregister_server(const std::string& server_id) -> bool
    {
        try {
            auto it = servers_.find(server_id);
            if (it != servers_.end()) {
                servers_.erase(it);
                Logger::handle().write(Utilities::LogTypes::Information, "Server unregistered: " + server_id);
                return true;
            }
            return false;
        }
        catch (const std::exception& e) {
            Logger::handle().write(LogTypes::Error, "Failed to unregister server: " + std::string(e.what()));
            return false;
        }
    }
    
    auto LoadBalancer::get_server_load(const std::string& server_id) -> float
    {
        auto it = servers_.find(server_id);
        if (it != servers_.end()) {
            return (it->second.cpu_usage + it->second.memory_usage) / 2.0f;
        }
        return 0.0f;
    }
}
