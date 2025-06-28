#include "GameNetworkServer.h"
#include "../Monitoring/SystemMonitor.h"
#include "../Session/DisconnectionHandler.h"
#include "../Session/GameSessionManager.h"
#include "../Packet/PacketProcessor.h"
#include "../Packet/MessageDispatcher.h"
#include "../Synchronization/WorldSynchronizer.h"
#include "../LoadBalancing/LoadBalancer.h"
#include "../LoadBalancing/ServerMonitor.h"

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
    
    auto GameNetworkServer::session_manager() -> std::shared_ptr<GameSessionManager>
    {
        return session_manager_;
    }
    
    auto GameNetworkServer::message_dispatcher() -> std::shared_ptr<MessageDispatcher>
    {
        return message_dispatcher_;
    }
    
    auto GameNetworkServer::world_synchronizer() -> std::shared_ptr<WorldSynchronizer>
    {
        return world_synchronizer_;
    }
    
    auto GameNetworkServer::get_stats() const -> ServerStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
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
    
    auto GameNetworkServer::setup_network_callbacks() -> void
    {
        // Setup network server callbacks here
        // This would connect to the NetworkServer's callback system
    }
    auto GameNetworkServer::get_system_monitor() -> std::shared_ptr<Monitoring::SystemMonitor>
    {
        return system_monitor_;
    }
    
    auto GameNetworkServer::get_disconnection_handler() -> std::shared_ptr<DisconnectionHandler>
    {
        return disconnection_handler_;
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
    }    auto GameNetworkServer::stop_monitoring() -> void
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
    
} // namespace GameNetwork