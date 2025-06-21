#pragma once

#include "../GameNetworkConstants.h"
#include "../Session/GameSession.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <optional>
#include <tuple>
#include <future>
#include <atomic>

#include "boost/functional/hash.hpp"

namespace GameNetwork
{
    struct Entity
    {
        uint64_t id;
        uint32_t type;
        Location location;
        std::unordered_map<std::string, std::any> properties;
        std::chrono::steady_clock::time_point last_update;
    };
    
    class WorldSynchronizer : public std::enable_shared_from_this<WorldSynchronizer>
    {
    public:
        WorldSynchronizer();
        virtual ~WorldSynchronizer();
        
        // Area of Interest (AOI) management
        auto set_view_distance(float distance) -> void;
        auto get_view_distance() const -> float;
        
        auto enter_area(std::shared_ptr<GameSession> session, const Location& location) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto leave_area(std::shared_ptr<GameSession> session) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto update_position(std::shared_ptr<GameSession> session, const Location& location) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Entity management
        auto spawn_entity(const Entity& entity) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto despawn_entity(uint64_t entity_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto update_entity(uint64_t entity_id, const Entity& updated_entity) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Interest management
        auto get_interested_sessions(const Location& location, float radius = 0.0f) const 
            -> std::vector<std::shared_ptr<GameSession>>;
        auto get_visible_entities(std::shared_ptr<GameSession> session) const 
            -> std::vector<Entity>;
        
        // Broadcasting
        auto broadcast_to_area(const Location& center, 
                               float radius, 
                               const GamePacket& packet, 
                               std::shared_ptr<GameSession> exclude = nullptr) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Channel management
        auto get_channel_sessions(uint32_t channel_id) const 
            -> std::vector<std::shared_ptr<GameSession>>;
        auto broadcast_to_channel(uint32_t channel_id, 
                                  const GamePacket& packet, 
                                  std::shared_ptr<GameSession> exclude = nullptr) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Synchronization
        auto start_sync_timer() -> void;
        auto stop_sync_timer() -> void;
        auto force_sync() -> void;
        
        // Level of Detail (LOD)
        enum class DetailLevel
        {
            Full = 0,
            High = 1,
            Medium = 2,
            Low = 3
        };
        
        auto calculate_detail_level(float distance) const -> DetailLevel;
        auto set_lod_distances(float high, float medium, float low) -> void;
        
        // Statistics
        struct SyncStats
        {
            uint64_t total_syncs;
            uint64_t entities_tracked;
            uint64_t sessions_tracked;
            uint64_t packets_broadcasted;
            std::chrono::microseconds average_sync_time;
        };
        
        auto get_stats() const -> SyncStats;
        auto reset_stats() -> void;
        
    private:
        struct SessionInfo
        {
            std::shared_ptr<GameSession> session;
            Location location;
            std::unordered_set<uint64_t> visible_entities;
            std::chrono::steady_clock::time_point last_sync;
        };
        
        struct GridCell
        {
            std::unordered_set<std::string> session_ids;
            std::unordered_set<uint64_t> entity_ids;
        };
        
        auto get_grid_key(const Location& location) const -> std::pair<int32_t, int32_t>;
        auto get_nearby_cells(const Location& location, float radius) const 
            -> std::vector<std::pair<int32_t, int32_t>>;
        
        auto sync_session(SessionInfo& info) -> void;
        auto sync_all_sessions() -> void;
        
    private:
        mutable std::mutex mutex_;
        
        // Configuration
        float view_distance_;
        float grid_cell_size_;
        float lod_high_distance_;
        float lod_medium_distance_;
        float lod_low_distance_;
        
        // Session tracking
        std::unordered_map<std::string, SessionInfo> sessions_;
        
        // Entity tracking
        std::unordered_map<uint64_t, Entity> entities_;
        
        // Spatial indexing
        std::unordered_map<std::pair<int32_t, int32_t>, GridCell, 
                           boost::hash<std::pair<int32_t, int32_t>>> grid_;
        
        // Synchronization
        std::future<void> sync_timer_;
        std::atomic<bool> sync_running_;
        std::chrono::milliseconds sync_interval_;
        
        // Statistics
        SyncStats stats_;
    };
}
