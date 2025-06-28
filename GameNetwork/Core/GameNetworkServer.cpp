#include "GameNetworkServer.h"
#include "../GameNetworkConstants.h"
#include "../Monitoring/SystemMonitor.h"
#include "../Session/DisconnectionHandler.h"
#include "../Session/GameSessionManager.h"
#include "../Packet/PacketProcessor.h"
#include "../Packet/MessageDispatcher.h"
#include "../Synchronization/WorldSynchronizer.h"
#include "../LoadBalancing/LoadBalancer.h"
#include "../LoadBalancing/ServerMonitor.h"
#include "../Packet/GamePacket.h"
#include <Logger.h>

namespace GameNetwork
{
    GameNetworkServer::GameNetworkServer(const ServerConfig& config)
        : config_(config)
        , is_running_(false)
        , is_monitoring_(false)
        , stats_{}
    {
        // Initialize core components
        thread_pool_ = std::make_shared<Thread::ThreadPool>("GameNetworkServer");
        
        session_manager_ = std::make_shared<GameSessionManager>();
        system_monitor_ = std::make_shared<Monitoring::SystemMonitor>();
        disconnection_handler_ = std::make_shared<DisconnectionHandler>();
        
        // Initialize other components
        packet_processor_ = std::make_shared<PacketProcessor>();
        message_dispatcher_ = std::make_shared<MessageDispatcher>();
        world_synchronizer_ = std::make_shared<WorldSynchronizer>();
        load_balancer_ = std::make_shared<LoadBalancer>();
        server_monitor_ = std::make_shared<ServerMonitor>();
        
        // Initialize network server
        network_server_ = std::make_shared<Network::NetworkServer>(
            config_.server_id,
            config_.high_priority_threads,
            config_.normal_priority_threads,
            config_.low_priority_threads
        );
        
        // Setup network callbacks
        setup_network_callbacks();
        
        stats_.start_time = std::chrono::steady_clock::now();
    }
    
    GameNetworkServer::~GameNetworkServer()
    {
        if (is_running_)
        {
            stop();
        }
    }
    
