#pragma once

#include "GameSession.h"
#include <RedisClient.h>

#include <memory>
#include <string>
#include <chrono>
#include <optional>
#include <tuple>
#include <vector>
#include <functional>
#include <future>
#include <atomic>
#include <mutex>

namespace GameNetwork
{
    struct SessionSnapshot
    {
        std::string session_id;
        std::string account_id;
        SessionState state;
        Location location;
        std::vector<uint8_t> character_data;
        std::unordered_map<std::string, std::string> custom_data;
        std::chrono::system_clock::time_point created_time;
        std::chrono::system_clock::time_point last_save_time;
        uint32_t version;
    };
    
    class SessionPersistence : public std::enable_shared_from_this<SessionPersistence>
    {
    public:
        SessionPersistence(std::shared_ptr<Redis::RedisClient> redis_client);
        virtual ~SessionPersistence();
        
        // Session saving
        auto save_session(std::shared_ptr<GameSession> session) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto save_session_async(std::shared_ptr<GameSession> session,
                                std::function<void(bool, std::optional<std::string>)> callback) 
            -> void;
        
        // Session loading
        auto load_session(const std::string& session_id) 
            -> std::tuple<std::optional<SessionSnapshot>, std::optional<std::string>>;
        auto load_session_by_account(const std::string& account_id) 
            -> std::tuple<std::optional<SessionSnapshot>, std::optional<std::string>>;
        
        // Session queries
        auto session_exists(const std::string& session_id) const -> bool;
        auto get_active_sessions() const -> std::vector<std::string>;
        auto get_sessions_by_server(const std::string& server_id) const 
            -> std::vector<std::string>;
        
        // Session lifecycle
        auto mark_session_active(const std::string& session_id,
                                 const std::string& server_id) -> void;
        auto mark_session_inactive(const std::string& session_id) -> void;
        auto delete_session(const std::string& session_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Expiration management
        auto set_session_ttl(const std::string& session_id,
                             std::chrono::seconds ttl) -> void;
        auto refresh_session_ttl(const std::string& session_id) -> void;
        auto get_session_ttl(const std::string& session_id) const 
            -> std::optional<std::chrono::seconds>;
        
        // Atomic operations
        auto increment_session_value(const std::string& session_id,
                                     const std::string& key,
                                     int64_t delta) -> int64_t;
        auto compare_and_swap(const std::string& session_id,
                              const std::string& key,
                              const std::string& expected,
                              const std::string& new_value) -> bool;
        
        // Bulk operations
        auto save_sessions_batch(const std::vector<std::shared_ptr<GameSession>>& sessions) 
            -> std::tuple<size_t, std::vector<std::string>>;
        auto load_sessions_batch(const std::vector<std::string>& session_ids) 
            -> std::vector<SessionSnapshot>;
        
        // Session locking (for distributed consistency)
        auto acquire_session_lock(const std::string& session_id,
                                  const std::string& holder_id,
                                  std::chrono::seconds timeout = std::chrono::seconds(30)) 
            -> bool;
        auto release_session_lock(const std::string& session_id,
                                  const std::string& holder_id) -> bool;
        auto extend_session_lock(const std::string& session_id,
                                 const std::string& holder_id,
                                 std::chrono::seconds extension) -> bool;
        
        // Backup and recovery
        auto create_session_backup(const std::string& session_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto restore_from_backup(const std::string& session_id,
                                 uint32_t backup_version) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto list_backups(const std::string& session_id) const 
            -> std::vector<uint32_t>;
        
        // Configuration
        auto enable_auto_save(bool enable) -> void;
        auto set_auto_save_interval(std::chrono::seconds interval) -> void;
        auto enable_compression(bool enable) -> void;
        auto set_redis_key_prefix(const std::string& prefix) -> void;
        
        // Monitoring
        auto get_storage_size(const std::string& session_id) const -> size_t;
        auto get_total_storage_size() const -> size_t;
        auto cleanup_expired_sessions() -> size_t;
        
        // Statistics
        struct PersistenceStats
        {
            uint64_t sessions_saved;
            uint64_t sessions_loaded;
            uint64_t save_failures;
            uint64_t load_failures;
            uint64_t average_save_time_ms;
            uint64_t average_load_time_ms;
            uint64_t total_storage_bytes;
            uint64_t compression_ratio_percent;
        };
        
        auto get_stats() const -> PersistenceStats;
        auto reset_stats() -> void;
        
    private:
        auto serialize_session(std::shared_ptr<GameSession> session) const 
            -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>;
        auto deserialize_session(const std::vector<uint8_t>& data) const 
            -> std::tuple<std::optional<SessionSnapshot>, std::optional<std::string>>;
        
        auto compress_data(const std::vector<uint8_t>& data) const 
            -> std::vector<uint8_t>;
        auto decompress_data(const std::vector<uint8_t>& compressed) const 
            -> std::vector<uint8_t>;
        
        auto get_session_key(const std::string& session_id) const -> std::string;
        auto get_account_index_key(const std::string& account_id) const -> std::string;
        auto get_lock_key(const std::string& session_id) const -> std::string;
        
        auto update_indices(const std::string& session_id,
                            const std::string& account_id,
                            const std::string& server_id) -> void;
        
    private:
        mutable std::mutex mutex_;
        
        // Redis client
        std::shared_ptr<Redis::RedisClient> redis_client_;
        
        // Configuration
        std::string key_prefix_;
        bool auto_save_enabled_;
        std::chrono::seconds auto_save_interval_;
        bool compression_enabled_;
        
        // Auto-save thread
        std::future<void> auto_save_thread_;
        std::atomic<bool> auto_save_running_;
        
        // Statistics
        PersistenceStats stats_;
        
        // Constants
        static constexpr const char* DEFAULT_KEY_PREFIX = "game:session:";
        static constexpr auto DEFAULT_SESSION_TTL = std::chrono::hours(24);
        static constexpr auto DEFAULT_AUTO_SAVE_INTERVAL = std::chrono::minutes(5);
    };
}
