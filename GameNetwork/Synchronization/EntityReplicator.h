#pragma once

#include "GameNetworkConstants.h"
#include "GamePacket.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <bitset>
#include <chrono>
#include <optional>
#include <tuple>
#include <any>

namespace GameNetwork
{
    enum class ReplicationMode
    {
        Reliable = 0,
        Unreliable = 1,
        ReliableOrdered = 2,
        UnreliableLatest = 3
    };
    
    enum class PropertyType
    {
        Int32 = 0,
        Float = 1,
        Vector3 = 2,
        Quaternion = 3,
        String = 4,
        Custom = 5
    };
    
    struct ReplicatedProperty
    {
        std::string name;
        PropertyType type;
        std::any value;
        std::any previous_value;
        ReplicationMode mode;
        uint32_t priority;
        bool dirty;
        std::chrono::steady_clock::time_point last_replicated;
    };
    
    class EntityReplicator : public std::enable_shared_from_this<EntityReplicator>
    {
    public:
        EntityReplicator();
        virtual ~EntityReplicator() = default;
        
        // Entity registration
        auto register_entity(uint64_t entity_id, uint32_t entity_type) -> void;
        auto unregister_entity(uint64_t entity_id) -> void;
        auto is_entity_registered(uint64_t entity_id) const -> bool;
        
        // Property management
        auto register_property(uint64_t entity_id, 
                               const std::string& property_name,
                               PropertyType type,
                               ReplicationMode mode = ReplicationMode::Reliable,
                               uint32_t priority = 100) -> void;
        
        template<typename T>
        auto update_property(uint64_t entity_id, 
                             const std::string& property_name, 
                             const T& value) -> void;
        
        template<typename T>
        auto get_property(uint64_t entity_id, 
                          const std::string& property_name) const -> std::optional<T>;
        
        // Replication control
        auto set_replication_rate(uint64_t entity_id, uint32_t updates_per_second) -> void;
        auto set_relevancy_distance(uint64_t entity_id, float distance) -> void;
        auto force_replication(uint64_t entity_id) -> void;
        
        // Delta compression
        auto enable_delta_compression(bool enable) -> void;
        auto create_delta_snapshot(uint64_t entity_id) -> std::vector<uint8_t>;
        auto apply_delta_snapshot(uint64_t entity_id, const std::vector<uint8_t>& delta) -> void;
        
        // Replication process
        auto gather_dirty_entities() const -> std::vector<uint64_t>;
        auto create_replication_packet(uint64_t entity_id) -> std::unique_ptr<GamePacket>;
        auto process_replication_packet(const GamePacket& packet) -> void;
        
        // Interest management
        auto calculate_priority(uint64_t entity_id, const Location& viewer_location) const -> float;
        auto should_replicate_to(uint64_t entity_id, const Location& viewer_location) const -> bool;
        
        // Snapshot management
        auto create_full_snapshot() const -> std::vector<uint8_t>;
        auto restore_from_snapshot(const std::vector<uint8_t>& snapshot) -> void;
        
        // Statistics
        struct ReplicationStats
        {
            uint64_t total_replications;
            uint64_t delta_compressions;
            uint64_t bytes_saved_by_compression;
            uint64_t properties_replicated;
            std::unordered_map<uint64_t, uint64_t> replications_per_entity;
        };
        
        auto get_stats() const -> ReplicationStats;
        auto reset_stats() -> void;
        
    private:
        struct EntityState
        {
            uint64_t entity_id;
            uint32_t entity_type;
            std::unordered_map<std::string, ReplicatedProperty> properties;
            uint32_t replication_rate;
            float relevancy_distance;
            std::chrono::steady_clock::time_point last_replication;
            std::bitset<256> property_mask;  // For delta compression
        };
        
        auto mark_property_dirty(uint64_t entity_id, const std::string& property_name) -> void;
        auto clear_dirty_flags(uint64_t entity_id) -> void;
        auto serialize_property(const ReplicatedProperty& prop) const -> std::vector<uint8_t>;
        auto deserialize_property(const std::vector<uint8_t>& data, PropertyType type) -> std::any;
        
    private:
        mutable std::mutex mutex_;
        
        // Entity storage
        std::unordered_map<uint64_t, EntityState> entities_;
        
        // Configuration
        bool delta_compression_enabled_;
        
        // Statistics
        ReplicationStats stats_;
    };
}
