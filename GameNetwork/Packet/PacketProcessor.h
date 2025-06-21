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
        PacketProcessor(std::shared_ptr<Thread::ThreadPool> thread_pool = nullptr);
        virtual ~PacketProcessor();
        
        // Packet serialization
        auto serialize_packet(const GamePacket& packet) 
            -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>;
        auto deserialize_packet(const std::vector<uint8_t>& data) 
            -> std::tuple<std::unique_ptr<GamePacket>, std::optional<std::string>>;
        
        // Packet compression
        auto enable_compression(bool enable) -> void;
        auto is_compression_enabled() const -> bool;
        auto compress_packet(const std::vector<uint8_t>& data) 
            -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>;
        auto decompress_packet(const std::vector<uint8_t>& data) 
            -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>;
        
        // Packet encryption
        auto enable_encryption(bool enable) -> void;
        auto is_encryption_enabled() const -> bool;
        auto set_encryption_key(const std::vector<uint8_t>& key) -> void;
        auto encrypt_packet(const std::vector<uint8_t>& data) 
            -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>;
        auto decrypt_packet(const std::vector<uint8_t>& data) 
            -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>;
        
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
            std::vector<std::unique_ptr<GamePacket>> packets;
            std::chrono::steady_clock::time_point created_time;
            size_t total_size;
        };
        
        auto calculate_checksum(const std::vector<uint8_t>& data) const -> uint32_t;
        auto process_batch() -> std::vector<std::vector<uint8_t>>;
        
    private:
        mutable std::mutex mutex_;
        
        // Thread pool for async processing
        std::shared_ptr<Thread::ThreadPool> thread_pool_;
        
        // Feature flags
        bool compression_enabled_;
        bool encryption_enabled_;
        bool batching_enabled_;
        
        // Encryption
        std::vector<uint8_t> encryption_key_;
        
        // Batching
        PacketBatch current_batch_;
        size_t max_batch_size_;
        std::chrono::milliseconds batch_timeout_;
        
        // Statistics
        ProcessorStats stats_;
    };
}
