#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>

namespace GameNetwork
{
    struct ServerInfo
    {
        std::string server_id;
        std::string endpoint;
        float cpu_usage;
        float memory_usage;
        int active_connections;
        bool is_healthy;
        uint32_t capacity;
        uint32_t current_load;
    };

    class LoadBalancer
    {
    public:
        LoadBalancer();
        ~LoadBalancer();
        
        auto set_server_id(const std::string& server_id) -> void;
        
        // Server management
        auto get_server_list() -> std::vector<ServerInfo>;
        auto register_server(const ServerInfo& server) -> bool;
        auto unregister_server(const std::string& server_id) -> bool;
        auto get_server_load(const std::string& server_id) -> float;
        
    private:
        std::string server_id_;
        std::map<std::string, ServerInfo> servers_;
    };
}
