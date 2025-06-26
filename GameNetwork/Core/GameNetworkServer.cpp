#include "GameNetworkServer.h"
#include <GameSessionManager.h>
#include <PacketProcessor.h>
#include <MessageDispatcher.h>
#include <WorldSynchronizer.h>
#include <LoadBalancer.h>
#include <ServerMonitor.h>
#include <Logger.h>
#include <Converter.h>
#include <Job.h>
#include <chrono>

using namespace Utilities;

namespace GameNetwork
{
    GameNetworkServer::GameNetworkServer(const ServerConfig& config)
        : config_(config)
        , is_running_(false)
        , is_monitoring_(false)
    {
        stats_.total_connections = 0;
        stats_.current_connections = 0;
        stats_.packets_sent = 0;
        stats_.packets_received = 0;
        stats_.bytes_sent = 0;
        stats_.bytes_received = 0;
        stats_.start_time = std::chrono::steady_clock::now();
        stats_.jobs_processed = 0;
        stats_.jobs_failed = 0;
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
            return { false, "Server is already running" };
        }

        // Initialize components
        auto [init_success, init_error] = initialize_components();
        if (!init_success)
        {
            return { false, init_error };
        }

        // Setup network callbacks
        setup_network_callbacks();

        // Start network server
        auto [start_success, start_error] = network_server_->start(config_.port, config_.socket_buffer_size);
        if (!start_success)
        {
            return { false, start_error };
        }

        is_running_ = true;
        
        // Start monitoring
        if (server_monitor_)
        {
            start_monitoring();
        }

