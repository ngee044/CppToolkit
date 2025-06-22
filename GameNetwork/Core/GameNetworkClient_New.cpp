#include "GameNetworkClient.h"
#include "../Session/GameSession.h"
#include "../Session/DisconnectionHandler.h"
#include "../GameNetworkConstants.h"

#include <fmt/format.h>
#include <chrono>
#include <thread>

using namespace Utilities;

namespace GameNetwork
{
    GameNetworkClient::GameNetworkClient(const ClientConfig& config)
        : config_(config)
        , network_client_(nullptr)
        , thread_pool_(nullptr)
        , disconnection_handler_(nullptr)
        , current_session_(nullptr)
        , is_connected_(false)
        , is_shutdown_(false)
        , heartbeat_running_(false)
    {
        // Initialize ThreadPool
        auto thread_pool_result = Thread::ThreadPool::create(
            Thread::ThreadPoolConfig{
                .high_priority_threads = DEFAULT_HIGH_PRIORITY_THREADS,
                .normal_priority_threads = DEFAULT_NORMAL_PRIORITY_THREADS,
                .low_priority_threads = DEFAULT_LOW_PRIORITY_THREADS
            }
        );

        if (!thread_pool_result.first)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Failed to create ThreadPool: {}", 
                    thread_pool_result.second.value_or("Unknown error")));
            return;
        }

        thread_pool_ = thread_pool_result.first;

        // Initialize NetworkClient
        Network::ClientConfig network_config;
        network_config.host = config.server_host;
        network_config.port = config.server_port;
        network_config.connectionTimeoutMs = config.connection_timeout_ms;

        network_client_ = std::make_unique<Network::NetworkClient>(network_config);
        
        if (!network_client_)
        {
            Logger::handle().write(LogTypes::Error,
                "Failed to create NetworkClient");
            return;
        }

        // Set up network callbacks
        network_client_->set_connected_callback([this]() {
            on_network_connected();
        });

        network_client_->set_disconnected_callback([this]() {
            on_network_disconnected();
        });

        network_client_->set_message_callback([this](const std::string& message) {
            on_network_message(message);
        });

        network_client_->set_binary_callback([this](const std::vector<uint8_t>& data) {
            on_network_binary(data);
        });

        network_client_->set_error_callback([this](const std::string& error) {
            on_network_error(error);
        });

        // Initialize disconnection handler
        disconnection_handler_ = std::make_shared<DisconnectionHandler>();

        Logger::handle().write(LogTypes::Information,
            "GameNetworkClient initialized successfully");
    }

    GameNetworkClient::~GameNetworkClient()
    {
        disconnect();
        
        if (thread_pool_)
        {
            thread_pool_->shutdown();
        }
    }

    auto GameNetworkClient::connect() -> std::tuple<bool, std::optional<std::string>>
    {
        if (!network_client_)
        {
            return {false, "NetworkClient not initialized"};
        }

        if (is_connected_)
        {
            return {true, std::nullopt};
        }

        Logger::handle().write(LogTypes::Information,
            fmt::format("Connecting to {}:{}", config_.server_host, config_.server_port));

        auto [success, error] = network_client_->connect();
        if (!success)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Connection failed: {}", error.value_or("Unknown error")));
            return {false, error};
        }

        is_connected_ = true;
        start_heartbeat();

        Logger::handle().write(LogTypes::Information,
            "Connected successfully");

        return {true, std::nullopt};
    }

    auto GameNetworkClient::disconnect() -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_connected_)
        {
            return {true, std::nullopt};
        }

        is_shutdown_ = true;
        stop_heartbeat();

        if (network_client_)
        {
            auto [success, error] = network_client_->disconnect();
            if (!success)
            {
                Logger::handle().write(LogTypes::Error,
                    fmt::format("Disconnect warning: {}", error.value_or("Unknown error")));
            }
        }

        is_connected_ = false;

        Logger::handle().write(LogTypes::Information,
            "Disconnected successfully");

        return {true, std::nullopt};
    }

    auto GameNetworkClient::is_connected() const -> bool
    {
        return is_connected_ && network_client_ && network_client_->is_connected();
    }

    auto GameNetworkClient::reconnect() -> std::tuple<bool, std::optional<std::string>>
    {
        if (is_connected_)
        {
            disconnect();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return connect();
    }

    auto GameNetworkClient::send_message(const std::string& message) -> std::tuple<bool, std::optional<std::string>>
    {
        if (!network_client_ || !is_connected_)
        {
            return {false, "Not connected"};
        }

        return network_client_->send_message(message);
    }

    auto GameNetworkClient::send_binary(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
    {
        if (!network_client_ || !is_connected_)
        {
            return {false, "Not connected"};
        }

        return network_client_->send_binary(data);
    }

    auto GameNetworkClient::set_connection_callback(std::function<void(bool)> callback) -> void
    {
        connection_callback_ = std::move(callback);
    }

    auto GameNetworkClient::set_message_callback(std::function<void(const std::string&)> callback) -> void
    {
        message_callback_ = std::move(callback);
    }

    auto GameNetworkClient::set_binary_callback(std::function<void(const std::vector<uint8_t>&)> callback) -> void
    {
        binary_callback_ = std::move(callback);
    }

    auto GameNetworkClient::start_heartbeat() -> void
    {
        if (!thread_pool_ || heartbeat_running_)
        {
            return;
        }

        heartbeat_running_ = true;

        auto heartbeat_job = std::make_shared<Thread::Job>(Thread::JobPriorities::Low, 
            [this]() -> std::tuple<bool, std::optional<std::string>>
        {
            while (heartbeat_running_ && is_connected_)
            {
                perform_heartbeat();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config_.heartbeat_interval_ms));
            }
            return {true, std::nullopt};
        }, "Heartbeat");
        
        thread_pool_->push(heartbeat_job);
    }

    auto GameNetworkClient::stop_heartbeat() -> void
    {
        heartbeat_running_ = false;
    }

    auto GameNetworkClient::perform_heartbeat() -> void
    {
        Logger::handle().write(LogTypes::Debug, "Sending heartbeat");
        // TODO: Implement actual heartbeat packet sending
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

        auto reconnect_job = std::make_shared<Thread::Job>(Thread::JobPriorities::Normal, 
            [this]() -> std::tuple<bool, std::optional<std::string>>
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
        Logger::handle().write(LogTypes::Error,
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

    auto GameNetworkClient::on_network_message(const std::string& message) -> void
    {
        stats_.messages_received++;
        stats_.bytes_received += message.size();

        if (message_callback_)
        {
            message_callback_(message);
        }
    }

    auto GameNetworkClient::on_network_binary(const std::vector<uint8_t>& data) -> void
    {
        stats_.messages_received++;
        stats_.bytes_received += data.size();

        if (binary_callback_)
        {
            binary_callback_(data);
        }
    }

    auto GameNetworkClient::on_network_error(const std::string& error) -> void
    {
        {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = error;
        }

        stats_.error_count++;

        if (connection_callback_)
        {
            connection_callback_(false);
        }
    }

    auto GameNetworkClient::get_session() const -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_session_;
    }

    auto GameNetworkClient::get_last_error() const -> std::string
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        return last_error_;
    }

    auto GameNetworkClient::get_disconnection_handler() const -> std::shared_ptr<DisconnectionHandler>
    {
        return disconnection_handler_;
    }

    auto GameNetworkClient::update_config(const ClientConfig& new_config) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (new_config.server_host.empty())
        {
            return {false, "Server host cannot be empty"};
        }

        if (new_config.server_port == 0)
        {
            return {false, "Server port cannot be 0"};
        }

        config_ = new_config;

        Logger::handle().write(LogTypes::Information,
            "Client configuration updated successfully");

        return {true, std::nullopt};
    }
}
