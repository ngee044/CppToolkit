#pragma once

#include "GameNetworkConstants.h"
#include "EntityReplicator.h"
#include "NetworkMetrics.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <bitset>
#include <chrono>
#include <optional>
#include <tuple>
#include <functional>
#include <future>

namespace GameNetwork  
{
    enum class ReplicationPriority
    {
        Critical = 0,    // Always replicate
        High = 1,        // Replicate frequently
        Medium = 2,      // Normal replication
        Low = 3,         // Replicate when bandwidth available
        OnChange = 4     // Only when changed significantly
    };
    
    struct ReplicationPolicy
    {
        ReplicationMode mode;
        ReplicationPriority priority;
        float update_rate;  // Updates per second
        float min_change_threshold;  // Minimum change to trigger update
        bool use_delta_compression;
        bool use_bit_packing;
    };
    
    class ReplicationSystem : public std::enable_shared_from_this<ReplicationSystem>
    {
    public:
        ReplicationSystem(std::shared_ptr<EntityReplicator> entity_replicator,
                          std::shared_ptr<NetworkMetrics> network_metrics);
        virtual ~ReplicationSystem();
        
        // Entity management
        auto register_replicated_entity(uint64_t entity_id,
                                        const ReplicationPolicy& policy) -> void;
        auto unregister_replicated_entity(uint64_t entity_id) -> void;
        auto update_replication_policy(uint64_t entity_id,
                                       const ReplicationPolicy& policy) -> void;
        
        // Property replication
        template<typename T>
        auto replicate_property(uint64_t entity_id,
                                const std::string& property_name,
                                const T& value,
                                ReplicationPriority priority = ReplicationPriority::Medium) -> void;
        
        // Batch replication
        auto begin_replication_frame() -> void;
        auto end_replication_frame() -> void;
        auto flush_replication_queue() -> void;
        
        // Interest management  
        auto set_interest_manager(
            std::function<bool(uint64_t, const Location&, const Location&)> interest_func) -> void;
        auto update_viewer_location(const std::string& session_id,
                                    const Location& location) -> void;
        auto get_relevant_entities(const std::string& session_id) const 
            -> std::vector<uint64_t>;
        
        // Delta compression
        auto create_delta_update(uint64_t entity_id,
                                 const std::string& session_id) 
            -> std::optional<std::vector<uint8_t>>;
        auto apply_delta_update(uint64_t entity_id,
                                const std::vector<uint8_t>& delta) -> void;
        
        // Snapshot system
        auto create_world_snapshot() const -> std::vector<uint8_t>;
        auto create_partial_snapshot(const std::vector<uint64_t>& entities) const 
            -> std::vector<uint8_t>;
        auto restore_from_snapshot(const std::vector<uint8_t>& snapshot) -> void;
        
        // Reliability
        auto acknowledge_replication(const std::string& session_id,
                                     uint32_t sequence) -> void;
        auto handle_packet_loss(const std::string& session_id,
                                uint32_t lost_sequence) -> void;
        auto get_unacknowledged_count(const std::string& session_id) const -> size_t;
        
        // Bandwidth management
        auto set_bandwidth_limit(const std::string& session_id,
                                 uint32_t bytes_per_second) -> void;
        auto get_bandwidth_usage(const std::string& session_id) const -> uint32_t;
        auto prioritize_replication_queue(const std::string& session_id) -> void;
        
        // State diffing
        auto enable_state_diffing(bool enable) -> void;
        auto set_diff_threshold(float threshold) -> void;
        auto should_replicate_change(uint64_t entity_id,
                                     const std::string& property_name,
                                     const std::any& old_value,
                                     const std::any& new_value) const -> bool;
        
        // Optimization
        auto optimize_for_network_conditions(NetworkQuality quality) -> void;
        auto get_replication_statistics() const -> std::unordered_map<uint64_t, float>;
        
        // Processing
        auto start_replication_thread() -> void;
        auto stop_replication_thread() -> void;
        
        // Statistics
        struct ReplicationStats
        {
            uint64_t total_replications;
            uint64_t delta_updates_sent;
            uint64_t full_updates_sent;
            uint64_t bytes_sent;
            uint64_t bytes_saved_by_compression;
            float average_compression_ratio;
            std::unordered_map<ReplicationPriority, uint64_t> replications_by_priority;
        };
        
        auto get_stats() const -> ReplicationStats;
        auto reset_stats() -> void;
        
    private:
        struct ReplicationQueueEntry
        {
            uint64_t entity_id;
            std::string property_name;
            std::any value;
            ReplicationPriority priority;
            std::chrono::steady_clock::time_point queued_time;
            size_t estimated_size;
        };
        
        struct SessionReplicationState
        {
            Location viewer_location;
            std::unordered_map<uint64_t, uint32_t> last_replicated_version;
            std::unordered_map<uint64_t, std::bitset<256>> replicated_properties;
            std::deque<std::pair<uint32_t, std::vector<uint8_t>>> unacknowledged_packets;
            uint32_t bandwidth_limit;
            uint32_t current_bandwidth_usage;
            std::chrono::steady_clock::time_point bandwidth_reset_time;
        };
        
        struct EntityReplicationState
        {
            ReplicationPolicy policy;
            uint32_t current_version;
            std::chrono::steady_clock::time_point last_replication;
            std::unordered_map<std::string, uint32_t> property_versions;
            std::unordered_map<std::string, std::any> property_values;
        };
        
        auto process_replication_queue() -> void;
        auto should_replicate_entity(uint64_t entity_id,
                                     const SessionReplicationState& session_state) const -> bool;
        auto calculate_replication_priority(uint64_t entity_id,
                                            const Location& viewer_location) const -> float;
        auto pack_replication_data(const std::vector<ReplicationQueueEntry>& entries) 
            -> std::vector<uint8_t>;
        
    private:
        mutable std::mutex mutex_;
        
        // Dependencies
        std::shared_ptr<EntityReplicator> entity_replicator_;
        std::shared_ptr<NetworkMetrics> network_metrics_;
        
        // Entity states
        std::unordered_map<uint64_t, EntityReplicationState> entity_states_;
        
        // Session states
        std::unordered_map<std::string, SessionReplicationState> session_states_;
        
        // Replication queue
        std::vector<ReplicationQueueEntry> replication_queue_;
        bool in_replication_frame_;
        
        // Interest management
        std::function<bool(uint64_t, const Location&, const Location&)> interest_function_;
        
        // Configuration
        bool state_diffing_enabled_;
        float diff_threshold_;
        
        // Processing thread
        std::future<void> replication_thread_;
        std::atomic<bool> replication_running_;
        std::chrono::milliseconds replication_interval_;
        
        // Statistics
        ReplicationStats stats_;
    };
}
