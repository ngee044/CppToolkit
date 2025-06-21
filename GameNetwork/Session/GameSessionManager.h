#pragma once

#include "GameSession.h"
#include "../Network/NetworkSession.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <vector>
#include <optional>
#include <tuple>

namespace GameNetwork
{
    class GameSessionManager : public std::enable_shared_from_this<GameSessionManager>
    {
    public:
        GameSessionManager();
        virtual ~GameSessionManager();
        
        // Session lifecycle
        auto create_session(const std::string& account_id) 
            -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>;
        auto restore_session(const std::string& account_id) 
            -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>;
        auto create_or_restore_session(const std::string& account_id) 
            -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>;
        auto terminate_session(const std::string& session_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Connection management
        auto on_network_connected(std::shared_ptr<Network::NetworkSession> network_session, 
                                  const std::string& account_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto on_network_disconnected(const std::string& connection_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Session queries
        auto get_session_by_id(const std::string& session_id) const 
            -> std::shared_ptr<GameSession>;
        auto get_session_by_account(const std::string& account_id) const 
            -> std::shared_ptr<GameSession>;
        auto get_connection_by_id(const std::string& connection_id) const 
            -> std::shared_ptr<GameConnection>;
        
        // Statistics
        auto active_session_count() const -> size_t;
        auto online_session_count() const -> size_t;
        auto suspended_session_count() const -> size_t;
        
        // Session migration
        auto migrate_session(const std::string& session_id, 
                             const std::string& target_server) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Cleanup
        auto cleanup_inactive_sessions() -> size_t;
        auto cleanup_timeout_connections() -> size_t;
        
        // Callbacks
        using SessionCallback = std::function<void(std::shared_ptr<GameSession>)>;
        auto on_session_created(SessionCallback callback) -> void;
        auto on_session_terminated(SessionCallback callback) -> void;
        auto on_session_connected(SessionCallback callback) -> void;
        auto on_session_disconnected(SessionCallback callback) -> void;
        
    private:
        auto generate_session_id() const -> std::string;
        auto start_cleanup_timer() -> void;
        auto stop_cleanup_timer() -> void;
        
    private:
        mutable std::mutex mutex_;
        
        // Session storage
        std::unordered_map<std::string, std::shared_ptr<GameSession>> sessions_by_id_;
        std::unordered_map<std::string, std::shared_ptr<GameSession>> sessions_by_account_;
        
        // Connection storage
        std::unordered_map<std::string, std::shared_ptr<GameConnection>> connections_;
        std::unordered_map<std::string, std::string> connection_to_session_;
        
        // Callbacks
        std::vector<SessionCallback> session_created_callbacks_;
        std::vector<SessionCallback> session_terminated_callbacks_;
        std::vector<SessionCallback> session_connected_callbacks_;
        std::vector<SessionCallback> session_disconnected_callbacks_;
        
        // Cleanup timer
        std::future<void> cleanup_timer_;
        std::atomic<bool> cleanup_running_;
    };
}
