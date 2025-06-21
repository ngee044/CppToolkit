#pragma once

#include "../GameNetworkConstants.h"
#include "../Session/GameSessionManager.h"
#include "../../Network/NetworkServer.h"

#include <memory>
#include <string>
#include <mutex>
#include <functional>
#include <optional>
#include <tuple>

namespace GameNetwork
{
    class PacketProcessor;
    class MessageDispatcher;
    class WorldSynchronizer;
    
    struct ServerConfig
    {
        std::string server_id;
        std::string server_name;
        uint16_t port;
        uint32_t max_players;
        uint32_t max_channels;
        size_t socket_buffer_size;
        
        // Thread pool configuration
        uint16_t high_priority_threads;
        uint16_t normal_priority_threads;
        uint16_t low_priority_threads;
        
        // Feature flags
        bool enable_encryption;
        bool enable_compression;
        bool enable_rate_limiting;
    };
    
    class GameNetworkServer : public std::enable_shared_from_this<GameNetworkServer>
    {
    public:
        GameNetworkServer(const ServerConfig& config);
        virtual ~GameNetworkServer();
        
        // Server lifecycle
        auto start() -> std::tuple<bool, std::optional<std::string>>;
        auto stop() -> std::tuple<bool, std::optional<std::string>>;
        auto is_running() const -> bool;
        
        // Configuration
        auto config() const -> const ServerConfig&;
        auto update_config(const ServerConfig& new_config) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Session management
        auto session_manager() -> std::shared_ptr<GameSessionManager>;
        auto session_manager() const -> std::shared_ptr<const GameSessionManager>;
        
        // Packet handling
        auto packet_processor() -> std::shared_ptr<PacketProcessor>;
        auto message_dispatcher() -> std::shared_ptr<MessageDispatcher>;
        
        // World synchronization
        auto world_synchronizer() -> std::shared_ptr<WorldSynchronizer>;
        
        // Broadcasting
        auto broadcast_to_all(const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto broadcast_to_channel(uint32_t channel_id, const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto broadcast_to_area(const Location& center, float radius, const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Server statistics
        struct ServerStats
        {
            uint64_t total_connections;
            uint64_t current_connections;
            uint64_t packets_sent;
            uint64_t packets_received;
            uint64_t bytes_sent;
            uint64_t bytes_received;
            std::chrono::steady_clock::time_point start_time;
        };
        
        auto get_stats() const -> ServerStats;
        
        // Event callbacks
        using ServerCallback = std::function<void()>;
        auto on_server_started(ServerCallback callback) -> void;
        auto on_server_stopped(ServerCallback callback) -> void;
        
    private:
        auto initialize_components() -> std::tuple<bool, std::optional<std::string>>;
        auto setup_network_callbacks() -> void;
        
        auto on_client_connected(const std::string& client_id, 
                                 const std::string& sub_id, 
                                 const bool& condition) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto on_message_received(const std::string& client_id, 
                                 const std::string& sub_id, 
                                 const std::string& message) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto on_binary_received(const std::string& client_id, 
                                const std::string& sub_id, 
                                const std::string& message, 
                                const std::vector<uint8_t>& data) 
            -> std::tuple<bool, std::optional<std::string>>;
        
    private:
        mutable std::mutex mutex_;
        
        // Configuration
        ServerConfig config_;
        
        // Core components
        std::shared_ptr<Network::NetworkServer> network_server_;
        std::shared_ptr<GameSessionManager> session_manager_;
        std::shared_ptr<PacketProcessor> packet_processor_;
        std::shared_ptr<MessageDispatcher> message_dispatcher_;
        std::shared_ptr<WorldSynchronizer> world_synchronizer_;
        
        // Server state
        std::atomic<bool> is_running_;
        ServerStats stats_;
        
        // Callbacks
        std::vector<ServerCallback> server_started_callbacks_;
        std::vector<ServerCallback> server_stopped_callbacks_;
    };
}
