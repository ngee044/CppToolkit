#pragma once

#include "GameNetworkConstants.h"
#include <NetworkClient.h>
#include <ThreadPool.h>
#include <Logger.h>

#include <memory>
#include <string>
#include <functional>
#include <future>
#include <optional>
#include <atomic>
#include <chrono>

namespace GameNetwork
{
    struct ClientConfig
    {
        std::string server_host;
        uint16_t server_port;
        std::string client_id;
        bool enable_auto_reconnect;
        uint16_t reconnect_interval_ms;
        uint16_t connection_timeout_ms;
        uint16_t heartbeat_interval_ms;
    };

    class GameNetworkClient
    {
    public:
        explicit GameNetworkClient(const ClientConfig& config);
        virtual ~GameNetworkClient();
        
        // Connection management
        auto connect() -> std::tuple<bool, std::optional<std::string>>;
        auto disconnect() -> std::tuple<bool, std::optional<std::string>>;
        auto is_connected() const -> bool;
        auto reconnect() -> std::tuple<bool, std::optional<std::string>>;
        
        // Message handling
        auto send_message(const std::string& message) -> std::tuple<bool, std::optional<std::string>>;
        auto send_binary(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
        
        // Event callbacks
        using MessageCallback = std::function<void(const std::string&)>;
        using BinaryCallback = std::function<void(const std::vector<uint8_t>&)>;
        using ConnectionCallback = std::function<void(bool)>;
        
        auto on_message_received(MessageCallback callback) -> void;
        auto on_binary_received(BinaryCallback callback) -> void;
        auto on_connection_changed(ConnectionCallback callback) -> void;
        
        // Configuration
        auto get_config() const -> const ClientConfig&;
        auto update_config(const ClientConfig& new_config) -> std::tuple<bool, std::optional<std::string>>;
        
        // Statistics
        struct ClientStats
        {
            std::chrono::steady_clock::time_point connection_time;
            uint64_t messages_sent;
            uint64_t messages_received;
            uint64_t bytes_sent;
            uint64_t bytes_received;
            uint32_t reconnect_count;
        };
        
        auto get_stats() const -> ClientStats;
        
    private:
        auto initialize_components() -> std::tuple<bool, std::optional<std::string>>;
        auto setup_network_callbacks() -> void;
        auto start_heartbeat() -> void;
        auto stop_heartbeat() -> void;
        auto perform_heartbeat() -> void;
        auto handle_auto_reconnect() -> void;
        
        // Network event handlers
        auto on_network_connected() -> void;
        auto on_network_disconnected() -> void;
        auto on_network_message(const std::string& message) -> void;
        auto on_network_binary(const std::vector<uint8_t>& data) -> void;
        
    private:
        ClientConfig config_;
        std::shared_ptr<Network::NetworkClient> network_client_;
        std::shared_ptr<Thread::ThreadPool> thread_pool_;
        
        std::atomic<bool> is_connected_;
        std::atomic<bool> is_shutdown_;
        std::atomic<bool> heartbeat_running_;
        
        ClientStats stats_;
        
        // Callbacks
        MessageCallback message_callback_;
        BinaryCallback binary_callback_;
        ConnectionCallback connection_callback_;
        
        mutable std::mutex mutex_;
    };
}
