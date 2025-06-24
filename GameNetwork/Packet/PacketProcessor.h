#pragma once

#include "../Packet/GamePacket.h"
#include "../../ThreadPool/ThreadPool.h"

#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <queue>
#include <optional>
#include <tuple>

namespace GameNetwork
{
    class PacketProcessor : public std::enable_shared_from_this<PacketProcessor>
    {
    public:
        PacketProcessor();
        virtual ~PacketProcessor();
        
        // Simplified interface for GameNetworkServer
        auto serialize(const GamePacket& packet) -> std::optional<std::string>;
        auto deserialize(const std::string& data) -> std::unique_ptr<GamePacket>;
        auto deserialize_binary(const std::vector<uint8_t>& data) -> std::unique_ptr<GamePacket>;
        
        // Compression settings
        auto set_compression_enabled(bool enabled) -> void;
        auto is_compression_enabled() const -> bool;
        
        // Encryption settings
        auto set_encryption_enabled(bool enabled) -> void;
        auto is_encryption_enabled() const -> bool;
        auto set_encryption_key(const std::string& key) -> void;
        
        // Packet batching
        auto enable_batching(bool enable) -> void;
        auto is_batching_enabled() const -> bool;
        auto add_to_batch(const GamePacket& packet) -> void;
        auto flush_batch() -> std::vector<std::vector<uint8_t>>;
        
        // Packet validation
        auto validate_packet(const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto validate_packet_size(size_t size) const -> bool;
        auto validate_packet_type(PacketType type) const -> bool;
        
        // Statistics
        struct ProcessorStats
        {
            uint64_t packets_processed;
            uint64_t packets_compressed;
            uint64_t packets_encrypted;
            uint64_t packets_batched;
            uint64_t compression_ratio_percent;
            uint64_t average_processing_time_us;
        };
        
        auto get_stats() const -> ProcessorStats;
        auto reset_stats() -> void;
        
    private:
        struct PacketBatch
        {
            std::vector<std::vector<uint8_t>> serialized_packets;
            std::chrono::steady_clock::time_point created_time;
            size_t total_size;
        };
        
        auto calculate_checksum(const std::vector<uint8_t>& data) const -> uint32_t;
        auto process_batch() -> std::vector<std::vector<uint8_t>>;
        auto should_compress(const GamePacket& packet) const -> bool;
        
    private:
        mutable std::mutex mutex_;
        
        // Feature flags
        bool compression_enabled_;
        bool encryption_enabled_;
        bool batching_enabled_;
        
        // Encryption
        std::string encryption_key_;
        
        // Statistics
        mutable ProcessorStats stats_;
    };
}
