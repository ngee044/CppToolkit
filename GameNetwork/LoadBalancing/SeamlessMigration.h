#pragma once

#include <GameNetworkConstants.h>
#include <GameSession.h>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <atomic>
#include <queue>

namespace GameNetwork
{
    namespace LoadBalancing
    {
        // Session snapshot for migration
        struct SessionSnapshot
        {
            uint64_t session_id;
            uint64_t player_id;
            
            // Player state
            struct PlayerState
            {
                glm::vec3 position;
                glm::quat rotation;
                float health;
                float mana;
                uint32_t level;
                uint64_t experience;
            } player_state;
            
            // Inventory
            std::vector<std::pair<uint32_t, uint32_t>> inventory; // item_id, quantity
            
            // Active quests
            std::vector<uint32_t> active_quests;
            std::unordered_map<uint32_t, uint32_t> quest_progress;
            
            // Buffs and debuffs
            struct BuffInfo
            {
                uint32_t buff_id;
                float remaining_duration;
                uint32_t stacks;
            };
            std::vector<BuffInfo> active_buffs;            
            // Cooldowns
            std::unordered_map<uint32_t, float> ability_cooldowns;
            
            // Pending packets
            std::queue<std::vector<uint8_t>> pending_packets;
            
            // Timestamp
            std::chrono::steady_clock::time_point snapshot_time;
            uint32_t snapshot_version;
        };

        enum class MigrationPhase
        {
            Preparing,
            Duplicating,
            Switching,
            Completing,
            Completed,
            Failed
        };

        struct MigrationState
        {
            uint64_t session_id;
            std::string source_server;
            std::string target_server;
            MigrationPhase phase;
            std::chrono::steady_clock::time_point start_time;
            std::chrono::steady_clock::time_point phase_start_time;
            std::optional<std::string> error;
            float progress; // 0.0 to 1.0
        };

        class SeamlessMigration
        {
        public:
            SeamlessMigration();
            virtual ~SeamlessMigration() = default;
            // Snapshot management
            auto create_session_snapshot(uint64_t session_id) -> SessionSnapshot;
            auto apply_session_snapshot(const SessionSnapshot& snapshot) 
                -> std::tuple<bool, std::optional<std::string>>;
            auto validate_snapshot(const SessionSnapshot& snapshot) const -> bool;

            // Migration process
            auto migrate_gradually() -> void;
            auto start_migration(uint64_t session_id, 
                               const std::string& target_server)
                -> std::tuple<bool, std::optional<std::string>>;
            auto get_migration_state(uint64_t session_id) const 
                -> std::optional<MigrationState>;

            // Phase operations
            auto duplicate_read_traffic(const std::string& target_server) -> void;
            auto switch_write_traffic(const std::string& target_server) -> void;
            auto graceful_disconnect(const std::string& old_server) -> void;

            // Callbacks
            using SnapshotCallback = std::function<SessionSnapshot(uint64_t)>;
            using ApplyCallback = std::function<bool(const SessionSnapshot&)>;
            using ProgressCallback = std::function<void(uint64_t, MigrationPhase, float)>;

            auto set_snapshot_callback(SnapshotCallback callback) -> void;
            auto set_apply_callback(ApplyCallback callback) -> void;
            auto set_progress_callback(ProgressCallback callback) -> void;

            // Configuration
            struct MigrationConfig
            {
                std::chrono::seconds phase_timeout = std::chrono::seconds(30);
                uint32_t max_retry_attempts = 3;
                bool enable_compression = true;
                bool enable_encryption = true;
                std::chrono::milliseconds duplicate_window = std::chrono::milliseconds(5000);
            };
            auto configure(const MigrationConfig& config) -> void;

            // Batch operations
            auto migrate_multiple_sessions(const std::vector<uint64_t>& session_ids,
                                         const std::string& target_server)
                -> std::tuple<bool, std::optional<std::string>>;
            auto get_active_migrations() const -> std::vector<MigrationState>;

            // Statistics
            struct MigrationStats
            {
                uint64_t migrations_started;
                uint64_t migrations_completed;
                uint64_t migrations_failed;
                std::chrono::milliseconds average_migration_time;
                std::chrono::milliseconds fastest_migration;
                std::chrono::milliseconds slowest_migration;
                size_t average_snapshot_size_bytes;
            };

            auto get_statistics() const -> MigrationStats;

        private:
            // Phase implementations
            auto prepare_migration(uint64_t session_id) -> bool;
            auto duplicate_traffic(uint64_t session_id) -> bool;
            auto switch_traffic(uint64_t session_id) -> bool;
            auto complete_migration(uint64_t session_id) -> bool;
            auto handle_migration_failure(uint64_t session_id, const std::string& error) -> void;

            // Helpers
            auto compress_snapshot(const SessionSnapshot& snapshot) -> std::vector<uint8_t>;
            auto decompress_snapshot(const std::vector<uint8_t>& data) -> SessionSnapshot;
            auto encrypt_snapshot(const std::vector<uint8_t>& data) -> std::vector<uint8_t>;
            auto decrypt_snapshot(const std::vector<uint8_t>& data) -> std::vector<uint8_t>;

        private:
            MigrationConfig config_;
            MigrationStats stats_;
            
            // Active migrations
            std::unordered_map<uint64_t, MigrationState> active_migrations_;
            
            // Callbacks
            SnapshotCallback snapshot_callback_;
            ApplyCallback apply_callback_;
            ProgressCallback progress_callback_;
            
            mutable std::mutex mutex_;
        };
    }
}