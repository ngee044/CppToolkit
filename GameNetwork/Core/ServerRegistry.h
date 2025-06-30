#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <unordered_map>

namespace GameNetwork {

class GameNetworkServer;

/**
 * @brief Central registry for managing game servers and inter-server communication
 * 
 * ServerRegistry acts as a service locator and communication hub for the game network.
 * It maintains references to game servers and provides APIs for server-to-server messaging.
 */
class ServerRegistry
{
public:
	static ServerRegistry& get_instance();
    
	// Server management
	void register_local_server(std::shared_ptr<GameNetworkServer> server);
	std::shared_ptr<GameNetworkServer> get_local_server();
    
	// Server discovery and registration
	void register_remote_server(const std::string& server_id, const std::string& endpoint);
	void unregister_remote_server(const std::string& server_id);
	std::vector<std::string> get_registered_servers() const;
	bool is_server_registered(const std::string& server_id) const;
    
	// Network utilities (HTTP requests, etc.)
	std::string send_network_request(const std::string& url, const std::string& payload);
    
	// Inter-server communication
	std::tuple<bool, std::string> send_message(const std::string& target_server, const std::string& message);
	std::tuple<bool, std::string> send_and_wait(const std::string& target_server, const std::string& message, int timeout_ms = 5000);
    
private:
	ServerRegistry() = default;
	~ServerRegistry() = default;
	ServerRegistry(const ServerRegistry&) = delete;
	ServerRegistry& operator=(const ServerRegistry&) = delete;
    
	std::shared_ptr<GameNetworkServer> local_server_;
	std::unordered_map<std::string, std::string> remote_servers_; // server_id -> endpoint
};

// Backward compatibility alias - deprecated, use ServerRegistry instead
using Core = ServerRegistry;

} // namespace GameNetwork
