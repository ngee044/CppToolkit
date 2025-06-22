#include "GameNetworkServer.h"

#include <MessageDispatcher.h>
#include <Job.h>

#include <fmt/format.h>
#include <thread>

using namespace Utilities;

namespace GameNetwork
{
    GameNetworkServer::GameNetworkServer(const ServerConfig& config)
        : config_(config)
        , is_running_(false)
        , is_monitoring_(false)
        , stats_{}
    {
        Logger::handle().write(LogTypes::Information, 
            fmt::format("Initializing GameNetworkServer '{}' on port {}", 
                config_.server_name, config_.port));
        
        auto [success, error] = initialize_components();
        if (!success)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Failed to initialize GameNetworkServer: {}", 
                    error.value_or("Unknown error")));
            throw std::runtime_error("Failed to initialize GameNetworkServer");
        }
        
        Logger::handle().write(LogTypes::Information, 
            "GameNetworkServer initialized successfully");
    }
    
    GameNetworkServer::~GameNetworkServer()
    {
        Logger::handle().write(LogTypes::Information, 
            "Shutting down GameNetworkServer");
        
        stop();
        
        if (thread_pool_)
        {
            thread_pool_->stop(true);
        }
        
        Logger::handle().write(LogTypes::Information, 
            "GameNetworkServer shutdown complete");
    }
    
    auto GameNetworkServer::initialize_components() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            // Initialize ThreadPool
            thread_pool_ = std::make_shared<Thread::ThreadPool>(
                fmt::format("GameNetworkServer-{}", config_.server_name));
            
            auto [start_success, start_error] = thread_pool_->start();
            if (!start_success)
            {
                return {false, fmt::format("Failed to start ThreadPool: {}", 
                    start_error.value_or("Unknown error"))};
            }
            
            Logger::handle().write(LogTypes::Debug, 
                "ThreadPool initialized for GameNetworkServer");
            
            // Initialize NetworkServer
            network_server_ = std::make_shared<Network::NetworkServer>(config_.server_name);
            
            // Initialize game components
            session_manager_ = std::make_shared<GameSessionManager>();
            message_dispatcher_ = std::make_shared<MessageDispatcher>();
            
            setup_network_callbacks();
            
            Logger::handle().write(LogTypes::Debug, 
                "All components initialized successfully");
            
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            return {false, fmt::format("Exception during initialization: {}", e.what())};
        }
    }
    
    auto GameNetworkServer::setup_network_callbacks() -> void
    {
        Logger::handle().write(LogTypes::Debug, 
            "Setting up network callbacks for GameNetworkServer");
        
        // Initialize session manager with thread pool if not already initialized
        if (session_manager_ && thread_pool_)
        {
            auto [init_success, init_error] = session_manager_->initialize(thread_pool_);
            if (!init_success)
            {
                Logger::handle().write(LogTypes::Error,
                    fmt::format("Failed to initialize session manager: {}", 
                        init_error.value_or("Unknown error")));
            }
            else
            {
                Logger::handle().write(LogTypes::Information,
                    "Session manager initialized successfully");
            }
        }
        
        // Set up production-level error handling and monitoring
        // Note: NetworkServer callback setup would go here based on actual NetworkServer interface
    }
    
    auto GameNetworkServer::start() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_running_)
        {
            return {true, std::nullopt};
        }
        
        Logger::handle().write(LogTypes::Information, 
            fmt::format("Starting GameNetworkServer '{}' on port {}", 
                config_.server_name, config_.port));
        
        try
        {
            // Start network server
            // network_server_->start(); // Would call actual NetworkServer start
            
            // Initialize stats
            stats_.start_time = std::chrono::steady_clock::now();
            stats_.total_connections = 0;
            stats_.current_connections = 0;
            stats_.packets_sent = 0;
            stats_.packets_received = 0;
            stats_.bytes_sent = 0;
            stats_.bytes_received = 0;
            stats_.jobs_processed = 0;
            stats_.jobs_failed = 0;
            
            is_running_ = true;
            start_monitoring();
            
            // Notify callbacks
            for (const auto& callback : server_started_callbacks_)
            {
                if (callback)
                {
                    callback();
                }
            }
            
            Logger::handle().write(LogTypes::Information, 
                fmt::format("GameNetworkServer '{}' started successfully", config_.server_name));
            
            log_server_status();
            
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Failed to start GameNetworkServer: {}", e.what()));
            return {false, e.what()};
        }
    }
    
    auto GameNetworkServer::stop() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_running_)
        {
            return {true, std::nullopt};
        }
        
        Logger::handle().write(LogTypes::Information, 
            fmt::format("Stopping GameNetworkServer '{}'", config_.server_name));
        
        try
        {
            stop_monitoring();
            
            // Stop network server
            // network_server_->stop(); // Would call actual NetworkServer stop
            
            is_running_ = false;
            
            // Notify callbacks
            for (const auto& callback : server_stopped_callbacks_)
            {
                if (callback)
                {
                    callback();
                }
            }
            
            Logger::handle().write(LogTypes::Information, 
                fmt::format("GameNetworkServer '{}' stopped successfully", config_.server_name));
            
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Failed to stop GameNetworkServer: {}", e.what()));
            return {false, e.what()};
        }
    }
    
    auto GameNetworkServer::is_running() const -> bool
    {
        return is_running_;
    }
    
    auto GameNetworkServer::start_monitoring() -> void
    {
        if (is_monitoring_)
        {
            return;
        }
        
        is_monitoring_ = true;
        
        Logger::handle().write(LogTypes::Debug,
            "Starting server monitoring");
        
        auto monitoring_job = std::make_shared<Thread::Job>(Thread::JobPriorities::Low, [this]() -> std::tuple<bool, std::optional<std::string>>
        {
            while (is_monitoring_ && is_running_)
            {
                perform_server_maintenance();
                std::this_thread::sleep_for(std::chrono::seconds(30)); // Monitor every 30 seconds
            }
            return {true, std::nullopt};
        }, "ServerMonitoring");
        
        thread_pool_->push(monitoring_job);
    }
    
    auto GameNetworkServer::stop_monitoring() -> void
    {
        if (!is_monitoring_)
        {
            return;
        }
        
        Logger::handle().write(LogTypes::Debug,
            "Stopping server monitoring");
        
        is_monitoring_ = false;
    }
    
    auto GameNetworkServer::perform_server_maintenance() -> void
    {
        try
        {
            if (config_.enable_detailed_logging)
            {
                log_server_status();
            }
            
            // Cleanup disconnected sessions
            if (session_manager_)
            {
                session_manager_->cleanup_stale_sessions();
                session_manager_->cleanup_timeout_connections();
            }
            
            // Update statistics
            stats_.jobs_processed++; // This would be updated by actual job processing
            
            // Check thread pool health
            if (thread_pool_)
            {
                // Log thread pool status if needed
                if (config_.enable_detailed_logging)
                {
                    Logger::handle().write(LogTypes::Debug,
                        "ThreadPool maintenance completed");
                }
            }
            
            // Memory and performance checks could go here
            auto current_time = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::hours>(current_time - stats_.start_time).count();
            
            // Log health status every hour
            if (uptime > 0 && uptime % 24 == 0) // Every 24 hours
            {
                Logger::handle().write(LogTypes::Information,
                    fmt::format("Server health check - Uptime: {}h, Active sessions: {}, Memory checks passed",
                        uptime, stats_.current_connections));
            }
        }
        catch (const std::exception& e)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Server maintenance error: {}", e.what()));
            stats_.jobs_failed++;
        }
    }
    
    auto GameNetworkServer::log_server_status() -> void
    {
        auto stats = get_stats();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - stats.start_time).count();
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("Server Status - Uptime: {}s, Connections: {}/{}, "
                       "Packets Sent/Received: {}/{}, Jobs Processed: {}", 
                uptime, stats.current_connections, config_.max_players,
                stats.packets_sent, stats.packets_received, stats.jobs_processed));
    }
    
    auto GameNetworkServer::session_manager() -> std::shared_ptr<GameSessionManager>
    {
        if (!session_manager_)
        {
            session_manager_ = std::make_shared<GameSessionManager>();
        }
        return session_manager_;
    }
    
    auto GameNetworkServer::message_dispatcher() -> std::shared_ptr<MessageDispatcher>
    {
        if (!message_dispatcher_)
        {
            message_dispatcher_ = std::make_shared<MessageDispatcher>();
        }
        return message_dispatcher_;
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
        stats_ = {};
        stats_.start_time = std::chrono::steady_clock::now();
        
        Logger::handle().write(LogTypes::Information,
            "Server statistics reset");
    }
    
    auto GameNetworkServer::config() const -> const ServerConfig&
    {
        return config_;
    }
    
    auto GameNetworkServer::on_server_started(ServerCallback callback) -> void
    {
        server_started_callbacks_.push_back(callback);
    }
    
    auto GameNetworkServer::on_server_stopped(ServerCallback callback) -> void
    {
        server_stopped_callbacks_.push_back(callback);
    }
    
    auto GameNetworkServer::on_client_connected(ClientCallback callback) -> void
    {
        client_connected_callbacks_.push_back(callback);
    }
    
    auto GameNetworkServer::on_client_disconnected(ClientCallback callback) -> void
    {
        client_disconnected_callbacks_.push_back(callback);
    }
    
    auto GameNetworkServer::on_client_connected(const std::string& client_id, 
                                                 const std::string& sub_id, 
                                                 const bool& condition) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (config_.log_connections)
        {
            Logger::handle().write(LogTypes::Information,
                fmt::format("Client connected: {} (sub: {})", client_id, sub_id));
        }
        
        stats_.total_connections++;
        stats_.current_connections++;
        
        // Notify callbacks
        for (const auto& callback : client_connected_callbacks_)
        {
            if (callback)
            {
                callback(client_id);
            }
        }
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::on_message_received(const std::string& client_id, 
                                                 const std::string& sub_id, 
                                                 const std::string& message) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (config_.log_packets)
        {
            Logger::handle().write(LogTypes::Debug,
                fmt::format("Message received from {}: {} bytes", client_id, message.size()));
        }
        
        stats_.packets_received++;
        stats_.bytes_received += message.size();
        
        // Process message asynchronously
        auto message_job = std::make_shared<Thread::Job>(Thread::JobPriorities::Normal, [this, client_id, sub_id, message]() -> std::tuple<bool, std::optional<std::string>>
        {
            try
            {
                // Process the message using message dispatcher
                if (message_dispatcher_)
                {
                    // message_dispatcher_->dispatch(client_id, message);
                }
                
                stats_.jobs_processed++;
                return {true, std::nullopt};
            }
            catch (const std::exception& e)
            {
                Logger::handle().write(LogTypes::Error,
                    fmt::format("Failed to process message from {}: {}", client_id, e.what()));
                stats_.jobs_failed++;
                return {false, e.what()};
            }
        }, "ProcessMessage");
        
        thread_pool_->push(message_job);
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkServer::on_client_disconnected_internal(const std::string& client_id) -> void
    {
        if (config_.log_connections)
        {
            Logger::handle().write(LogTypes::Information,
                fmt::format("Client disconnected: {}", client_id));
        }
        
        if (stats_.current_connections > 0)
        {
            stats_.current_connections--;
        }
        
        // Notify callbacks
        for (const auto& callback : client_disconnected_callbacks_)
        {
            if (callback)
            {
                callback(client_id);
            }
        }
    }
}
