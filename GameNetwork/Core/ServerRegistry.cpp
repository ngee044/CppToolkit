#include "ServerRegistry.h"
#include "../Core/GameNetworkServer.h"
#include <Logger.h>
#include <algorithm>

using namespace Utilities;

namespace GameNetwork {

ServerRegistry& ServerRegistry::get_instance()
{
    static ServerRegistry instance;
    return instance;
}

void ServerRegistry::register_local_server(std::shared_ptr<GameNetworkServer> server)
{
    local_server_ = server;
    Logger::handle().write(LogTypes::Information, "Local game server registered");
}

std::shared_ptr<GameNetworkServer> ServerRegistry::get_local_server()
{
    return local_server_;
}

void ServerRegistry::register_remote_server(const std::string& server_id, const std::string& endpoint)
{
    remote_servers_[server_id] = endpoint;
    Logger::handle().write(LogTypes::Information, "Remote server registered: " + server_id + " -> " + endpoint);
}

void ServerRegistry::unregister_remote_server(const std::string& server_id)
{
    auto it = remote_servers_.find(server_id);
    if (it != remote_servers_.end()) {
        remote_servers_.erase(it);
        Logger::handle().write(LogTypes::Information, "Remote server unregistered: " + server_id);
    }
}

std::vector<std::string> ServerRegistry::get_registered_servers() const
{
    std::vector<std::string> server_ids;
    server_ids.reserve(remote_servers_.size());
    
    for (const auto& [server_id, endpoint] : remote_servers_) {
        server_ids.push_back(server_id);
    }
    
    return server_ids;
}

bool ServerRegistry::is_server_registered(const std::string& server_id) const
{
    return remote_servers_.find(server_id) != remote_servers_.end();
}

std::string ServerRegistry::send_network_request(const std::string& url, const std::string& payload)
{
    // Simple stub implementation for network requests
    try {
        // In a real implementation, this would use HTTP client library
        // For now, return a dummy success response
        Logger::handle().write(LogTypes::Information, "Network request sent to: " + url);
        return R"({"status": "success", "message": "Request processed"})";
    }
    catch (const std::exception& e) {
        Logger::handle().write(LogTypes::Error, "Network request failed: " + std::string(e.what()));
        return R"({"status": "error", "message": "Request failed"})";
    }
}

std::tuple<bool, std::string> ServerRegistry::send_message(const std::string& target_server, const std::string& message)
{
    try {
        // Check if target server is registered
        if (!is_server_registered(target_server)) {
            Logger::handle().write(LogTypes::Warning, "Target server not registered: " + target_server);
            return std::make_tuple(false, "Target server not registered: " + target_server);
        }
        
        // Get endpoint for target server
        auto endpoint = remote_servers_.at(target_server);
        
        // Stub implementation for inter-server messaging
        Logger::handle().write(LogTypes::Information, "Sending message to server: " + target_server + " (" + endpoint + ")");
        // In real implementation, would use networking library
        return std::make_tuple(true, "Message sent successfully");
    }
    catch (const std::exception& e) {
        Logger::handle().write(LogTypes::Error, "Failed to send message: " + std::string(e.what()));
        return std::make_tuple(false, "Failed to send message: " + std::string(e.what()));
    }
}

std::tuple<bool, std::string> ServerRegistry::send_and_wait(const std::string& target_server, const std::string& message, int timeout_ms)
{
    try {
        // Check if target server is registered
        if (!is_server_registered(target_server)) {
            Logger::handle().write(LogTypes::Warning, "Target server not registered: " + target_server);
            return std::make_tuple(false, "Target server not registered: " + target_server);
        }
        
        // Get endpoint for target server
        auto endpoint = remote_servers_.at(target_server);
        
        // Stub implementation for synchronous inter-server messaging
        Logger::handle().write(LogTypes::Information, 
            "Sending sync message to server: " + target_server + " (" + endpoint + ") with timeout: " + std::to_string(timeout_ms) + "ms");
        // In real implementation, would use networking library with timeout
        return std::make_tuple(true, R"({"status": "success", "response": "Operation completed"})");
    }
    catch (const std::exception& e) {
        Logger::handle().write(LogTypes::Error, "Failed to send sync message: " + std::string(e.what()));
        return std::make_tuple(false, "Failed to send sync message: " + std::string(e.what()));
    }
}

} // namespace GameNetwork
