#include "GameSessionManager.h"
#include "GameSession.h"
#include "GameConnection.h"
#include "SessionPersistence.h"
#include "ChannelManager.h"
#include "DisconnectionHandler.h"
#include "Security/SessionSecurityManager.h"
#include <Logger.h>
#include <Converter.h>
#include <Generator.h>

using namespace Utilities;

namespace GameNetwork
{
    // Static member definitions
    std::shared_ptr<GameSessionManager> GameSessionManager::instance_ = nullptr;
    std::mutex GameSessionManager::instance_mutex_;

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
        // shutdown();
    }

    auto GameSessionManager::get_instance() -> std::shared_ptr<GameSessionManager>
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        return instance_;
    }

    auto GameSessionManager::set_instance(std::shared_ptr<GameSessionManager> instance) -> void
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        instance_ = instance;
    }

    auto GameSessionManager::initialize(std::shared_ptr<Thread::ThreadPool> thread_pool) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            thread_pool_ = thread_pool;
            is_running_ = true;
            return std::make_tuple(true, std::optional<std::string>());
        }
        catch (const std::exception& e)
        {
            return std::make_tuple(false, std::optional<std::string>(e.what()));
        }
    }

    auto GameSessionManager::shutdown() -> void
    {
        is_running_ = false;
        
        // Stop worker threads
        if (event_processor_thread_.joinable())
        {
            event_processor_thread_.join();
        }
        
        // Disconnect all sessions
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [session_id, session] : sessions_by_id_)
            {
                if (session)
                {
                    session->disconnect("Server shutting down");
                }
            }
            
            // Clear all session containers
            sessions_by_id_.clear();
            sessions_by_account_.clear();
        }
        
        // Clear event queue
        {
            std::lock_guard<std::mutex> event_lock(event_mutex_);
            while (!event_queue_.empty())
            {
                event_queue_.pop();
            }
        }
        
        Logger::handle().write(LogTypes::Information, "GameSessionManager shutdown complete");
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
            Logger::handle().write(LogTypes::Error,
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
        
        Logger::handle().write(LogTypes::Information,
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
        
        Logger::handle().write(LogTypes::Information,
            "Removed session " + session->session_id());
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
        
        Logger::handle().write(LogTypes::Information,
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
        cleanup_inactive_sessions();
        
        // Security cleanup if enabled
        if (security_manager_)
        {
            auto expired_tokens = security_manager_->cleanup_expired_tokens();
            auto inactive_sessions = security_manager_->cleanup_inactive_sessions();
            
            if (expired_tokens > 0 || inactive_sessions > 0)
            {
                // Utilities::Logger::debug("Security cleanup: " + 
                //     std::to_string(expired_tokens) + " expired tokens, " +
                //     std::to_string(inactive_sessions) + " inactive sessions");
            }
        }
        
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
            Logger::handle().write(LogTypes::Information,
                "Cleaned up " + std::to_string(sessions_to_remove.size()) + " timeout sessions");
        }
        
        return sessions_to_remove.size();
    }

    auto GameSessionManager::log_statistics() -> void
    {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::hours>(now - stats_.start_time);
        
        Logger::handle().write(LogTypes::Information,
            "Session Manager Statistics - Uptime: " + std::to_string(uptime.count()) + " hours, " +
            "Total Sessions: " + std::to_string(stats_.total_sessions_created) + ", " +
            "Active Sessions: " + std::to_string(sessions_by_id_.size()) + ", " +
            "Peak Sessions: " + std::to_string(stats_.peak_concurrent_sessions));
    }

    // Missing method implementations for GameNetworkServerSample
    auto GameSessionManager::get_session_by_id(const std::string& session_id) const -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        auto it = sessions_by_id_.find(session_id);
        if (it != sessions_by_id_.end())
        {
            return it->second;
        }
        return nullptr;
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
        
        // Get idle sessions from security manager
        if (security_manager_)
        {
            auto policy = security_manager_->get_policy();
            auto idle_sessions = security_manager_->get_idle_sessions(policy.idle_timeout);
            sessions_to_remove.insert(sessions_to_remove.end(), idle_sessions.begin(), idle_sessions.end());
        }
        
        // Also check for inactive sessions using existing logic
        for (const auto& [id, session] : sessions_by_id_)
        {
            if (session && !session->is_active())
            {
                if (std::find(sessions_to_remove.begin(), sessions_to_remove.end(), id) == sessions_to_remove.end())
                {
                    sessions_to_remove.push_back(id);
                }
            }
        }
        
        // Remove sessions
        for (const auto& id : sessions_to_remove)
        {
            auto it = sessions_by_id_.find(id);
            if (it != sessions_by_id_.end())
            {
                // Remove from account mapping
                if (auto session = it->second; session)
                {
                    auto account_id = session->get_account_id();
                    sessions_by_account_.erase(account_id);
                    
                    // Unregister from security manager
                    if (security_manager_)
                    {
                        security_manager_->unregister_session(id);
                    }
                    
                    // Clear security token
                    session->clear_session_token();
                }
                sessions_by_id_.erase(it);
            }
        }
        
        // Cleanup expired tokens
        if (security_manager_)
        {
            security_manager_->cleanup_expired_tokens();
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

    auto GameSessionManager::on_network_connected(std::shared_ptr<Network::NetworkSession> network_session, 
                                                 const std::string& account_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!network_session)
        {
            return std::make_tuple(false, std::string("Invalid network session"));
        }
        
        // Create or restore session for the account
        auto [session, error] = create_or_restore_session(account_id);
        if (!session)
        {
            return std::make_tuple(false, error.value_or("Failed to create session"));
        }
        
        // Create GameConnection wrapper for NetworkSession
        auto game_connection = std::make_shared<GameConnection>(network_session);
        
        // Bind game connection to game session
        session->set_network_session(game_connection);
        
        // Update session state
        session->set_state(SessionConnectionState::Connected);
        
        // Notify callbacks
        notify_session_connected(session);
        
        Logger::handle().write(LogTypes::Information,
            "Network session connected for account: " + account_id);
        
        return {true, std::nullopt};
    }

    auto GameSessionManager::create_or_restore_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if session already exists
        auto existing = get_session(account_id);
        if (existing)
        {
            Logger::handle().write(LogTypes::Information,
                "Restoring existing session for account: " + account_id);
            return {existing, std::nullopt};
        }
        
        // Try to load session from persistence
        if (session_persistence_)
        {
            auto [loaded_data, load_error] = session_persistence_->load_session(account_id);
            if (!load_error.has_value() && !loaded_data.session_id.empty())
            {
                // Create session from loaded data
                auto session = std::make_shared<GameSession>(loaded_data.session_id, account_id);
                session->set_account_id(account_id);
                
                // Restore session data
                if (loaded_data.character_id != 0)
                {
                    session->set_character_id(loaded_data.character_id);
                }
                
                // Register session
                sessions_by_id_[session->session_id()] = session;
                sessions_by_account_[account_id] = session;
                
                Logger::handle().write(LogTypes::Information,
                    "Session restored from persistence for account: " + account_id);
                
                return std::make_tuple(session, std::nullopt);
            }
        }
        
        // Create new session
        return create_session(account_id);
    }

    auto GameSessionManager::terminate_session(const std::string& session_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find session
        auto it = sessions_by_id_.find(session_id);
        if (it == sessions_by_id_.end())
        {
            return std::make_tuple(false, std::string("Session not found"));
        }
        
        auto session = it->second;
        auto account_id = session->account_id();
        
        // Disconnect the session
        session->disconnect("Session terminated");
        
        // Remove from containers
        sessions_by_id_.erase(it);
        
        if (!account_id.empty())
        {
            sessions_by_account_.erase(account_id);
        }
        
        // Update stats
        stats_.total_disconnections++;
        
        // Notify callbacks
        for (const auto& callback : session_disconnected_callbacks)
        {
            if (callback)
            {
                callback(session);
            }
        }
        
        Logger::handle().write(LogTypes::Information,
            "Session terminated: " + session_id);
        
        return std::make_tuple(true, std::nullopt);
    }

    auto GameSessionManager::update_peak_sessions() -> void
    {
        size_t current_sessions = sessions_by_id_.size();
        if (current_sessions > stats_.peak_concurrent_sessions)
        {
            stats_.peak_concurrent_sessions = current_sessions;
            Logger::handle().write(LogTypes::Information,
                "New peak concurrent sessions: " + std::to_string(current_sessions));
        }
    }

    auto GameSessionManager::notify_session_connected(std::shared_ptr<GameSession> session) -> void
    {
        if (!session) return;
        
        // Update stats
        stats_.total_connections++;
        update_peak_sessions();
        
        // Call registered callbacks
        for (const auto& callback : session_connected_callbacks)
        {
            if (callback)
            {
                callback(session);
            }
        }
        
        // Queue session connected event
        {
            std::lock_guard<std::mutex> event_lock(event_mutex_);
            SessionEvent event;
            event.type = GameSessionManager::SessionEventType::Connected;
            event.session = session;
            event.timestamp = std::chrono::steady_clock::now();
            event_queue_.push(std::move(event));
        }
        event_cv_.notify_one();
    }
}
