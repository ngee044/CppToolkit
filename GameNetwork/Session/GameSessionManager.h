#pragma once

#include "GameSession.h"
#include "GameConnection.h"
#include "ChannelManager.h"
#include "SessionPersistence.h"
#include "../GameNetworkConstants.h"
#include "../../Network/NetworkSession.h"
#include "../../ThreadPool/ThreadPool.h"
#include "../../Utilities/Logger.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <vector>
#include <optional>
#include <tuple>
#include <atomic>
#include <chrono>
#include <thread>
#include <condition_variable>

namespace GameNetwork
{
    // Forward declarations
    class DisconnectionHandler;
    
    class GameSessionManager : public std::enable_shared_from_this<GameSessionManager>
    {
    public:
        GameSessionManager(uint32_t max_players = 1000, uint32_t max_channels = 10);
        virtual ~GameSessionManager();
        
        // Initialization
        auto initialize(std::shared_ptr<Thread::ThreadPool> thread_pool) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto shutdown() -> void;
        
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
        auto on_network_connected(std::shared_ptr<Network::NetworkSession> network_session, const std::string& account_id) 
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
        auto get_all_online_sessions() const -> std::vector<std::shared_ptr<GameSession>>;
        auto get_sessions_in_channel(uint32_t channel_id) const -> std::vector<std::shared_ptr<GameSession>>;
        
        // Statistics
        auto active_session_count() const -> size_t;
        auto online_session_count() const -> size_t;
        auto get_online_count() const -> size_t { return online_session_count(); }
        auto suspended_session_count() const -> size_t;
        
        // Session migration
        auto migrate_session(const std::string& session_id, const std::string& target_server) -> std::tuple<bool, std::optional<std::string>>;
        
        // Cleanup and maintenance
        auto cleanup_inactive_sessions() -> size_t;
        auto cleanup_timeout_connections() -> size_t;
        auto cleanup_stale_sessions() -> size_t;
        auto perform_maintenance() -> void;
        
        // Monitoring
        struct SessionStatistics
        {
            size_t total_sessions_created;
            size_t total_sessions_terminated;
            size_t peak_concurrent_sessions;
            std::chrono::steady_clock::time_point start_time;
            uint64_t cleanup_runs;
            uint64_t sessions_cleaned;
            size_t total_migrations;
            size_t failed_migrations;
            std::chrono::duration<double> average_session_duration;
        };
        
        auto get_statistics() const -> SessionStatistics;
        auto reset_statistics() -> void;
        auto log_status() -> void;
        
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
        auto schedule_cleanup() -> void;
        auto execute_cleanup() -> void;
        
        auto update_peak_sessions() -> void;
        auto notify_session_created(std::shared_ptr<GameSession> session) -> void;
        auto notify_session_terminated(std::shared_ptr<GameSession> session) -> void;
        auto notify_session_connected(std::shared_ptr<GameSession> session) -> void;
        auto notify_session_disconnected(std::shared_ptr<GameSession> session) -> void;
        
        // Helper methods
        auto cleanup_timeout_sessions() -> size_t;
        auto log_statistics() -> void;
        auto maintenance_loop() -> void;
        
    public:
        // Public methods for GameNetworkServer
        auto get_session(const std::string& client_id) const -> std::shared_ptr<GameSession>;
        auto remove_session(const std::string& client_id) -> void;
        auto get_all_sessions() const -> std::vector<std::shared_ptr<GameSession>>;
        auto get_channel_sessions(uint32_t channel_id) const -> std::vector<std::shared_ptr<GameSession>>;
        auto disconnect_all() -> void;
        auto set_thread_pool(std::shared_ptr<Thread::ThreadPool> thread_pool) -> void;

    private:
        mutable std::mutex mutex_;
        mutable std::mutex maintenance_mutex_;
        std::condition_variable maintenance_cv_;
        
        // Configuration
        uint32_t max_players_;
        uint32_t max_channels_;
        
        // Core components
        std::shared_ptr<Thread::ThreadPool> thread_pool_;
        std::unique_ptr<ChannelManager> channel_manager_;
        std::unique_ptr<SessionPersistence> session_persistence_;
        std::unique_ptr<DisconnectionHandler> disconnection_handler_;
        
        // Session storage
        std::unordered_map<std::string, std::shared_ptr<GameSession>> sessions_by_id_;
        std::unordered_map<std::string, std::shared_ptr<GameSession>> sessions_by_account_;
        
        // Connection storage
        std::unordered_map<std::string, std::shared_ptr<GameConnection>> connections_by_id_;
        std::unordered_map<std::string, std::string> connection_to_session_;
        
        // Statistics
        SessionStatistics stats_;
        
        // State management
        std::atomic<bool> is_running_;
        std::thread maintenance_thread_;
        
        // Constants
        static constexpr std::chrono::seconds kMaintenanceInterval{30};
    };
}
