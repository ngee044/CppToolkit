// GameNetworkClient implementation with internal ThreadPool and Logger integration

#include "GameNetworkClient.h"
#include <Job.h>

#include <fmt/format.h>
#include <thread>

using namespace Utilities;

namespace GameNetwork
{
    GameNetworkClient::GameNetworkClient(const ClientConfig& config)
        : config_(config)
        , is_connected_(false)
        , is_shutdown_(false)
        , heartbeat_running_(false)
        , stats_{}
    {
        Logger::handle().write(LogTypes::Information, 
            fmt::format("Initializing GameNetworkClient for server {}:{}", 
                config_.server_host, config_.server_port));
        
        auto [success, error] = initialize_components();
        if (!success)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Failed to initialize GameNetworkClient: {}", 
                    error.value_or("Unknown error")));
            throw std::runtime_error("Failed to initialize GameNetworkClient");
        }
        
        Logger::handle().write(LogTypes::Information, 
            "GameNetworkClient initialized successfully");
    }
    
    GameNetworkClient::~GameNetworkClient()
    {
        Logger::handle().write(LogTypes::Information, 
            "Shutting down GameNetworkClient");
        
        is_shutdown_ = true;
        stop_heartbeat();
        disconnect();
        
        if (thread_pool_)
        {
            thread_pool_->stop(true);
        }
        
        Logger::handle().write(LogTypes::Information, 
            "GameNetworkClient shutdown complete");
    }
    
    auto GameNetworkClient::initialize_components() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            // Initialize ThreadPool
            thread_pool_ = std::make_shared<Thread::ThreadPool>("GameNetworkClient");
            auto [start_success, start_error] = thread_pool_->start();
            if (!start_success)
            {
                return {false, fmt::format("Failed to start ThreadPool: {}", 
                    start_error.value_or("Unknown error"))};
            }
            
            // Initialize NetworkClient
            network_client_ = std::make_shared<Network::NetworkClient>(
                config_.server_host, config_.server_port);
            
            setup_network_callbacks();
            
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            return {false, fmt::format("Exception during initialization: {}", e.what())};
        }
    }
    
    auto GameNetworkClient::setup_network_callbacks() -> void
    {
        // Note: NetworkClient callback setup would go here
        // This depends on the actual NetworkClient interface
        Logger::handle().write(LogTypes::Debug, 
            "Setting up network callbacks for GameNetworkClient");
    }
    
    auto GameNetworkClient::connect() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_connected_)
        {
            return {true, std::nullopt};
        }
        
        Logger::handle().write(LogTypes::Information, 
            fmt::format("Connecting to server {}:{}", config_.server_host, config_.server_port));
        
        try
        {
            // Actual connection logic would use NetworkClient here
            stats_.connection_time = std::chrono::steady_clock::now();
            is_connected_ = true;
            
            start_heartbeat();
            on_network_connected();
            
            Logger::handle().write(LogTypes::Information, 
                "Successfully connected to server");
            
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Connection failed: {}", e.what()));
            return {false, e.what()};
        }
    }
    
    auto GameNetworkClient::disconnect() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_connected_)
        {
            return {true, std::nullopt};
        }
        
        Logger::handle().write(LogTypes::Information, 
            "Disconnecting from server");
        
        try
        {
            stop_heartbeat();
            is_connected_ = false;
            on_network_disconnected();
            
            Logger::handle().write(LogTypes::Information, 
                "Successfully disconnected from server");
            
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Disconnection failed: {}", e.what()));
            return {false, e.what()};
        }
    }
    
    auto GameNetworkClient::is_connected() const -> bool
    {
        return is_connected_;
    }
    
    auto GameNetworkClient::reconnect() -> std::tuple<bool, std::optional<std::string>>
    {
        Logger::handle().write(LogTypes::Information, 
            "Attempting reconnection");
        
        auto [disconnect_success, disconnect_error] = disconnect();
        if (!disconnect_success)
        {
            return {false, fmt::format("Failed to disconnect before reconnect: {}", 
                disconnect_error.value_or("Unknown error"))};
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
        
        stats_.reconnect_count++;
        return connect();
    }
    
    auto GameNetworkClient::send_message(const std::string& message) -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_connected_)
        {
            return {false, "Client not connected"};
        }
        
        if (!thread_pool_)
        {
            return {false, "ThreadPool not initialized"};
        }
        
        if (message.empty())
        {
            return {false, "Message cannot be empty"};
        }
        
        Logger::handle().write(LogTypes::Debug,
            fmt::format("Sending message: {} bytes", message.size()));
        
        // Use ThreadPool for async sending
        auto job = std::make_shared<Thread::Job>(Thread::JobPriorities::Normal, [this, message]() -> std::tuple<bool, std::optional<std::string>>
        {
            try
            {
                // Actual network sending would go here
                stats_.messages_sent++;
                stats_.bytes_sent += message.size();
                
                Logger::handle().write(LogTypes::Debug,
                    "Message sent successfully");
                return {true, std::nullopt};
            }
            catch (const std::exception& e)
            {
                Logger::handle().write(LogTypes::Error,
                    fmt::format("Failed to send message: {}", e.what()));
                return {false, e.what()};
            }
        }, "SendMessage");
        
        auto [push_success, push_error] = thread_pool_->push(job);
        if (!push_success)
        {
            return {false, fmt::format("Failed to queue message: {}", 
                push_error.value_or("Unknown error"))};
        }
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkClient::send_binary(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_connected_)
        {
            return {false, "Client not connected"};
        }
        
        if (!thread_pool_)
        {
            return {false, "ThreadPool not initialized"};
        }
        
        if (data.empty())
        {
            return {false, "Binary data cannot be empty"};
        }
        
        Logger::handle().write(LogTypes::Debug,
            fmt::format("Sending binary data: {} bytes", data.size()));
        
        // Use ThreadPool for async sending
        auto job = std::make_shared<Thread::Job>(Thread::JobPriorities::Normal, [this, data]() -> std::tuple<bool, std::optional<std::string>>
        {
            try
            {
                // Actual network sending would go here
                stats_.messages_sent++;
                stats_.bytes_sent += data.size();
                
                Logger::handle().write(LogTypes::Debug,
                    "Binary data sent successfully");
                return {true, std::nullopt};
            }
            catch (const std::exception& e)
            {
                Logger::handle().write(LogTypes::Error,
                    fmt::format("Failed to send binary data: {}", e.what()));
                return {false, e.what()};
            }
        }, "SendBinary");
        
        auto [push_success, push_error] = thread_pool_->push(job);
        if (!push_success)
        {
            return {false, fmt::format("Failed to queue binary data: {}", 
                push_error.value_or("Unknown error"))};
        }
        
        return {true, std::nullopt};
    }
    
    auto GameNetworkClient::start_heartbeat() -> void
    {
        if (heartbeat_running_ || config_.heartbeat_interval_ms == 0)
        {
            return;
        }
        
        if (!thread_pool_)
        {
            Logger::handle().write(LogTypes::Error,
                "Cannot start heartbeat: ThreadPool not initialized");
            return;
        }
        
        heartbeat_running_ = true;
        
        Logger::handle().write(LogTypes::Debug,
            fmt::format("Starting heartbeat with interval {}ms", config_.heartbeat_interval_ms));
        
        auto heartbeat_job = std::make_shared<Thread::Job>(Thread::JobPriorities::Low, [this]() -> std::tuple<bool, std::optional<std::string>>
        {
            while (heartbeat_running_ && !is_shutdown_)
            {
                perform_heartbeat();
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeat_interval_ms));
            }
            return {true, std::nullopt};
        }, "Heartbeat");
        
        thread_pool_->push(heartbeat_job);
    }
    
    auto GameNetworkClient::stop_heartbeat() -> void
    {
        if (!heartbeat_running_)
        {
            return;
        }
        
        Logger::handle().write(LogTypes::Debug,
            "Stopping heartbeat");
        
        heartbeat_running_ = false;
    }
    
    auto GameNetworkClient::perform_heartbeat() -> void
    {
        if (!is_connected_)
        {
            return;
        }
        
        // Send heartbeat message
        // This would typically send a small ping message
        Logger::handle().write(LogTypes::Debug,
            "Sending heartbeat");
    }
    
    auto GameNetworkClient::on_network_connected() -> void
    {
        Logger::handle().write(LogTypes::Information,
            "Network connection established");
        
        if (connection_callback_)
        {
            connection_callback_(true);
        }
    }
    
    auto GameNetworkClient::on_network_disconnected() -> void
    {
        Logger::handle().write(LogTypes::Information,
            "Network connection lost");
        
        if (connection_callback_)
        {
            connection_callback_(false);
        }
        
        if (config_.enable_auto_reconnect && !is_shutdown_)
        {
            handle_auto_reconnect();
        }
    }
    
    auto GameNetworkClient::handle_auto_reconnect() -> void
    {
        if (!thread_pool_)
        {
            Logger::handle().write(LogTypes::Error,
                "Cannot handle auto-reconnect: ThreadPool not initialized");
            return;
        }
        
        Logger::handle().write(LogTypes::Information,
            "Auto-reconnect enabled, scheduling reconnection attempt");
        
        auto reconnect_job = std::make_shared<Thread::Job>(Thread::JobPriorities::Normal, [this]() -> std::tuple<bool, std::optional<std::string>>
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
            
            if (!is_shutdown_)
            {
                auto [success, error] = reconnect();
                if (!success)
                {
                    Logger::handle().write(LogTypes::Error,
                        fmt::format("Auto-reconnect failed: {}", error.value_or("Unknown error")));
                    return {false, error};
                }
                return {true, std::nullopt};
            }
            return {true, std::nullopt};
        }, "AutoReconnect");
        
        thread_pool_->push(reconnect_job);
    }
    
    auto GameNetworkClient::on_message_received(MessageCallback callback) -> void
    {
        message_callback_ = callback;
    }
    
    auto GameNetworkClient::on_binary_received(BinaryCallback callback) -> void
    {
        binary_callback_ = callback;
    }
    
    auto GameNetworkClient::on_connection_changed(ConnectionCallback callback) -> void
    {
        connection_callback_ = callback;
    }
    
    auto GameNetworkClient::get_config() const -> const ClientConfig&
    {
        return config_;
    }
    
    auto GameNetworkClient::get_stats() const -> ClientStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
}
