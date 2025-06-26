#pragma once

#include "GameNetworkConstants.h"
#include <GameSessionManager.h>
#include <NetworkServer.h>
#include <ThreadPool.h>
#include <Logger.h>

#include <memory>
#include <string>
#include <mutex>
#include <functional>
#include <optional>
#include <tuple>
#include <atomic>
#include <chrono>

namespace GameNetwork
{
    class PacketProcessor;
    class MessageDispatcher;
    class WorldSynchronizer;
    class LoadBalancer;
    class ServerMonitor;
    class GamePacket;
    struct Location;
    
    namespace Monitoring { class SystemMonitor; }
    class DisconnectionHandler;
    
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
        bool enable_heartbeat;
        uint16_t heartbeat_interval_ms;
        
        // Logging configuration
        bool enable_detailed_logging;
        bool log_packets;
        bool log_connections;
    };
    
    class GameNetworkServer : public std::enable_shared_from_this<GameNetworkServer>
    {
    public:
        explicit GameNetworkServer(const ServerConfig& config);
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
        
        // Load balancing
        auto load_balancer() -> std::shared_ptr<LoadBalancer>;
        auto server_monitor() -> std::shared_ptr<ServerMonitor>;
        
        // Broadcasting
        auto broadcast_to_all(const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto broadcast_to_channel(uint32_t channel_id, const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto broadcast_to_area(const Location& center, float radius, const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // ThreadPool access
        auto get_thread_pool() -> std::shared_ptr<Thread::ThreadPool>;
        
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
            uint64_t jobs_processed;
            uint64_t jobs_failed;
        };
        
        auto get_stats() const -> ServerStats;
        auto reset_stats() -> void;
        
        // Event callbacks
        using ServerCallback = std::function<void()>;
        using ClientCallback = std::function<void(const std::string&)>;
        auto on_server_started(ServerCallback callback) -> void;
        auto on_server_stopped(ServerCallback callback) -> void;
        auto on_client_connected(ClientCallback callback) -> void;
        auto on_client_disconnected(ClientCallback callback) -> void;
        
        // Monitoring and diagnostics
        auto log_server_status() -> void;
        auto get_active_connections() const -> std::vector<std::string>;
        auto kick_client(const std::string& client_id) -> std::tuple<bool, std::optional<std::string>>;
        
        // 모니터링 시스템
        auto get_system_monitor() -> std::shared_ptr<Monitoring::SystemMonitor>;
        
        // 재접속 핸들러
        auto get_disconnection_handler() -> std::shared_ptr<DisconnectionHandler>;
        
    private:
        auto initialize_components() -> std::tuple<bool, std::optional<std::string>>;
        auto setup_network_callbacks() -> void;
        auto start_monitoring() -> void;
        auto stop_monitoring() -> void;
        auto perform_server_maintenance() -> void;
        
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
        auto on_client_disconnected_internal(const std::string& client_id) -> void;
        
    private:
        mutable std::mutex mutex_;
        
        // Configuration
        ServerConfig config_;
        
        // Core components
        std::shared_ptr<Network::NetworkServer> network_server_;
        std::shared_ptr<Thread::ThreadPool> thread_pool_;
        std::shared_ptr<GameSessionManager> session_manager_;
        std::shared_ptr<PacketProcessor> packet_processor_;
        std::shared_ptr<MessageDispatcher> message_dispatcher_;
        std::shared_ptr<WorldSynchronizer> world_synchronizer_;
        std::shared_ptr<LoadBalancer> load_balancer_;
        std::shared_ptr<ServerMonitor> server_monitor_;
        std::shared_ptr<Monitoring::SystemMonitor> system_monitor_;
        std::shared_ptr<DisconnectionHandler> disconnection_handler_;
        
        // Server state
        std::atomic<bool> is_running_;
        std::atomic<bool> is_monitoring_;
        ServerStats stats_;
        
        // Callbacks
        std::vector<ServerCallback> server_started_callbacks_;
        std::vector<ServerCallback> server_stopped_callbacks_;
        std::vector<ClientCallback> client_connected_callbacks_;
        std::vector<ClientCallback> client_disconnected_callbacks_;
    };
}
