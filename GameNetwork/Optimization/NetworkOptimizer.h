#pragma once

#include "GameNetworkConstants.h"
#include "NetworkMetrics.h"
#include "GamePacket.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <tuple>
#include <queue>

namespace GameNetwork
{
    struct OptimizationSettings
    {
        // Packet batching
        bool enable_batching;
        size_t max_batch_size;
        std::chrono::microseconds batch_timeout;
        
        // Packet merging
        bool enable_merging;
        size_t max_merge_size;
        
        // Compression
        bool enable_compression;
        float compression_threshold;  // Min compression ratio to apply
        
        // Priority queueing
        bool enable_priority_queue;
        size_t queue_size_per_priority;
        
        // Adaptive quality
        bool enable_adaptive_quality;
        float quality_reduction_threshold;
    };
    
    class NetworkOptimizer : public std::enable_shared_from_this<NetworkOptimizer>
    {
    public:
        NetworkOptimizer(std::shared_ptr<NetworkMetrics> metrics);
        virtual ~NetworkOptimizer() = default;
        
        // Packet batching
        auto batch_packet(std::unique_ptr<GamePacket> packet, 
                          const std::string& session_id) -> void;
        auto flush_batch(const std::string& session_id) 
            -> std::vector<std::unique_ptr<GamePacket>>;
        auto flush_all_batches() -> void;
        
        // Packet merging
        auto can_merge_packets(const GamePacket& packet1, 
                               const GamePacket& packet2) const -> bool;
        auto merge_packets(std::vector<std::unique_ptr<GamePacket>> packets) 
            -> std::unique_ptr<GamePacket>;
        
        // Compression optimization
        auto should_compress(const std::vector<uint8_t>& data) const -> bool;
        auto select_compression_algorithm(const std::vector<uint8_t>& data) 
            -> std::string;
        
        // Bandwidth optimization
        auto optimize_send_rate(const std::string& session_id) -> uint32_t;
        auto calculate_optimal_packet_size(NetworkQuality quality) const -> size_t;
        auto should_drop_packet(const GamePacket& packet, 
                                NetworkQuality quality) const -> bool;
        
        // Priority management
        auto adjust_packet_priority(std::unique_ptr<GamePacket> packet,
                                    NetworkQuality quality) -> void;
        auto reorder_packet_queue(std::vector<std::unique_ptr<GamePacket>>& packets) -> void;
        
        // Adaptive optimization
        auto update_optimization_settings(NetworkQuality quality) -> void;
        auto get_current_settings() const -> OptimizationSettings;
        auto override_settings(const OptimizationSettings& settings) -> void;
        
        // LOD optimization
        auto calculate_lod_level(float distance, NetworkQuality quality) const -> uint8_t;
        auto optimize_entity_update_rate(float distance, 
                                         NetworkQuality quality) const -> float;
        
        // Delta optimization
        auto should_send_full_update(uint64_t entity_id, 
                                     const std::string& session_id) const -> bool;
        auto optimize_delta_threshold(NetworkQuality quality) const -> float;
        
        // Statistics
        struct OptimizationStats
        {
            uint64_t packets_batched;
            uint64_t packets_merged;
            uint64_t packets_compressed;
            uint64_t packets_dropped;
            uint64_t bytes_saved;
            float average_compression_ratio;
            std::unordered_map<std::string, uint64_t> optimizations_by_type;
        };
        
        auto get_stats() const -> OptimizationStats;
        auto reset_stats() -> void;
        
    private:
        struct SessionBatch
        {
            std::vector<std::unique_ptr<GamePacket>> packets;
            size_t total_size;
            std::chrono::steady_clock::time_point created_time;
        };
        
        struct PacketQueueEntry
        {
            std::unique_ptr<GamePacket> packet;
            PacketPriority priority;
            std::chrono::steady_clock::time_point queued_time;
            
            auto operator<(const PacketQueueEntry& other) const -> bool
            {
                return priority < other.priority;
            }
        };
        
        auto apply_quality_settings(NetworkQuality quality) -> void;
        auto calculate_compression_ratio(const std::vector<uint8_t>& original,
                                         const std::vector<uint8_t>& compressed) const -> float;
        
    private:
        mutable std::mutex mutex_;
        
        // Network metrics reference
        std::shared_ptr<NetworkMetrics> network_metrics_;
        
        // Current settings
        OptimizationSettings current_settings_;
        
        // Batching
        std::unordered_map<std::string, SessionBatch> session_batches_;
        
        // Priority queues
        std::unordered_map<std::string, std::priority_queue<PacketQueueEntry>> priority_queues_;
        
        // Delta tracking
        std::unordered_map<std::string, std::unordered_map<uint64_t, 
                           std::chrono::steady_clock::time_point>> last_full_updates_;
        
        // Statistics
        OptimizationStats stats_;
        
        // Configuration
        static constexpr size_t DEFAULT_MAX_BATCH_SIZE = 1400;  // MTU safe
        static constexpr auto DEFAULT_BATCH_TIMEOUT = std::chrono::microseconds(100);
        static constexpr float DEFAULT_COMPRESSION_THRESHOLD = 0.8f;
    };
}
