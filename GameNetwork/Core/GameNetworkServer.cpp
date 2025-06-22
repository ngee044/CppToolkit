#include "GameNetworkServer.h"
#include "../Packet/PacketProcessor.h"
#include "../Packet/MessageDispatcher.h"
#include "../Synchronization/WorldSynchronizer.h"
#include "../LoadBalancing/LoadBalancer.h"
#include "../LoadBalancing/ServerMonitor.h"
#include <fmt/format.h>

using namespace Utilities;
using namespace fmt;

namespace GameNetwork
{
    GameNetworkServer::GameNetworkServer(const ServerConfig& config)
        : config_(config)
        , is_running_(false)
    {
        // Initialize statistics
        stats_.total_connections = 0;
        stats_.current_connections = 0;
        stats_.packets_sent = 0;
        stats_.packets_received = 0;
        stats_.bytes_sent = 0;
        stats_.bytes_received = 0;
        stats_.start_time = std::chrono::steady_clock::now();
        
        // Create components
        session_manager_ = std::make_shared<GameSessionManager>();
        packet_processor_ = std::make_shared<PacketProcessor>();
        message_dispatcher_ = std::make_shared<MessageDispatcher>();
        world_synchronizer_ = std::make_shared<WorldSynchronizer>();
        load_balancer_ = std::make_shared<LoadBalancer>(LoadBalancingStrategy::LeastLoad);
        server_monitor_ = std::make_shared<ServerMonitor>(load_balancer_);
        
        // Create base network server
        network_server_ = std::make_shared<Network::NetworkServer>(
            config_.server_id,
            config_.high_priority_threads,
            config_.normal_priority_threads,
            config_.low_priority_threads
        );
        
        // Setup callbacks
        setup_network_callbacks();
    }
    
    GameNetworkServer::~GameNetworkServer()
    {
        stop();
    }
    
