#include "GameSessionManager.h"
#include "GameConnection.h"
#include <Job.h>

#include <fmt/format.h>
#include <thread>

#include <random>
#include <sstream>
#include <iomanip>

using namespace Utilities;

namespace GameNetwork
{
    GameSessionManager::GameSessionManager()
        : is_initialized_(false)
        , is_shutdown_(false)
        , cleanup_running_(false)
        , statistics_{}
    {
        Logger::handle().write(LogTypes::Information,
            "GameSessionManager created");
        
        statistics_.start_time = std::chrono::steady_clock::now();
    }
    
    GameSessionManager::~GameSessionManager()
    {
        Logger::handle().write(LogTypes::Information,
            "GameSessionManager shutting down");
        
        shutdown();
    }
    
    auto GameSessionManager::initialize(std::shared_ptr<Thread::ThreadPool> thread_pool) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_initialized_)
        {
            return {true, std::nullopt};
        }
        
        if (!thread_pool)
        {
            return {false, "ThreadPool cannot be null"};
        }
        
        thread_pool_ = thread_pool;
        is_initialized_ = true;
        
        start_cleanup_timer();
        
        Logger::handle().write(LogTypes::Information,
            "GameSessionManager initialized successfully");
        
        return {true, std::nullopt};
    }
    
    auto GameSessionManager::shutdown() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_shutdown_)
        {
            return;
        }
        
        is_shutdown_ = true;
        stop_cleanup_timer();
        
        // Terminate all active sessions
        auto session_count = sessions_by_id_.size();
        sessions_by_id_.clear();
        sessions_by_account_.clear();
        connections_.clear();
        connection_to_session_.clear();
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("GameSessionManager shutdown complete. {} sessions terminated", 
                session_count));
    }
    
    auto GameSessionManager::create_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_initialized_)
        {
            return {nullptr, "GameSessionManager not initialized"};
        }
        
        if (account_id.empty())
        {
            return {nullptr, "Account ID cannot be empty"};
        }
        
        // Check if session already exists
        auto it = sessions_by_account_.find(account_id);
        if (it != sessions_by_account_.end())
        {
            Logger::handle().write(LogTypes::Debug,
                fmt::format("Session already exists for account: {}", account_id));
            return {nullptr, "Session already exists for account: " + account_id};
        }
        
        // Generate new session ID
        auto session_id = generate_session_id();
        
        // Create new session
        auto session = std::make_shared<GameSession>(session_id, account_id);
        
        // Store session
        sessions_by_id_[session_id] = session;
        sessions_by_account_[account_id] = session;
        
        // Update statistics
        statistics_.total_sessions_created++;
        update_peak_sessions();
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("Created new session {} for account {}", session_id, account_id));
        
        // Notify callbacks
        notify_session_created(session);
        
        return {session, std::nullopt};
    }
    
    auto GameSessionManager::restore_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_initialized_)
        {
            return {nullptr, "GameSessionManager not initialized"};
        }
        
        if (account_id.empty())
        {
            return {nullptr, "Account ID cannot be empty"};
        }
        
        auto it = sessions_by_account_.find(account_id);
        if (it == sessions_by_account_.end())
        {
            Logger::handle().write(LogTypes::Debug,
                fmt::format("No session found for account: {}", account_id));
            return {nullptr, "No session found for account: " + account_id};
        }
        
        auto session = it->second;
        if (!session)
        {
            return {nullptr, "Session object is null"};
        }
        
        // Restore session state
        auto [success, error] = session->restore_state();
        if (!success)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Failed to restore session for account {}: {}", 
                    account_id, error.value_or("Unknown error")));
            return {nullptr, error};
        }
        
        session->set_state(SessionState::Active);
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("Restored session {} for account {}", session->session_id(), account_id));
        
        return {session, std::nullopt};
    }
    
    auto GameSessionManager::create_or_restore_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        if (account_id.empty())
        {
            return {nullptr, "Account ID cannot be empty"};
        }
        
        // Try to restore existing session first
        auto [session, restore_error] = restore_session(account_id);
        if (session)
        {
            Logger::handle().write(LogTypes::Debug,
                fmt::format("Restored existing session for account: {}", account_id));
            return {session, std::nullopt};
        }
        
        // Create new session if restore failed
        Logger::handle().write(LogTypes::Debug,
            fmt::format("Creating new session for account: {} (restore failed: {})", 
                account_id, restore_error.value_or("Unknown error")));
        
        return create_session(account_id);
    }
    
    auto GameSessionManager::terminate_session(const std::string& session_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_initialized_)
        {
            return {false, "GameSessionManager not initialized"};
        }
        
        if (session_id.empty())
        {
            return {false, "Session ID cannot be empty"};
        }
        
        auto it = sessions_by_id_.find(session_id);
        if (it == sessions_by_id_.end())
        {
            Logger::handle().write(LogTypes::Debug,
                fmt::format("Session not found: {}", session_id));
            return {false, "Session not found: " + session_id};
        }
        
        auto session = it->second;
        if (!session)
        {
            return {false, "Session object is null"};
        }
        
        auto account_id = session->account_id();
        
        // Remove from storage
        sessions_by_id_.erase(it);
        sessions_by_account_.erase(account_id);
        
        // Update statistics
        statistics_.total_sessions_terminated++;
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("Terminated session {} for account {}", session_id, account_id));
        
        // Notify callbacks
        notify_session_terminated(session);
        
        return {true, std::nullopt};
    }
    
    auto GameSessionManager::on_network_connected(std::shared_ptr<Network::NetworkSession> network_session, 
                                                   const std::string& account_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!network_session)
        {
            return {false, "Invalid network session"};
        }
        
        // Create game connection
        auto connection_id = generate_session_id(); // Use our own session ID generator
        auto game_connection = std::make_shared<GameConnection>(connection_id);
        game_connection->attach_network_session(network_session);
        
        // Authenticate connection
        auto [auth_success, auth_error] = game_connection->authenticate(account_id, ""); // TODO: Add token
        if (!auth_success)
        {
            return {false, auth_error};
        }
        
        // Create or restore session
        auto [session, session_error] = create_or_restore_session(account_id);
        if (!session)
        {
            return {false, session_error};
        }
        
        // Bind connection to session
        session->bind_connection(game_connection);
        
        // Store connection
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_[connection_id] = game_connection;
            connection_to_session_[connection_id] = session->session_id();
        }
        
        // Trigger callbacks
        for (const auto& callback : session_connected_callbacks_)
        {
            callback(session);
        }
        
        return {true, std::nullopt};
    }
    
    auto GameSessionManager::get_session_by_id(const std::string& session_id) const 
        -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (session_id.empty())
        {
            return nullptr;
        }
        
        auto it = sessions_by_id_.find(session_id);
        return (it != sessions_by_id_.end()) ? it->second : nullptr;
    }
    
    auto GameSessionManager::get_session_by_account(const std::string& account_id) const 
        -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (account_id.empty())
        {
            return nullptr;
        }
        
        auto it = sessions_by_account_.find(account_id);
        return (it != sessions_by_account_.end()) ? it->second : nullptr;
    }
    
    auto GameSessionManager::get_connection_by_id(const std::string& connection_id) const 
        -> std::shared_ptr<GameConnection>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (connection_id.empty())
        {
            return nullptr;
        }
        
        auto it = connections_.find(connection_id);
        return (it != connections_.end()) ? it->second : nullptr;
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
            if (session && session->is_online())
            {
                count++;
            }
        }
        
        return count;
    }
    
    auto GameSessionManager::suspended_session_count() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = 0;
        for (const auto& [id, session] : sessions_by_id_)
        {
            if (session && session->state() == SessionState::Suspended)
            {
                count++;
            }
        }
        
        return count;
    }
    
    auto GameSessionManager::migrate_session(const std::string& session_id, const std::string& target_server) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (session_id.empty() || target_server.empty())
        {
            return {false, "Session ID and target server cannot be empty"};
        }
        
        // Implementation would depend on distributed system architecture
        Logger::handle().write(LogTypes::Information,
            fmt::format("Session migration requested from {} to {}", session_id, target_server));
        
        return {false, "Session migration not implemented"};
    }
    
    auto GameSessionManager::cleanup_inactive_sessions() -> size_t
    {
        if (!is_initialized_)
        {
            return 0;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t cleaned_count = 0;
        std::vector<std::string> sessions_to_remove;
        
        for (const auto& [id, session] : sessions_by_id_)
        {
            if (!session)
            {
                sessions_to_remove.push_back(id);
                continue;
            }
            
            // Check if session is inactive for too long
            auto last_activity = session->last_activity_time();
            auto now = std::chrono::steady_clock::now();
            auto inactive_duration = std::chrono::duration_cast<std::chrono::minutes>(now - last_activity);
            
            if (inactive_duration.count() > 30) // 30 minutes inactive
            {
                sessions_to_remove.push_back(id);
            }
        }
        
        for (const auto& session_id : sessions_to_remove)
        {
            auto it = sessions_by_id_.find(session_id);
            if (it != sessions_by_id_.end())
            {
                auto session = it->second;
                if (session)
                {
                    auto account_id = session->account_id();
                    sessions_by_account_.erase(account_id);
                    
                    Logger::handle().write(LogTypes::Debug,
                        fmt::format("Cleaned up inactive session {} for account {}", 
                            session_id, account_id));
                }
                sessions_by_id_.erase(it);
                cleaned_count++;
            }
        }
        
        statistics_.sessions_cleaned += cleaned_count;
        
        if (cleaned_count > 0)
        {
            Logger::handle().write(LogTypes::Information,
                fmt::format("Cleaned up {} inactive sessions", cleaned_count));
        }
        
        return cleaned_count;
    }
    
    auto GameSessionManager::cleanup_timeout_connections() -> size_t
    {
        if (!is_initialized_)
        {
            return 0;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t cleaned_count = 0;
        std::vector<std::string> connections_to_remove;
        
        for (const auto& [id, connection] : connections_)
        {
            if (!connection || !connection->is_connected())
            {
                connections_to_remove.push_back(id);
            }
        }
        
        for (const auto& connection_id : connections_to_remove)
        {
            connections_.erase(connection_id);
            connection_to_session_.erase(connection_id);
            cleaned_count++;
        }
        
        if (cleaned_count > 0)
        {
            Logger::handle().write(LogTypes::Information,
                fmt::format("Cleaned up {} timeout connections", cleaned_count));
        }
        
        return cleaned_count;
    }
    
    auto GameSessionManager::cleanup_stale_sessions() -> size_t
    {
        return cleanup_inactive_sessions() + cleanup_timeout_connections();
    }
    
    auto GameSessionManager::perform_maintenance() -> void
    {
        if (!is_initialized_ || is_shutdown_)
        {
            return;
        }
        
        auto cleaned_sessions = cleanup_inactive_sessions();
        auto cleaned_connections = cleanup_timeout_connections();
        
        statistics_.cleanup_runs++;
        
        if (cleaned_sessions > 0 || cleaned_connections > 0)
        {
            Logger::handle().write(LogTypes::Information,
                fmt::format("Maintenance completed: {} sessions, {} connections cleaned", 
                    cleaned_sessions, cleaned_connections));
        }
    }
    
    auto GameSessionManager::get_statistics() const -> SessionStatistics
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return statistics_;
    }
    
    auto GameSessionManager::reset_statistics() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        statistics_ = {};
        statistics_.start_time = std::chrono::steady_clock::now();
        
        Logger::handle().write(LogTypes::Information,
            "Session statistics reset");
    }
    
    auto GameSessionManager::log_status() -> void
    {
        auto stats = get_statistics();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - stats.start_time).count();
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("SessionManager Status - Uptime: {}s, Active: {}, Online: {}, "
                       "Suspended: {}, Total Created: {}, Peak: {}", 
                uptime, active_session_count(), online_session_count(),
                suspended_session_count(), stats.total_sessions_created, 
                stats.peak_concurrent_sessions));
    }
    
    auto GameSessionManager::on_session_created(SessionCallback callback) -> void
    {
        session_created_callbacks_.push_back(callback);
    }
    
    auto GameSessionManager::on_session_terminated(SessionCallback callback) -> void
    {
        session_terminated_callbacks_.push_back(callback);
    }
    
    auto GameSessionManager::on_session_connected(SessionCallback callback) -> void
    {
        session_connected_callbacks_.push_back(callback);
    }
    
    auto GameSessionManager::on_session_disconnected(SessionCallback callback) -> void
    {
        session_disconnected_callbacks_.push_back(callback);
    }
    
    auto GameSessionManager::generate_session_id() const -> std::string
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 32; ++i)
        {
            ss << dis(gen);
        }
        return ss.str();
    }
    
    auto GameSessionManager::start_cleanup_timer() -> void
    {
        if (!thread_pool_)
        {
            Logger::handle().write(LogTypes::Error,
                "Cannot start cleanup timer: ThreadPool not available");
            return;
        }
        
        cleanup_running_ = true;
        
        auto cleanup_job = std::make_shared<Thread::Job>(Thread::JobPriorities::Low, [this]() -> std::tuple<bool, std::optional<std::string>>
        {
            while (cleanup_running_ && !is_shutdown_)
            {
                perform_maintenance();
                std::this_thread::sleep_for(std::chrono::minutes(5)); // Run every 5 minutes
            }
            return {true, std::nullopt};
        }, "SessionCleanup");
        
        thread_pool_->push(cleanup_job);
        
        Logger::handle().write(LogTypes::Debug,
            "Cleanup timer started");
    }
    
    auto GameSessionManager::stop_cleanup_timer() -> void
    {
        cleanup_running_ = false;
        
        Logger::handle().write(LogTypes::Debug,
            "Cleanup timer stopped");
    }
    
    auto GameSessionManager::schedule_cleanup() -> void
    {
        if (!thread_pool_)
        {
            return;
        }
        
        auto cleanup_job = std::make_shared<Thread::Job>(
            Thread::JobPriorities::Low, 
            [this]() -> std::tuple<bool, std::optional<std::string>>
            {
                perform_maintenance();
                return {true, std::nullopt};
            }, 
            "ScheduledCleanup"
        );
        
        thread_pool_->push(cleanup_job);
    }
    
    auto GameSessionManager::execute_cleanup() -> void
    {
        perform_maintenance();
    }
    
    auto GameSessionManager::update_peak_sessions() -> void
    {
        auto current_count = sessions_by_id_.size();
        if (current_count > statistics_.peak_concurrent_sessions)
        {
            statistics_.peak_concurrent_sessions = current_count;
        }
    }
    
    auto GameSessionManager::notify_session_created(std::shared_ptr<GameSession> session) -> void
    {
        if (!session)
        {
            return;
        }
        
        for (const auto& callback : session_created_callbacks_)
        {
            if (callback)
            {
                try
                {
                    callback(session);
                }
                catch (const std::exception& e)
                {
                    Logger::handle().write(LogTypes::Error,
                        fmt::format("Exception in session created callback: {}", e.what()));
                }
            }
        }
    }
    
    auto GameSessionManager::notify_session_terminated(std::shared_ptr<GameSession> session) -> void
    {
        if (!session)
        {
            return;
        }
        
        for (const auto& callback : session_terminated_callbacks_)
        {
            if (callback)
            {
                try
                {
                    callback(session);
                }
                catch (const std::exception& e)
                {
                    Logger::handle().write(LogTypes::Error,
                        fmt::format("Exception in session terminated callback: {}", e.what()));
                }
            }
        }
    }
    
    auto GameSessionManager::notify_session_connected(std::shared_ptr<GameSession> session) -> void
    {
        if (!session)
        {
            return;
        }
        
        for (const auto& callback : session_connected_callbacks_)
        {
            if (callback)
            {
                try
                {
                    callback(session);
                }
                catch (const std::exception& e)
                {
                    Logger::handle().write(LogTypes::Error,
                        fmt::format("Exception in session connected callback: {}", e.what()));
                }
            }
        }
    }
    
    auto GameSessionManager::notify_session_disconnected(std::shared_ptr<GameSession> session) -> void
    {
        if (!session)
        {
            return;
        }
        
        for (const auto& callback : session_disconnected_callbacks_)
        {
            if (callback)
            {
                try
                {
                    callback(session);
                }
                catch (const std::exception& e)
                {
                    Logger::handle().write(LogTypes::Error,
                        fmt::format("Exception in session disconnected callback: {}", e.what()));
                }
            }
        }
    }
}
