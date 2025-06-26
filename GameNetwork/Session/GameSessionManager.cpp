#include "GameSessionManager.h"
#include "GameConnection.h"
#include "SessionPersistence.h"
#include "ChannelManager.h"
#include "DisconnectionHandler.h"
#include "Security/SessionSecurityManager.h"
#include "../../Utilities/Logger.h"
#include "../../Utilities/Converter.h"
#include "../../Utilities/Generator.h"

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
        // shutdown();
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
        // TODO: Implement proper shutdown
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
        // TODO: Implement
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
                        security_manager_->unregister_active_session(account_id, id);
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
        // TODO: Implement
        return std::make_tuple(true, std::optional<std::string>());
    }

    auto GameSessionManager::create_or_restore_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        // TODO: Implement
        return std::make_tuple(nullptr, std::optional<std::string>("Not implemented"));
    }

    auto GameSessionManager::terminate_session(const std::string& session_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // TODO: Implement
        return std::make_tuple(false, std::optional<std::string>("Not implemented"));
    }

    auto GameSessionManager::update_peak_sessions() -> void
    {
        // TODO: Implement
    }

    auto GameSessionManager::notify_session_connected(std::shared_ptr<GameSession> session) -> void
    {
        // TODO: Implement
    }
}

    auto GameSessionManager::initialize_with_security(std::shared_ptr<Thread::ThreadPool> thread_pool,
                                                     const Security::SessionSecurityPolicy& security_policy) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // First perform basic initialization
        auto [init_success, init_error] = initialize(thread_pool);
        if (!init_success)
        {
            return {false, init_error};
        }
        
        try
        {
            // Initialize security manager
            security_manager_ = std::make_unique<Security::SessionSecurityManager>();
            auto [sec_success, sec_error] = security_manager_->initialize(security_policy);
            if (!sec_success)
            {
                shutdown();
                return {false, "Failed to initialize security: " + sec_error.value_or("Unknown error")};
            }
            
            Utilities::Logger::info("GameSessionManager initialized with security features");
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            shutdown();
            return {false, std::string("Failed to initialize security: ") + e.what()};
        }
    }

    auto GameSessionManager::on_network_connected_secure(std::shared_ptr<Network::NetworkSession> network_session, 
                                                        const std::string& account_id,
                                                        const std::string& client_ip,
                                                        const std::string& device_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!security_manager_)
        {
            // Fallback to basic connection if security not initialized
            return on_network_connected(network_session, account_id);
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        try
        {
            // Check concurrent sessions
            auto existing_sessions = security_manager_->check_concurrent_sessions(account_id);
            if (!existing_sessions.empty())
            {
                auto policy = security_manager_->get_policy();
                if (existing_sessions.size() >= policy.max_concurrent_sessions_per_account)
                {
                    if (!policy.allow_kick_previous_session)
                    {
                        return {false, "Maximum concurrent sessions reached"};
                    }
                    
                    // Kick previous sessions
                    for (const auto& session_id : existing_sessions)
                    {
                        if (auto session = get_session_by_id(session_id))
                        {
                            session->set_kicked_by_duplicate_login(true);
                            terminate_session(session_id);
                            Utilities::Logger::info("Kicked previous session for duplicate login: " + session_id);
                        }
                    }
                }
            }
            
            // Create or restore session
            auto [session, error] = create_or_restore_session(account_id);
            if (!session)
            {
                return {false, error};
            }
            
            // Generate security token
            auto [token, token_error] = security_manager_->generate_session_token(
                session->session_id(), client_ip, device_id);
            if (token.empty())
            {
                terminate_session(session->session_id());
                return {false, "Failed to generate security token: " + token_error.value_or("Unknown error")};
            }
            
            // Register active session
            auto [reg_success, reg_error] = security_manager_->register_active_session(
                account_id, session->session_id());
            if (!reg_success)
            {
                terminate_session(session->session_id());
                return {false, reg_error};
            }
            
            // Create connection
            auto connection = std::make_shared<GameConnection>(
                network_session, 
                Utilities::Generator::generate_uuid()
            );
            
            // Bind connection to session
            session->bind_connection(connection);
            session->set_session_token(token);
            
            // Store connection
            connections_by_id_[connection->id()] = connection;
            connection_to_session_[connection->id()] = session->session_id();
            
            // Update statistics
            update_peak_sessions();
            
            // Notify callbacks
            notify_session_connected(session);
            
            Utilities::Logger::info("Secure connection established for account: " + account_id);
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            return {false, std::string("Connection failed: ") + e.what()};
        }
    }

    auto GameSessionManager::validate_session_token(const std::string& session_id,
                                                   const std::string& token,
                                                   const std::string& client_ip,
                                                   const std::string& device_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!security_manager_)
        {
            return {true, std::nullopt};  // No security check if not initialized
        }
        
        return security_manager_->validate_session_token(token, session_id, client_ip, device_id);
    }

    auto GameSessionManager::refresh_session_token(const std::string& session_id,
                                                  const std::string& old_token) 
        -> std::tuple<std::string, std::optional<std::string>>
    {
        if (!security_manager_)
        {
            return {old_token, std::nullopt};  // Return same token if security not initialized
        }
        
        auto [new_token, error] = security_manager_->refresh_session_token(old_token);
        if (!new_token.empty())
        {
            if (auto session = get_session_by_id(session_id))
            {
                session->set_session_token(new_token);
            }
        }
        
        return {new_token, error};
    }

    auto GameSessionManager::check_session_security(const std::string& session_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!security_manager_)
        {
            return {true, std::nullopt};
        }
        
        // Check if session is locked
        if (security_manager_->is_session_locked(session_id))
        {
            return {false, "Session is locked due to security violations"};
        }
        
        // Check timeout
        auto [is_timeout, remaining_time] = security_manager_->check_session_timeout(session_id);
        if (is_timeout)
        {
            return {false, "Session has timed out"};
        }
        
        return {true, std::nullopt};
    }

    auto GameSessionManager::get_active_sessions_for_account(const std::string& account_id) 
        -> std::vector<std::string>
    {
        if (!security_manager_)
        {
            // Fallback to basic session lookup
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = sessions_by_account_.find(account_id);
            if (it != sessions_by_account_.end())
            {
                return {it->second->session_id()};
            }
            return {};
        }
        
        return security_manager_->check_concurrent_sessions(account_id);
    }