        // Call callbacks
        for (auto& callback : server_started_callbacks_)
        {
            auto wrapped_callback = [callback]() -> std::tuple<bool, std::optional<std::string>> {
                callback();
                return { true, std::nullopt };
            };
            auto job = std::make_shared<Thread::Job>(Thread::JobPriorities::Low, wrapped_callback);
            thread_pool_->add_job(job);
        }

        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "GameNetworkServer started on port " + std::to_string(config_.port));

        return { true, std::nullopt };
    }

    auto GameNetworkServer::stop() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_running_)
        {
            return { true, std::nullopt };
        }

        // Stop monitoring
        stop_monitoring();

        // Stop network server
        if (network_server_)
        {
            network_server_->stop();
        }

        // Disconnect all sessions
        if (session_manager_)
        {
            session_manager_->disconnect_all();
        }

        // Stop thread pool
        if (thread_pool_)
        {
            thread_pool_->stop();
        }

        is_running_ = false;

        // Call callbacks
        for (auto& callback : server_stopped_callbacks_)
        {
            callback();
        }

        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "GameNetworkServer stopped");

        return { true, std::nullopt };
    }

    auto GameNetworkServer::initialize_components() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            // Initialize thread pool
            thread_pool_ = std::make_shared<Thread::ThreadPool>();
            // Note: ThreadPool priority count configuration not available in current implementation
            thread_pool_->start();

            // Initialize network server
            network_server_ = std::make_shared<Network::NetworkServer>(config_.server_id);

            // Initialize session manager
            session_manager_ = std::make_shared<GameSessionManager>(config_.max_players, config_.max_channels);
            session_manager_->set_thread_pool(thread_pool_);

            // Initialize packet processor
            packet_processor_ = std::make_shared<PacketProcessor>();
            packet_processor_->set_encryption_enabled(config_.enable_encryption);
            packet_processor_->set_compression_enabled(config_.enable_compression);

            // Initialize message dispatcher
            message_dispatcher_ = std::make_shared<MessageDispatcher>();
            message_dispatcher_->set_thread_pool(thread_pool_);

            // Initialize world synchronizer
            world_synchronizer_ = std::make_shared<WorldSynchronizer>();
            world_synchronizer_->set_thread_pool(thread_pool_);

            // Initialize load balancer
            load_balancer_ = std::make_shared<LoadBalancer>();
            load_balancer_->set_server_id(config_.server_id);

            // Initialize server monitor
            server_monitor_ = std::make_shared<ServerMonitor>();
            server_monitor_->set_server_id(config_.server_id);

            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Failed to initialize components: ") + e.what() };
        }
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

        // Note: NetworkServer doesn't have a disconnected callback method in current API
        // Disconnection will be handled through other mechanisms
    }

    auto GameNetworkServer::on_client_connected(const std::string& client_id, 
                                                const std::string& sub_id, 
                                                const bool& condition) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }

        // Create new session
        auto [session, session_error] = session_manager_->create_session(client_id);
        if (!session)
        {
            return { false, session_error ? *session_error : "Failed to create session" };
        }

        // Update statistics
        stats_.total_connections++;
        stats_.current_connections++;

        // Log connection
        if (config_.log_connections)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Client connected: " + client_id);
        }

        // Call callbacks
        for (auto& callback : client_connected_callbacks_)
        {
            auto job_callback = [callback, client_id]() -> std::tuple<bool, std::optional<std::string>> {
                callback(client_id);
                return { true, std::nullopt };
            };
            auto job = std::make_shared<Thread::Job>(Thread::JobPriorities::Low, job_callback);
            thread_pool_->add_job(job);
        }

        return { true, std::nullopt };
    }

    auto GameNetworkServer::on_message_received(const std::string& client_id, 
                                               const std::string& sub_id, 
                                               const std::string& message) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }

        // Get session
        auto session = session_manager_->get_session(client_id);
        if (!session)
        {
            return { false, "Session not found" };
        }

        // Update statistics
        stats_.packets_received++;
        stats_.bytes_received += message.length();

        // Process packet in thread pool
        auto job_callback = [this, session, message]() -> std::tuple<bool, std::optional<std::string>>
        {
            try
            {
                // Deserialize packet
                auto packet = packet_processor_->deserialize(message);
                if (!packet)
                {
                    Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                        "Failed to deserialize packet from " + session->session_id());
                    return { false, "Failed to deserialize packet" };
                }

                // Update session activity
                session->update_last_activity();

                // Dispatch message
                message_dispatcher_->dispatch(session, *packet);

                stats_.jobs_processed++;
                return { true, std::nullopt };
            }
            catch (const std::exception& e)
            {
                stats_.jobs_failed++;
                Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                    "Error processing packet: " + std::string(e.what()));
                return { false, std::string("Error processing packet: ") + e.what() };
            }
        };
        auto job = std::make_shared<Thread::Job>(Thread::JobPriorities::High, job_callback);
        thread_pool_->add_job(job);

        return { true, std::nullopt };
    }

    auto GameNetworkServer::on_binary_received(const std::string& client_id, 
                                              const std::string& sub_id, 
                                              const std::string& message, 
                                              const std::vector<uint8_t>& data) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }

        // Get session
        auto session = session_manager_->get_session(client_id);
        if (!session)
        {
            return { false, "Session not found" };
        }

        // Update statistics
        stats_.packets_received++;
        stats_.bytes_received += data.size();

        // Process binary data in thread pool
        auto job_callback = [this, session, data]() -> std::tuple<bool, std::optional<std::string>>
        {
            try
            {
                // Deserialize binary packet
                auto packet = packet_processor_->deserialize_binary(data);
                if (!packet)
                {
                    Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                        "Failed to deserialize binary packet from " + session->session_id());
                    return { false, "Failed to deserialize binary packet" };
                }

                // Update session activity
                session->update_last_activity();

                // Dispatch message
                message_dispatcher_->dispatch(session, *packet);

                stats_.jobs_processed++;
                return { true, std::nullopt };
            }
            catch (const std::exception& e)
            {
                stats_.jobs_failed++;
                Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                    "Error processing binary packet: " + std::string(e.what()));
                return { false, std::string("Error processing binary packet: ") + e.what() };
            }
        };
        auto job = std::make_shared<Thread::Job>(Thread::JobPriorities::High, job_callback);
        thread_pool_->add_job(job);

        return { true, std::nullopt };
    }

    auto GameNetworkServer::on_client_disconnected_internal(const std::string& client_id) -> void
    {
        if (!is_running_)
        {
            return;
        }

        // Process disconnection in thread pool
        auto job_callback = [this, client_id]() -> std::tuple<bool, std::optional<std::string>>
        {
            // Remove session
            session_manager_->remove_session(client_id);

            // Update statistics
            if (stats_.current_connections > 0)
            {
                stats_.current_connections--;
            }

            // Log disconnection
            if (config_.log_connections)
            {
                Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                    "Client disconnected: " + client_id);
            }

            // Call callbacks
            for (auto& callback : client_disconnected_callbacks_)
            {
                callback(client_id);
            }
            return { true, std::nullopt };
        };
        auto job = std::make_shared<Thread::Job>(Thread::JobPriorities::Normal, job_callback);
        thread_pool_->add_job(job);
    }

    auto GameNetworkServer::broadcast_to_all(const GamePacket& packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }

        auto sessions = session_manager_->get_all_sessions();
        auto serialized = packet_processor_->serialize(packet);
        
        if (!serialized)
        {
            return { false, "Failed to serialize packet" };
        }

        for (const auto& session : sessions)
        {
            // Convert string to vector<uint8_t>
            std::vector<uint8_t> binary_data(serialized->begin(), serialized->end());
            network_server_->send_binary(binary_data, "", session->session_id());
            stats_.packets_sent++;
            stats_.bytes_sent += serialized->size();
        }

        return { true, std::nullopt };
    }

    auto GameNetworkServer::broadcast_to_channel(uint32_t channel_id, const GamePacket& packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_running_)
        {
            return { false, "Server is not running" };
        }

        auto sessions = session_manager_->get_channel_sessions(channel_id);
        auto serialized = packet_processor_->serialize(packet);
        
        if (!serialized)
        {
            return { false, "Failed to serialize packet" };
        }

        for (const auto& session : sessions)
        {
            // Convert string to vector<uint8_t>
            std::vector<uint8_t> binary_data(serialized->begin(), serialized->end());
            network_server_->send_binary(binary_data, "", session->session_id());
            stats_.packets_sent++;
            stats_.bytes_sent += serialized->size();
        }

        return { true, std::nullopt };
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

    auto GameNetworkServer::get_thread_pool() -> std::shared_ptr<Thread::ThreadPool>
    {
        return thread_pool_;
    }

    // Missing method implementations for GameNetworkServerSample
    auto GameNetworkServer::get_stats() const -> ServerStats
    {
        return stats_;
    }

    auto GameNetworkServer::start_monitoring() -> void
    {
        is_monitoring_ = true;
        if (server_monitor_)
        {
            // Start monitoring logic
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Server monitoring started");
        }
    }

    auto GameNetworkServer::stop_monitoring() -> void
    {
        is_monitoring_ = false;
        if (server_monitor_)
        {
            // Stop monitoring logic
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Server monitoring stopped");
        }
    }
}