    auto GameNetworkServer::start() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_running_)
        {
            return { false, "Server is already running" };
        }
        
        try
        {
            // Initialize components
            auto [init_success, init_error] = initialize_components();
            if (!init_success)
            {
                return { false, "Failed to initialize components: " + init_error.value_or("unknown error") };
            }
            
            // Start network server
            auto [success, error] = network_server_->start(config_.port, config_.socket_buffer_size);
            if (!success)
            {
                return { false, "Failed to start network server: " + error.value_or("unknown error") };
            }
            
            // Start monitoring
            start_monitoring();
            
            is_running_ = true;
            
            // Trigger callbacks
            for (const auto& callback : server_started_callbacks_)
            {
                callback();
            }
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "GameNetworkServer started on port " + std::to_string(config_.port));
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Exception during start: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::stop() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }
        
        try
        {
            // Stop monitoring
            stop_monitoring();
            
            // Stop network server
            if (network_server_)
            {
                auto [success, error] = network_server_->stop();
                if (!success)
                {
                    Utilities::Logger::handle().write(Utilities::LogTypes::Warning,
                        "Warning: Network server stop failed: " + error.value_or("unknown error"));
                }
            }
            
            is_running_ = false;
            
            // Trigger callbacks
            for (const auto& callback : server_stopped_callbacks_)
            {
                callback();
            }
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "GameNetworkServer stopped");
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Exception during stop: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::is_running() const -> bool
    {
        return is_running_;
    }
    
    auto GameNetworkServer::config() const -> const ServerConfig&
    {
        return config_;
    }
    
    auto GameNetworkServer::update_config(const ServerConfig& new_config) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_running_)
        {
            // Some config changes might not be allowed while running
            if (new_config.port != config_.port)
            {
                return { false, "Cannot change port while server is running" };
            }
        }
        
        config_ = new_config;
        
        // Apply configuration changes to components
        if (thread_pool_)
        {
            // Update thread pool if thread counts changed
            // This would require ThreadPool to support resizing
        }
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Server configuration updated");
        
        return { true, std::nullopt };
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
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }
        
        try
        {
            auto sessions = session_manager_->get_all_online_sessions();
            for (const auto& session : sessions)
            {
                if (session && session->is_connected())
                {
                    auto connection = session->connection();
                    if (connection)
                    {
                        auto serialized = packet_processor_->serialize(packet);
                        if (serialized)
                        {
                            connection->send(*serialized);
                            stats_.packets_sent++;
                        }
                    }
                }
            }
            
            stats_.bytes_sent += packet.get_payload().size() * sessions.size();
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Broadcast failed: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::broadcast_to_channel(uint32_t channel_id, const GamePacket& packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }
        
        try
        {
            auto sessions = session_manager_->get_sessions_in_channel(channel_id);
            for (const auto& session : sessions)
            {
                if (session && session->is_connected())
                {
                    auto connection = session->connection();
                    if (connection)
                    {
                        auto serialized = packet_processor_->serialize(packet);
                        if (serialized)
                        {
                            connection->send(*serialized);
                            stats_.packets_sent++;
                        }
                    }
                }
            }
            
            stats_.bytes_sent += packet.get_payload().size() * sessions.size();
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Channel broadcast failed: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::broadcast_to_area(const Location& center, float radius, const GamePacket& packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }
        
        try
        {
            auto all_sessions = session_manager_->get_all_online_sessions();
            std::vector<std::shared_ptr<GameSession>> sessions_in_area;
            
            // Filter sessions by area
            for (const auto& session : all_sessions)
            {
                if (session && session->is_connected())
                {
                    auto session_loc = session->location();
                    float distance = glm::distance(
                        glm::vec3(center.position.x, center.position.y, center.position.z),
                        glm::vec3(session_loc.position.x, session_loc.position.y, session_loc.position.z)
                    );
                    
                    if (distance <= radius)
                    {
                        sessions_in_area.push_back(session);
                    }
                }
            }
            
            // Send to sessions in area
            for (const auto& session : sessions_in_area)
            {
                auto connection = session->connection();
                if (connection)
                {
                    auto serialized = packet_processor_->serialize(packet);
                    if (serialized)
                    {
                        connection->send(*serialized);
                        stats_.packets_sent++;
                    }
                }
            }
            
            stats_.bytes_sent += packet.get_payload().size() * sessions_in_area.size();
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Area broadcast failed: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::get_thread_pool() -> std::shared_ptr<Thread::ThreadPool>
    {
        return thread_pool_;
    }
    
    auto GameNetworkServer::get_stats() const -> ServerStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    auto GameNetworkServer::reset_stats() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        stats_.total_connections = 0;
        stats_.current_connections = session_manager_ ? session_manager_->online_session_count() : 0;
        stats_.packets_sent = 0;
        stats_.packets_received = 0;
        stats_.bytes_sent = 0;
        stats_.bytes_received = 0;
        stats_.jobs_processed = 0;
        stats_.jobs_failed = 0;
        stats_.start_time = std::chrono::steady_clock::now();
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
    
    auto GameNetworkServer::on_client_connected(ClientCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        client_connected_callbacks_.push_back(callback);
    }
    
    auto GameNetworkServer::on_client_disconnected(ClientCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        client_disconnected_callbacks_.push_back(callback);
    }
    
    auto GameNetworkServer::log_server_status() -> void
    {
        auto stats = get_stats();
        auto uptime = std::chrono::steady_clock::now() - stats.start_time;
        auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(uptime).count();
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "=== Server Status ===");
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Server ID: " + config_.server_id);
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Uptime: " + std::to_string(uptime_seconds) + " seconds");
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Current Connections: " + std::to_string(stats.current_connections));
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Total Connections: " + std::to_string(stats.total_connections));
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Packets Sent: " + std::to_string(stats.packets_sent));
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Packets Received: " + std::to_string(stats.packets_received));
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Bytes Sent: " + std::to_string(stats.bytes_sent));
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Bytes Received: " + std::to_string(stats.bytes_received));
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Jobs Processed: " + std::to_string(stats.jobs_processed));
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Jobs Failed: " + std::to_string(stats.jobs_failed));
    }
    
    auto GameNetworkServer::get_active_connections() const -> std::vector<std::string>
    {
        std::vector<std::string> active_connections;
        
        auto sessions = session_manager_->get_all_online_sessions();
        for (const auto& session : sessions)
        {
            if (session && session->is_connected())
            {
                active_connections.push_back(session->session_id());
            }
        }
        
        return active_connections;
    }
    
    auto GameNetworkServer::kick_client(const std::string& client_id) -> std::tuple<bool, std::optional<std::string>>
    {
        auto session = session_manager_->get_session_by_id(client_id);
        if (!session)
        {
            return { false, "Client not found" };
        }
        
        try
        {
            // Disconnect the client
            auto connection = session->connection();
            if (connection)
            {
                connection->disconnect();
            }
            
            // Remove from session manager
            session_manager_->terminate_session(client_id);
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Client kicked: " + client_id);
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Failed to kick client: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::get_system_monitor() -> std::shared_ptr<Monitoring::SystemMonitor>
    {
        return system_monitor_;
    }
    
    auto GameNetworkServer::get_disconnection_handler() -> std::shared_ptr<DisconnectionHandler>
    {
        return disconnection_handler_;
    }
    
    auto GameNetworkServer::initialize_components() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            // Initialize session manager
            if (session_manager_)
            {
                session_manager_->set_thread_pool(thread_pool_);
            }
            
            // Initialize message dispatcher
            if (message_dispatcher_)
            {
                message_dispatcher_->set_thread_pool(thread_pool_);
                message_dispatcher_->register_default_handlers();
            }
            
            // Initialize world synchronizer
            if (world_synchronizer_)
            {
                world_synchronizer_->initialize(session_manager_);
                world_synchronizer_->set_thread_pool(thread_pool_);
            }
            
            // Initialize server monitor
            if (server_monitor_)
            {
                server_monitor_->set_server_id(config_.server_id);
            }
            
            // Initialize disconnection handler
            if (disconnection_handler_)
            {
                disconnection_handler_->set_session_manager(session_manager_);
            }
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Component initialization failed: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::setup_network_callbacks() -> void
    {
        if (!network_server_)
        {
            return;
        }
        
        // Set connection callbacks
        network_server_->received_connection_callback(
            [this](const std::string& client_id, const std::string& sub_id, const bool& condition)
            {
                return this->on_client_connected(client_id, sub_id, condition);
            });
        
        network_server_->received_message_callback(
            [this](const std::string& client_id, const std::string& sub_id, const std::string& message)
            {
                return this->on_message_received(client_id, sub_id, message);
            });
        
        network_server_->received_binary_callback(
            [this](const std::string& client_id, const std::string& sub_id, const std::string& message, const std::vector<uint8_t>& data)
            {
                return this->on_binary_received(client_id, sub_id, message, data);
            });
        
        // NetworkServer doesn't have disconnected callback, it's handled differently
    }
    
    auto GameNetworkServer::on_client_connected(const std::string& client_id, 
                                                const std::string& sub_id, 
                                                const bool& condition) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            // Create new session
            auto [session, error] = session_manager_->create_session(client_id);
            if (!session)
            {
                return { false, error.value_or("Failed to create session") };
            }
            
            // Update stats
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.total_connections++;
                stats_.current_connections = session_manager_->online_session_count();
            }
            
            // Trigger callbacks
            for (const auto& callback : client_connected_callbacks_)
            {
                callback(client_id);
            }
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Client connected: " + client_id);
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Connection handling failed: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::on_message_received(const std::string& client_id, 
                                                const std::string& sub_id, 
                                                const std::string& message) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            // Update stats
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.packets_received++;
                stats_.bytes_received += message.size();
            }
            
            // Process as text message if needed
            // Most game messages will be binary, so this might be for chat or commands
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, "Message processing failed: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::on_binary_received(const std::string& client_id, 
                                              const std::string& sub_id, 
                                              const std::string& message, 
                                              const std::vector<uint8_t>& data) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            // Update stats
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.packets_received++;
                stats_.bytes_received += data.size();
            }
            
            // Find session
            auto session = session_manager_->get_session_by_id(client_id);
            if (!session)
            {
                return { false, "Session not found" };
            }
            
            // Deserialize packet
            auto packet = packet_processor_->deserialize_binary(data);
            if (!packet)
            {
                return { false, "Failed to deserialize packet" };
            }
            
            // Dispatch to appropriate handler
            if (message_dispatcher_)
            {
                message_dispatcher_->dispatch(session, *packet);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stats_.jobs_processed++;
                }
            }
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.jobs_failed++;
            }
            return { false, "Binary message processing failed: " + std::string(e.what()) };
        }
    }
    
    auto GameNetworkServer::on_client_disconnected_internal(const std::string& client_id) -> void
    {
        try
        {
            // Get session first
            auto session = session_manager_->get_session_by_id(client_id);
            
            // Handle disconnection through disconnection handler
            if (disconnection_handler_ && session)
            {
                disconnection_handler_->handle_disconnection(session, DisconnectReason::NetworkError);
            }
            
            // Update stats
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.current_connections = session_manager_->online_session_count();
            }
            
            // Trigger callbacks
            for (const auto& callback : client_disconnected_callbacks_)
            {
                callback(client_id);
            }
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Client disconnected: " + client_id);
        }
        catch (const std::exception& e)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                "Error handling disconnection: " + std::string(e.what()));
        }
    }
    
    auto GameNetworkServer::start_monitoring() -> void
    {
        if (is_monitoring_.exchange(true))
        {
            return;
        }
        
        if (system_monitor_)
        {
            system_monitor_->start_recording(std::chrono::seconds(5));
            
            // Log monitoring started
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "System monitoring started");
        }
        
        // Periodic maintenance is handled separately
    }
    
    auto GameNetworkServer::stop_monitoring() -> void
    {
        if (!is_monitoring_.exchange(false))
        {
            return;
        }
        
        if (system_monitor_)
        {
            system_monitor_->stop_recording();
            
            // Log monitoring stopped
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "System monitoring stopped");
        }
    }
    
    auto GameNetworkServer::perform_server_maintenance() -> void
    {
        if (!is_running_)
        {
            return;
        }
        
        try
        {
            // Perform session cleanup
            if (session_manager_)
            {
                session_manager_->perform_maintenance();
                
                // Log cleanup results separately
                auto active_count = session_manager_->active_session_count();
                Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                    "Active sessions: " + std::to_string(active_count));
            }
            
            // Check system resources
            if (system_monitor_)
            {
                auto metrics = system_monitor_->get_current_metrics();
                
                // Log warnings if resources are high
                if (metrics.cpu_usage_percent > 80.0f)
                {
                    Utilities::Logger::handle().write(Utilities::LogTypes::Warning,
                        "High CPU usage: " + std::to_string(metrics.cpu_usage_percent) + "%");
                }
                
                if (metrics.memory_usage_percent > 80.0f)
                {
                    Utilities::Logger::handle().write(Utilities::LogTypes::Warning,
                        "High memory usage: " + std::to_string(metrics.memory_usage_percent) + "%");
                }
            }
            
            // Log server status periodically
            static auto last_status_log = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(now - last_status_log).count() >= 5)
            {
                log_server_status();
                last_status_log = now;
            }
        }
        catch (const std::exception& e)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                "Server maintenance error: " + std::string(e.what()));
        }
    }
    
} // namespace GameNetwork