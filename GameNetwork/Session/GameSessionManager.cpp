#include "GameSessionManager.h"
#include <GameConnection.h>
#include <SessionPersistence.h>
#include <ChannelManager.h>
#include <DisconnectionHandler.h>
#include <Logger.h>
#include <Converter.h>
#include <Generator.h>

namespace GameNetwork
{
    GameSessionManager::GameSessionManager(uint32_t max_players, uint32_t max_channels)
        : max_players_(max_players)
        , max_channels_(max_channels)
        , is_running_(false)
    {
        stats_.total_sessions_created = 0;
        stats_.total_sessions_terminated = 0;
        stats_.peak_concurrent_sessions = 0;
        stats_.start_time = std::chrono::steady_clock::now();
        stats_.total_migrations = 0;
        stats_.failed_migrations = 0;
        stats_.average_session_duration = std::chrono::seconds(0);
    }

    GameSessionManager::~GameSessionManager()
    {
        shutdown();
    }

    auto GameSessionManager::initialize(std::shared_ptr<Thread::ThreadPool> thread_pool) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_running_)
        {
            return { false, "Session manager already initialized" };
        }

        try
        {
            // Initialize channel manager
            channel_manager_ = std::make_unique<ChannelManager>(max_channels_);
            
            // Initialize session persistence
            session_persistence_ = std::make_unique<SessionPersistence>();
            auto [persist_success, persist_error] = session_persistence_->initialize();
            if (!persist_success)
            {
                return { false, persist_error };
            }
            
            // Initialize disconnection handler
            disconnection_handler_ = std::make_unique<DisconnectionHandler>();
            // Note: DisconnectionHandler might need session manager reference
            
            // Start maintenance thread
            is_running_ = true;
            maintenance_thread_ = std::thread(&GameSessionManager::maintenance_loop, this);
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "GameSessionManager initialized successfully");
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Failed to initialize session manager: ") + e.what() };
        }
    }

    auto GameSessionManager::shutdown() -> void
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            if (!is_running_)
            {
                return;
            }
            
            is_running_ = false;
        }
        
        // Wait for maintenance thread
        if (maintenance_thread_.joinable())
        {
            maintenance_cv_.notify_all();
            maintenance_thread_.join();
        }
        
        // Disconnect all sessions
        disconnect_all();
        
        // Cleanup
        sessions_by_id_.clear();
        sessions_by_account_.clear();
        connections_by_id_.clear();
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "GameSessionManager shut down");
    }

    auto GameSessionManager::create_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if session already exists
        auto existing = get_session(account_id);
        if (existing)
        {
            return { existing, std::nullopt };
        }
        
        // Check max players limit
        if (sessions_by_id_.size() >= max_players_)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                "Maximum player limit reached: " + std::to_string(max_players_));
            return { nullptr, "Maximum player limit reached" };
        }
        
        // Generate session ID
        std::string session_id = Utilities::Generator::guid();
        
        // Create new session
        auto session = std::make_shared<GameSession>(session_id, account_id);
        
        // Store session
        sessions_by_id_[session_id] = session;
        sessions_by_account_[account_id] = session;
        
        // Update statistics
        stats_.total_sessions_created++;
        if (sessions_by_id_.size() > stats_.peak_concurrent_sessions)
        {
            stats_.peak_concurrent_sessions = sessions_by_id_.size();
        }
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Created session " + session_id + " for connection");
        
        return { session, std::nullopt };
    }

    auto GameSessionManager::get_session(const std::string& client_id) const 
        -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // First try by account ID
        auto account_it = sessions_by_account_.find(client_id);
        if (account_it != sessions_by_account_.end())
        {
            return account_it->second;
        }
        
        // Then try by session ID
        auto session_it = sessions_by_id_.find(client_id);
        if (session_it != sessions_by_id_.end())
        {
            return session_it->second;
        }
        
        return nullptr;
    }

    auto GameSessionManager::remove_session(const std::string& client_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto session = get_session(client_id);
        if (!session)
        {
            return;
        }
        
        // Remove from channel if in one
        if (session->current_channel_id() != 0)
        {
            channel_manager_->leave_channel(session->current_channel_id(), session);
        }
        
        // Save session state before removal
        if (session_persistence_)
        {
            session->save_state();
        }
        
        // Remove from maps
        sessions_by_id_.erase(session->session_id());
        sessions_by_account_.erase(session->account_id());
        
        // Update statistics
        stats_.total_sessions_terminated++;
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Removed session " + session->session_id());
    }

    auto GameSessionManager::get_all_sessions() const -> std::vector<std::shared_ptr<GameSession>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::shared_ptr<GameSession>> sessions;
        sessions.reserve(sessions_by_id_.size());
        
        for (const auto& [id, session] : sessions_by_id_)
        {
            sessions.push_back(session);
        }
        
        return sessions;
    }

    auto GameSessionManager::get_channel_sessions(uint32_t channel_id) const 
        -> std::vector<std::shared_ptr<GameSession>>
    {
        return channel_manager_->get_channel_sessions(channel_id);
    }

    auto GameSessionManager::disconnect_all() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [id, session] : sessions_by_id_)
        {
            session->unbind_connection();
        }
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Disconnected all sessions");
    }

    auto GameSessionManager::set_thread_pool(std::shared_ptr<Thread::ThreadPool> thread_pool) -> void
    {
        thread_pool_ = thread_pool;
    }

    auto GameSessionManager::maintenance_loop() -> void
    {
        while (is_running_)
        {
            {
                std::unique_lock<std::mutex> lock(maintenance_mutex_);
                maintenance_cv_.wait_for(lock, kMaintenanceInterval,
                    [this] { return !is_running_; });
            }
            
            if (!is_running_)
            {
                break;
            }
            
            perform_maintenance();
        }
    }

    auto GameSessionManager::perform_maintenance() -> void
    {
        cleanup_timeout_sessions();
        
        // Log statistics periodically
        static auto last_log_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (now - last_log_time > std::chrono::minutes(5))
        {
            log_statistics();
            last_log_time = now;
        }
    }

    auto GameSessionManager::cleanup_timeout_sessions() -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> sessions_to_remove;
        
        for (const auto& [id, session] : sessions_by_id_)
        {
            if (session->is_timeout())
            {
                sessions_to_remove.push_back(id);
            }
        }
        
        for (const auto& id : sessions_to_remove)
        {
            remove_session(id);
        }
        
        if (!sessions_to_remove.empty())
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Cleaned up " + std::to_string(sessions_to_remove.size()) + " timeout sessions");
        }
        
        return sessions_to_remove.size();
    }

    auto GameSessionManager::log_statistics() -> void
    {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::hours>(now - stats_.start_time);
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Session Manager Statistics - Uptime: " + std::to_string(uptime.count()) + " hours, " +
            "Total Sessions: " + std::to_string(stats_.total_sessions_created) + ", " +
            "Active Sessions: " + std::to_string(sessions_by_id_.size()) + ", " +
            "Peak Sessions: " + std::to_string(stats_.peak_concurrent_sessions));
    }

    // Missing method implementations for GameNetworkServerSample
    auto GameSessionManager::get_session_by_id(const std::string& session_id) const -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_by_id_.find(session_id);
        return (it != sessions_by_id_.end()) ? it->second : nullptr;
    }

    auto GameSessionManager::get_sessions_in_channel(uint32_t channel_id) const -> std::vector<std::shared_ptr<GameSession>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<GameSession>> result;
        
        for (const auto& [id, session] : sessions_by_id_)
        {
            if (session && session->get_channel_id() == channel_id)
            {
                result.push_back(session);
            }
        }
        
        return result;
    }

    auto GameSessionManager::active_session_count() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_by_id_.size();
    }

    auto GameSessionManager::online_session_count() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        
        for (const auto& [id, session] : sessions_by_id_)
        {
            if (session && session->is_connected())
            {
                ++count;
            }
        }
        
        return count;
    }

    auto GameSessionManager::cleanup_inactive_sessions() -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> sessions_to_remove;
        
        for (const auto& [id, session] : sessions_by_id_)
        {
            if (session && !session->is_active())
            {
                sessions_to_remove.push_back(id);
            }
        }
        
        for (const auto& id : sessions_to_remove)
        {
            auto it = sessions_by_id_.find(id);
            if (it != sessions_by_id_.end())
            {
                // Remove from account mapping
                if (auto session = it->second; session)
                {
                    sessions_by_account_.erase(session->get_account_id());
                }
                sessions_by_id_.erase(it);
            }
        }
        
        return sessions_to_remove.size();
    }

    auto GameSessionManager::cleanup_timeout_connections() -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> connections_to_remove;
        
        for (const auto& [id, connection] : connections_by_id_)
        {
            if (connection && connection->is_timeout())
            {
                connections_to_remove.push_back(id);
            }
        }
        
        for (const auto& id : connections_to_remove)
        {
            connections_by_id_.erase(id);
            connection_to_session_.erase(id);
        }
        
        return connections_to_remove.size();
    }

    // Callback storage for session events
    static std::vector<std::function<void(std::shared_ptr<GameSession>)>> session_connected_callbacks;
    static std::vector<std::function<void(std::shared_ptr<GameSession>)>> session_disconnected_callbacks;

    auto GameSessionManager::on_session_connected(SessionCallback callback) -> void
    {
        session_connected_callbacks.push_back(callback);
    }

    auto GameSessionManager::on_session_disconnected(SessionCallback callback) -> void
    {
        session_disconnected_callbacks.push_back(callback);
    }
}
