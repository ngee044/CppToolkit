#include "GameSessionManager.h"
#include "GameConnection.h"

#include <random>
#include <sstream>
#include <iomanip>

namespace GameNetwork
{
    GameSessionManager::GameSessionManager()
        : cleanup_running_(false)
    {
        start_cleanup_timer();
    }
    
    GameSessionManager::~GameSessionManager()
    {
        stop_cleanup_timer();
    }
    
    auto GameSessionManager::create_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if session already exists
        auto it = sessions_by_account_.find(account_id);
        if (it != sessions_by_account_.end())
        {
            return {nullptr, "Session already exists for account: " + account_id};
        }
        
        // Generate new session ID
        auto session_id = generate_session_id();
        
        // Create new session
        auto session = std::make_shared<GameSession>(session_id, account_id);
        
        // Store session
        sessions_by_id_[session_id] = session;
        sessions_by_account_[account_id] = session;
        
        // Trigger callbacks
        for (const auto& callback : session_created_callbacks_)
        {
            callback(session);
        }
        
        return {session, std::nullopt};
    }
    
    auto GameSessionManager::restore_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_by_account_.find(account_id);
        if (it == sessions_by_account_.end())
        {
            return {nullptr, "No session found for account: " + account_id};
        }
        
        auto session = it->second;
        
        // Restore session state
        auto [success, error] = session->restore_state();
        if (!success)
        {
            return {nullptr, error};
        }
        
        session->set_state(SessionState::Active);
        
        return {session, std::nullopt};
    }
    
    auto GameSessionManager::create_or_restore_session(const std::string& account_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        // Try to restore existing session first
        auto [session, restore_error] = restore_session(account_id);
        if (session)
        {
            return {session, std::nullopt};
        }
        
        // Create new session if restore failed
        return create_session(account_id);
    }
    
    auto GameSessionManager::terminate_session(const std::string& session_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_by_id_.find(session_id);
        if (it == sessions_by_id_.end())
        {
            return {false, "Session not found: " + session_id};
        }
        
        auto session = it->second;
        auto account_id = session->account_id();
        
        // Set terminating state
        session->set_state(SessionState::Terminating);
        
        // Save session state before terminating
        session->save_state();
        
        // Unbind any active connection
        session->unbind_connection();
        
        // Remove from storage
        sessions_by_id_.erase(session_id);
        sessions_by_account_.erase(account_id);
        
        // Trigger callbacks
        for (const auto& callback : session_terminated_callbacks_)
        {
            callback(session);
        }
        
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
    
    auto GameSessionManager::on_network_disconnected(const std::string& connection_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find connection
        auto conn_it = connections_.find(connection_id);
        if (conn_it == connections_.end())
        {
            return {false, "Connection not found: " + connection_id};
        }
        
        // Find associated session
        auto session_it = connection_to_session_.find(connection_id);
        if (session_it != connection_to_session_.end())
        {
            auto session_id = session_it->second;
            auto session = sessions_by_id_[session_id];
            
            if (session)
            {
                // Unbind connection but keep session alive (for reconnection)
                session->unbind_connection();
                session->set_state(SessionState::Suspended);
                
                // Trigger callbacks
                for (const auto& callback : session_disconnected_callbacks_)
                {
                    callback(session);
                }
            }
            
            connection_to_session_.erase(session_it);
        }
        
        // Remove connection
        connections_.erase(conn_it);
        
        return {true, std::nullopt};
    }
    
    auto GameSessionManager::get_session_by_id(const std::string& session_id) const 
        -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_by_id_.find(session_id);
        return (it != sessions_by_id_.end()) ? it->second : nullptr;
    }
    
    auto GameSessionManager::get_session_by_account(const std::string& account_id) const 
        -> std::shared_ptr<GameSession>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_by_account_.find(account_id);
        return (it != sessions_by_account_.end()) ? it->second : nullptr;
    }
    
    auto GameSessionManager::get_connection_by_id(const std::string& connection_id) const 
        -> std::shared_ptr<GameConnection>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
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
            if (session->is_online())
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
            if (session->state() == SessionState::Suspended)
            {
                count++;
            }
        }
        
        return count;
    }
    
    auto GameSessionManager::cleanup_inactive_sessions() -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t cleaned_count = 0;
        std::vector<std::string> sessions_to_remove;
        
        for (const auto& [id, session] : sessions_by_id_)
        {
            if (session->is_timeout() && !session->is_online())
            {
                sessions_to_remove.push_back(id);
            }
        }
        
        for (const auto& session_id : sessions_to_remove)
        {
            terminate_session(session_id);
            cleaned_count++;
        }
        
        return cleaned_count;
    }
    
    auto GameSessionManager::cleanup_timeout_connections() -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t cleaned_count = 0;
        std::vector<std::string> connections_to_remove;
        
        for (const auto& [id, connection] : connections_)
        {
            if (connection->is_timeout())
            {
                connections_to_remove.push_back(id);
            }
        }
        
        for (const auto& connection_id : connections_to_remove)
        {
            on_network_disconnected(connection_id);
            cleaned_count++;
        }
        
        return cleaned_count;
    }
    
    auto GameSessionManager::on_session_created(SessionCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_created_callbacks_.push_back(callback);
    }
    
    auto GameSessionManager::on_session_terminated(SessionCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_terminated_callbacks_.push_back(callback);
    }
    
    auto GameSessionManager::on_session_connected(SessionCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_connected_callbacks_.push_back(callback);
    }
    
    auto GameSessionManager::on_session_disconnected(SessionCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_disconnected_callbacks_.push_back(callback);
    }
    
    auto GameSessionManager::generate_session_id() const -> std::string
    {
        // Generate UUID-like session ID
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        ss << std::hex;
        
        for (int i = 0; i < 32; ++i)
        {
            if (i == 8 || i == 12 || i == 16 || i == 20)
            {
                ss << "-";
            }
            ss << dis(gen);
        }
        
        return ss.str();
    }
    
    auto GameSessionManager::start_cleanup_timer() -> void
    {
        cleanup_running_ = true;
        
        cleanup_timer_ = std::async(std::launch::async, [this]()
        {
            while (cleanup_running_)
            {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                
                cleanup_inactive_sessions();
                cleanup_timeout_connections();
            }
        });
    }
    
    auto GameSessionManager::stop_cleanup_timer() -> void
    {
        cleanup_running_ = false;
        
        if (cleanup_timer_.valid())
        {
            cleanup_timer_.wait();
        }
    }
    
    auto GameSessionManager::migrate_session(const std::string& session_id, 
                                              const std::string& target_server) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_by_id_.find(session_id);
        if (it == sessions_by_id_.end())
        {
            return {false, "Session not found: " + session_id};
        }
        
        auto session = it->second;
        
        // Set migrating state
        session->set_state(SessionState::Migrating);
        
        // Save current state
        auto [save_success, save_error] = session->save_state();
        if (!save_success)
        {
            session->set_state(SessionState::Active);
            return {false, save_error};
        }
        
        // TODO: Implement actual migration logic
        // This would involve:
        // 1. Serializing session state
        // 2. Sending to target server
        // 3. Waiting for confirmation
        // 4. Disconnecting from current server
        
        return {true, std::nullopt};
    }
}