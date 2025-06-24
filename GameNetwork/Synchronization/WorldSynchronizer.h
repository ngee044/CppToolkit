#pragma once

#include "../GameNetworkConstants.h"
#include "../Session/GameSession.h"
#include "../Packet/GamePacket.h"
#include <ThreadPool.h>
#include <Logger.h>

#include <memory>
#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <queue>
#include <functional>

namespace GameNetwork
{
    // Forward declarations
    class GameSessionManager;
    
    // Entity state for synchronization
    struct WorldEntityState
    {
        uint64_t entity_id;
        Location position;
        Location velocity;
        float rotation;
        uint32_t health;
        uint32_t max_health;
        uint64_t last_update_time;
        uint64_t client_timestamp;
        bool is_moving;
        bool is_dirty;
    };
    
    // Lag compensation data
    struct LagCompensationSnapshot
    {
        uint64_t timestamp;
        std::unordered_map<uint64_t, Location> entity_positions;
        std::unordered_map<uint64_t, WorldEntityState> entity_states;
    };
    
    // Synchronization update
    struct SyncUpdate
    {
        uint64_t entity_id;
        WorldEntityState state;
        std::chrono::steady_clock::time_point created_time;
        uint32_t priority;
    };
    
    class WorldSynchronizer
    {
    public:
        WorldSynchronizer();
        ~WorldSynchronizer();
        
        // Initialization
        auto initialize(std::shared_ptr<GameSessionManager> session_manager) -> bool;
        auto shutdown() -> void;
        auto set_thread_pool(std::shared_ptr<Thread::ThreadPool> thread_pool) -> void;
        
        // Entity synchronization
        auto register_entity(uint64_t entity_id, const WorldEntityState& initial_state) -> void;
        auto unregister_entity(uint64_t entity_id) -> void;
        auto update_entity_state(uint64_t entity_id, const WorldEntityState& state) -> void;
        auto get_entity_state(uint64_t entity_id) const -> std::optional<WorldEntityState>;
        
        // Real-time synchronization
        auto start_sync_loop() -> void;
        auto stop_sync_loop() -> void;
        auto force_sync() -> void;
        auto sync_entity_to_clients(uint64_t entity_id, const std::vector<std::shared_ptr<GameSession>>& clients) -> void;
        auto sync_area_to_client(const Location& center, float radius, std::shared_ptr<GameSession> client) -> void;
        
        // Lag compensation
        auto enable_lag_compensation(bool enable) -> void;
        auto set_snapshot_history_duration(std::chrono::milliseconds duration) -> void;
        auto get_entity_position_at_time(uint64_t entity_id, uint64_t timestamp) const -> std::optional<Location>;
        auto validate_movement(uint64_t entity_id, const Location& from, const Location& to, uint64_t client_timestamp) const -> bool;
        auto compensate_for_lag(uint64_t entity_id, const Location& client_position, uint64_t client_timestamp) -> Location;
        
        // Area of Interest (AOI)
        auto set_aoi_radius(float radius) -> void;
        auto get_entities_in_range(const Location& center, float radius) const -> std::vector<uint64_t>;
        auto get_clients_in_range(const Location& center, float radius) const -> std::vector<std::shared_ptr<GameSession>>;
        
        // Conflict resolution
        auto resolve_movement_conflict(uint64_t entity_id, const std::vector<WorldEntityState>& conflicting_states) -> WorldEntityState;
        auto is_position_valid(const Location& position) const -> bool;
        auto clamp_to_world_bounds(Location& position) const -> void;
        
        // Performance monitoring
        struct SyncStats
        {
            uint64_t total_updates_processed;
            uint64_t total_entities_synced;
            uint64_t lag_compensation_queries;
            uint64_t conflict_resolutions;
            double average_sync_time_ms;
            uint32_t active_entities;
            uint32_t snapshots_stored;
        };
        
        auto get_stats() const -> SyncStats;
        auto reset_stats() -> void;
        
        // Configuration
        auto set_sync_frequency(uint32_t frequency_hz) -> void;
        auto set_max_entities_per_update(uint32_t max_entities) -> void;
        auto set_prediction_enabled(bool enabled) -> void;
        
    private:
        // Internal synchronization methods
        auto sync_loop() -> void;
        auto process_sync_updates() -> void;
        auto update_entity_snapshots() -> void;
        auto cleanup_old_snapshots() -> void;
        auto calculate_sync_priority(const WorldEntityState& state) const -> uint32_t;
        auto interpolate_entity_state(const WorldEntityState& from, const WorldEntityState& to, float factor) const -> WorldEntityState;
        auto extrapolate_entity_position(const WorldEntityState& state, std::chrono::milliseconds delta) const -> Location;
        
        // Lag compensation helpers
        auto create_snapshot() -> void;
        auto find_snapshot_at_time(uint64_t timestamp) const -> std::optional<LagCompensationSnapshot>;
        auto interpolate_snapshots(const LagCompensationSnapshot& earlier, const LagCompensationSnapshot& later, uint64_t timestamp) const -> LagCompensationSnapshot;
        
        // Area of Interest helpers
        auto calculate_distance(const Location& a, const Location& b) const -> float;
        auto get_nearby_clients(uint64_t entity_id) const -> std::vector<std::shared_ptr<GameSession>>;
        auto should_sync_to_client(uint64_t entity_id, std::shared_ptr<GameSession> client) const -> bool;
        
    private:
        mutable std::mutex mutex_;
        mutable std::mutex snapshot_mutex_;
        std::condition_variable sync_cv_;
        
        // Core components
        std::shared_ptr<Thread::ThreadPool> thread_pool_;
        std::shared_ptr<GameSessionManager> session_manager_;
        
        // Entity management
        std::unordered_map<uint64_t, WorldEntityState> entity_states_;
        std::queue<SyncUpdate> sync_queue_;
        
        // Lag compensation
        bool lag_compensation_enabled_;
        std::chrono::milliseconds snapshot_history_duration_;
        std::vector<LagCompensationSnapshot> snapshots_;
        
        // Configuration
        uint32_t sync_frequency_hz_;
        uint32_t max_entities_per_update_;
        float aoi_radius_;
        bool prediction_enabled_;
        
        // Synchronization loop
        std::atomic<bool> is_running_;
        std::thread sync_thread_;
        
        // World bounds
        Location world_min_;
        Location world_max_;
        
        // Statistics
        mutable SyncStats stats_;
        mutable std::chrono::steady_clock::time_point last_sync_time_;
    };
}
