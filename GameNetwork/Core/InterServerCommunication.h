#pragma once

#include "../GameNetworkConstants.h"
#include "../../Network/NetworkClient.h"
#include "../../Network/NetworkServer.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include <tuple>
#include <queue>

namespace GameNetwork
{
    enum class ServerType
    {
        Gateway = 0,
        Game = 1,
        World = 2,
        Database = 3,
        Chat = 4,
        Matching = 5
    };
    
    struct ServerInfo
    {
        std::string server_id;
        std::string server_name;
        ServerType type;
        std::string ip_address;
        uint16_t port;
        uint32_t current_load;
        uint32_t max_capacity;
        std::chrono::steady_clock::time_point last_heartbeat;
        bool is_online;
    };
    
    struct InterServerMessage
    {
        std::string source_server_id;
        std::string target_server_id;
        std::string message_type;
        std::vector<uint8_t> payload;
        uint32_t sequence;
        std::chrono::steady_clock::time_point timestamp;
        bool requires_response;
        uint32_t correlation_id;
    };
    
    class InterServerCommunication : public std::enable_shared_from_this<InterServerCommunication>
    {
    public:
        InterServerCommunication(const std::string& server_id, ServerType type);
        virtual ~InterServerCommunication();
        
        // Server lifecycle
        auto start(uint16_t listen_port) -> std::tuple<bool, std::optional<std::string>>;
        auto stop() -> void;
        auto is_running() const -> bool;
        
        // Server registration
        auto register_with_cluster(const std::string& cluster_address, uint16_t cluster_port) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto unregister_from_cluster() -> void;
        
        // Server discovery
        auto discover_servers(ServerType type = ServerType::Game) const 
            -> std::vector<ServerInfo>;
        auto find_server(const std::string& server_id) const 
            -> std::optional<ServerInfo>;
        auto get_least_loaded_server(ServerType type) const 
            -> std::optional<ServerInfo>;
        
        // Message sending
        auto send_message(const std::string& target_server_id, const std::string& message_type, const std::vector<uint8_t>& payload, bool requires_response = false) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        auto send_broadcast(ServerType target_type, const std::string& message_type, const std::vector<uint8_t>& payload) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        auto send_request(const std::string& target_server_id, const std::string& message_type, const std::vector<uint8_t>& payload, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) 
            -> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>;
        
        // Message handling
        using MessageHandler = std::function<std::vector<uint8_t>(const InterServerMessage&)>;
        auto register_message_handler(const std::string& message_type, MessageHandler handler) -> void;
        auto unregister_message_handler(const std::string& message_type) -> void;
        
        // Load balancing
        auto update_server_load(uint32_t current_load) -> void;
        auto get_server_load() const -> uint32_t;
        auto set_max_capacity(uint32_t max_capacity) -> void;
        
        // Session migration
        auto migrate_session(const std::string& session_id, const std::string& target_server_id, const std::vector<uint8_t>& session_data) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        auto accept_session_migration(const std::string& session_id, const std::vector<uint8_t>& session_data) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Heartbeat & monitoring
        auto enable_heartbeat(std::chrono::seconds interval = std::chrono::seconds(30)) -> void;
        auto disable_heartbeat() -> void;
        auto check_server_health() -> void;
        
        // Server events
        using ServerEventCallback = std::function<void(const ServerInfo&)>;
        auto on_server_connected(ServerEventCallback callback) -> void;
        auto on_server_disconnected(ServerEventCallback callback) -> void;
        auto on_server_load_changed(ServerEventCallback callback) -> void;
        
        // Statistics
        struct InterServerStats
        {
            uint64_t messages_sent;
            uint64_t messages_received;
            uint64_t bytes_sent;
            uint64_t bytes_received;
            uint64_t failed_sends;
            uint64_t sessions_migrated_out;
            uint64_t sessions_migrated_in;
            std::unordered_map<std::string, uint64_t> messages_by_type;
            std::unordered_map<std::string, std::chrono::microseconds> average_response_time;
        };
        
        auto get_stats() const -> InterServerStats;
        auto reset_stats() -> void;
        
    private:
        struct Connection
        {
            std::shared_ptr<Network::NetworkClient> client;
            bool is_connected = false;
            std::chrono::steady_clock::time_point last_active;
            std::queue<InterServerMessage> outgoing_queue;
        };

        struct PendingRequest
        {
            uint32_t correlation_id;
            std::promise<std::vector<uint8_t>> response_promise;
            std::chrono::steady_clock::time_point timeout_time;
        };

        auto handle_incoming_message(const std::string& source_server_id, const std::string& message, const std::vector<uint8_t>& data) -> void;
        auto get_or_create_connection(const std::string& target_server_id) -> std::shared_ptr<Network::NetworkClient>;
        auto establish_connection(const ServerInfo& server_info) -> bool;
        auto send_heartbeat() -> void;
        auto cleanup_dead_servers() -> void;
        auto handle_heartbeat(const std::string& source_server_id) -> void;
        auto handle_response(uint32_t correlation_id, const std::vector<uint8_t>& payload) -> void;
        auto send_response(const std::string& target_server_id, uint32_t correlation_id, const std::vector<uint8_t>& payload) -> void;

        mutable std::mutex mutex_;
        std::string server_id_;
        ServerType server_type_;
        uint16_t listen_port_;
        std::shared_ptr<Network::NetworkServer> server_;
        std::unordered_map<std::string, Connection> connections_;
        std::unordered_map<std::string, ServerInfo> known_servers_;
        std::string cluster_address_;
        uint16_t cluster_port_;
        std::shared_ptr<Network::NetworkClient> cluster_connection_;
        
        uint32_t current_load_;
        uint32_t max_capacity_;
        
        std::atomic<bool> is_running_;
        
        std::unordered_map<std::string, MessageHandler> message_handlers_;
        std::unordered_map<uint32_t, PendingRequest> pending_requests_;
        std::atomic<uint32_t> next_correlation_id_;
        
        std::atomic<bool> heartbeat_enabled_;
        std::chrono::seconds heartbeat_interval_;
        std::future<void> heartbeat_thread_;
        
        std::vector<ServerEventCallback> on_server_connected_callbacks_;
        std::vector<ServerEventCallback> on_server_disconnected_callbacks_;
        std::vector<ServerEventCallback> on_server_load_changed_callbacks_;
        
        InterServerStats stats_;
    };
}