    auto GameNetworkServer::start() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_running_)
        {
            return {false, "Server already running"};
        }
        
        // Initialize components
        auto [init_success, init_error] = initialize_components();
        if (!init_success)
        {
            return {false, init_error};
        }
        
        // Start network server
        auto [start_success, start_error] = network_server_->start(
            config_.port, config_.socket_buffer_size);
        if (!start_success)
        {
            return {false, start_error};
        }
        
        is_running_ = true;
        
        // Trigger callbacks
        for (const auto& callback : server_started_callbacks_)
        {
            callback();
        }
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::stop() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_running_)
        {
            return {false, "Server not running"};
        }
        
        is_running_ = false;
        
        // Stop network server
        auto [stop_success, stop_error] = network_server_->stop();
        if (!stop_success)
        {
            return {false, stop_error};
        }
        
        // Stop server monitor
        if (server_monitor_)
        {
            server_monitor_->stop();
        }
        
        // Stop world synchronizer
        world_synchronizer_->stop_sync_timer();
        
        // Clear all sessions
        session_manager_->cleanup_inactive_sessions();
        
        // Trigger callbacks
        for (const auto& callback : server_stopped_callbacks_)
        {
            callback();
        }
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::is_running() const -> bool
    {
        return is_running_;
    }
    
    auto GameNetworkServer::config() const -> const ServerConfig&
    {
        return config_;
    }
    
    auto GameNetworkServer::session_manager() -> std::shared_ptr<GameSessionManager>
    {
        return session_manager_;
    }
    
    auto GameNetworkServer::session_manager() const -> std::shared_ptr<const GameSessionManager>
    {
        return session_manager_;
    }
    
    auto GameNetworkServer::packet_processor() -> std::shared_ptr<PacketProcessor>
    {
        return packet_processor_;
    }
    
    auto GameNetworkServer::message_dispatcher() -> std::shared_ptr<MessageDispatcher>
    {
        return message_dispatcher_;
    }
    
    auto GameNetworkServer::world_synchronizer() -> std::shared_ptr<WorldSynchronizer>
    {
        return world_synchronizer_;
    }
    
    auto GameNetworkServer::load_balancer() -> std::shared_ptr<LoadBalancer>
    {
        return load_balancer_;
    }
    
    auto GameNetworkServer::server_monitor() -> std::shared_ptr<ServerMonitor>
    {
        return server_monitor_;
    }
    
    auto GameNetworkServer::broadcast_to_all(const GamePacket& packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto sessions = session_manager_->get_all_online_sessions();
        
        for (const auto& session : sessions)
        {
            auto connection = session->current_connection();
            if (connection)
            {
                connection->send_packet(packet);
                stats_.packets_sent++;
            }
        }
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::broadcast_to_channel(uint32_t channel_id, const GamePacket& packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto sessions = world_synchronizer_->get_channel_sessions(channel_id);
        
        for (const auto& session : sessions)
        {
            auto connection = session->current_connection();
            if (connection)
            {
                connection->send_packet(packet);
                stats_.packets_sent++;
            }
        }
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::broadcast_to_area(const Location& center, float radius, const GamePacket& packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        return world_synchronizer_->broadcast_to_area(center, radius, packet);
    }
    
    auto GameNetworkServer::get_stats() const -> ServerStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    auto GameNetworkServer::on_server_started(ServerCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server_started_callbacks_.push_back(callback);
    }
    
    auto GameNetworkServer::on_server_stopped(ServerCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server_stopped_callbacks_.push_back(callback);
    }
    
    auto GameNetworkServer::initialize_components() -> std::tuple<bool, std::optional<std::string>>
    {
        // Configure packet processor
        packet_processor_->enable_compression(config_.enable_compression);
        packet_processor_->enable_encryption(config_.enable_encryption);
        
        // Configure message dispatcher
        message_dispatcher_->enable_rate_limiting(config_.enable_rate_limiting);
        message_dispatcher_->register_default_handlers();
        
        // Start world synchronizer
        world_synchronizer_->start_sync_timer();
        
        // Register this server with load balancer
        ServerMetrics local_metrics;
        local_metrics.server_id = config_.server_id;
        local_metrics.server_address = "localhost"; // TODO: Get actual address
        local_metrics.port = config_.port;
        local_metrics.current_players = 0;
        local_metrics.max_players = config_.max_players;
        local_metrics.cpu_usage = 0.0f;
        local_metrics.memory_usage = 0.0f;
        local_metrics.network_usage = 0.0f;
        local_metrics.latency_ms = 0;
        local_metrics.is_healthy = true;
        local_metrics.is_accepting_players = true;
        
        auto [register_success, register_error] = load_balancer_->register_server(local_metrics);
        if (!register_success)
        {
            return {false, format("Failed to register server with load balancer: {}",
                register_error.value_or("Unknown error"))};
        }
        
        // Register channels
        for (uint32_t i = 1; i <= config_.max_channels; ++i)
        {
            ChannelMetrics channel;
            channel.channel_id = i;
            channel.channel_name = format("Channel {}", i);
            channel.current_players = 0;
            channel.max_players = config_.max_players / config_.max_channels;
            channel.map_id = 0;
            channel.load_factor = 0.0f;
            channel.is_available = true;
            channel.is_recommended = (i == 1); // First channel is recommended
            
            load_balancer_->register_channel(config_.server_id, channel);
        }
        
        // Configure server monitor
        server_monitor_->set_metrics_provider([this]() {
            ServerMetrics metrics;
            metrics.server_id = config_.server_id;
            metrics.server_address = "localhost";
            metrics.port = config_.port;
            metrics.current_players = session_manager_->get_online_count();
            metrics.max_players = config_.max_players;
            // TODO: Implement actual resource monitoring
            metrics.cpu_usage = 50.0f;
            metrics.memory_usage = 60.0f;
            metrics.network_usage = 30.0f;
            metrics.latency_ms = 10;
            metrics.is_healthy = true;
            metrics.is_accepting_players = metrics.current_players < metrics.max_players;
            return metrics;
        });
        
        // Start server monitoring
        auto [monitor_success, monitor_error] = server_monitor_->start();
        if (!monitor_success)
        {
            Logger::handle().write(Utilities::LogTypes::Error,
                std::string(format("Failed to start server monitor: {}",
                    monitor_error.value_or("Unknown error"))));
        }
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::setup_network_callbacks() -> void
    {
        network_server_->received_connection_callback(
            [this](const std::string& client_id, const std::string& sub_id, const bool& condition)
            {
                return on_client_connected(client_id, sub_id, condition);
            });
        
        network_server_->received_message_callback(
            [this](const std::string& client_id, const std::string& sub_id, const std::string& message)
            {
                return on_message_received(client_id, sub_id, message);
            });
        
        network_server_->received_binary_callback(
            [this](const std::string& client_id, const std::string& sub_id, 
                   const std::string& message, const std::vector<uint8_t>& data)
            {
                return on_binary_received(client_id, sub_id, message, data);
            });
    }
    
    auto GameNetworkServer::on_client_connected(const std::string& client_id, 
                                                 const std::string& sub_id, 
                                                 const bool& condition) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        stats_.total_connections++;
        stats_.current_connections++;
        
        // Network session will be bound to game session after authentication
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::on_message_received(const std::string& client_id, 
                                                 const std::string& sub_id, 
                                                 const std::string& message) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Text messages not used in game protocol
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::on_binary_received(const std::string& client_id, 
                                                const std::string& sub_id, 
                                                const std::string& message, 
                                                const std::vector<uint8_t>& data) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        stats_.packets_received++;
        stats_.bytes_received += data.size();
        
        // Deserialize packet
        auto [packet, deserialize_error] = packet_processor_->deserialize_packet(data);
        if (!packet)
        {
            return {false, deserialize_error};
        }
        
        // Find session by network session ID
        // For now, we'll use a simple mapping
        // In production, this would be tracked properly
        
        // Special handling for authentication packets
        if (packet->type() == PacketType::Authentication)
        {
            // Handle authentication separately
            auto auth_packet = static_cast<AuthenticationPacket*>(packet.get());
            auto [connected, error] = session_manager_->on_network_connected(
                nullptr, auth_packet->account_id());
            
            if (!connected)
            {
                return {false, error};
            }
        }
        
        // Dispatch packet through message dispatcher
        // This would need proper session lookup in production
        auto sessions = session_manager_->get_all_online_sessions();
        if (!sessions.empty())
        {
            message_dispatcher_->dispatch_async(sessions[0], std::move(packet));
        }
        
        return {true, std::nullopt};
    }
}
