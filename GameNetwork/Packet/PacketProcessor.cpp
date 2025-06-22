#include "PacketProcessor.h"

#ifdef USE_COMPRESSION
#include <zlib.h>
#endif

#include <cstring>
#include <algorithm>

namespace GameNetwork
{
    PacketProcessor::PacketProcessor(std::shared_ptr<Thread::ThreadPool> thread_pool)
        : thread_pool_(thread_pool)
        , compression_enabled_(false)
        , encryption_enabled_(false)
        , batching_enabled_(false)
        , max_batch_size_(1400)  // MTU safe
        , batch_timeout_(std::chrono::milliseconds(100))
    {
        stats_ = {};
        
        if (!thread_pool_)
        {
            // Create default thread pool if not provided
            thread_pool_ = std::make_shared<Thread::ThreadPool>("PacketProcessor");
        }
    }
    
    PacketProcessor::~PacketProcessor() = default;
    
    auto PacketProcessor::serialize_packet(const GamePacket& packet) 
        -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>
    {
        try
        {
            // Get raw packet data
            auto data = packet.serialize();
            
            // Validate packet size
            if (!validate_packet_size(data.size()))
            {
                return {{}, "Packet size exceeds maximum"};
            }
            
            // Apply compression if enabled
            if (compression_enabled_ && should_compress(packet))
            {
                auto [compressed, error] = compress_packet(data);
                if (error.has_value())
                {
                    return {{}, error};
                }
                data = compressed;
                stats_.packets_compressed++;
            }
            
            // Apply encryption if enabled
            if (encryption_enabled_)
            {
                auto [encrypted, error] = encrypt_packet(data);
                if (error.has_value())
                {
                    return {{}, error};
                }
                data = encrypted;
                stats_.packets_encrypted++;
            }
            
            stats_.packets_processed++;
            
            return {data, std::nullopt};
        }
        catch (const std::exception& e)
        {
            return {{}, std::string("Serialization error: ") + e.what()};
        }
    }
    
    auto PacketProcessor::deserialize_packet(const std::vector<uint8_t>& data) 
        -> std::tuple<std::unique_ptr<GamePacket>, std::optional<std::string>>
    {
        try
        {
            std::vector<uint8_t> processed_data = data;
            
            // Decrypt if enabled
            if (encryption_enabled_)
            {
                auto [decrypted, error] = decrypt_packet(processed_data);
                if (error.has_value())
                {
                    return {nullptr, error};
                }
                processed_data = decrypted;
            }
            
            // Decompress if needed
            if (compression_enabled_)
            {
                // Check if packet is compressed (would need a flag in the header)
                // For now, try to decompress and fall back if it fails
                auto [decompressed, error] = decompress_packet(processed_data);
                if (!error.has_value())
                {
                    processed_data = decompressed;
                }
            }
            
            // Deserialize packet
            auto [packet, error] = GamePacket::deserialize(processed_data);
            if (error.has_value())
            {
                return {nullptr, error};
            }
            
            // Validate packet
            auto [valid, validation_error] = validate_packet(*packet);
            if (!valid)
            {
                return {nullptr, validation_error};
            }
            
            stats_.packets_processed++;
            
            return {std::move(packet), std::nullopt};
        }
        catch (const std::exception& e)
        {
            return {nullptr, std::string("Deserialization error: ") + e.what()};
        }
    }
    
    auto PacketProcessor::enable_compression(bool enable) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        compression_enabled_ = enable;
    }
    
    auto PacketProcessor::is_compression_enabled() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return compression_enabled_;
    }
    
    auto PacketProcessor::compress_packet(const std::vector<uint8_t>& data) 
        -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>
    {
#ifdef USE_COMPRESSION
        // Use zlib compression
        uLongf compressed_size = compressBound(data.size());
        std::vector<uint8_t> compressed(compressed_size);
        
        int result = compress2(compressed.data(), &compressed_size,
                               data.data(), data.size(),
                               Z_BEST_SPEED);  // Fast compression
        
        if (result != Z_OK)
        {
            return {{}, "Compression failed"};
        }
        
        compressed.resize(compressed_size);
        
        // Calculate compression ratio
        if (data.size() > 0)
        {
            stats_.compression_ratio_percent = 
                (compressed_size * 100) / data.size();
        }
        
        return {compressed, std::nullopt};
#else
        // No compression available
        return {data, std::nullopt};
#endif
    }
    
    auto PacketProcessor::decompress_packet(const std::vector<uint8_t>& data) 
        -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>
    {
#ifdef USE_COMPRESSION
        // Estimate decompressed size (would need to store this in packet header)
        uLongf decompressed_size = data.size() * 10;  // Assume 10x compression max
        std::vector<uint8_t> decompressed(decompressed_size);
        
        int result = uncompress(decompressed.data(), &decompressed_size,
                                data.data(), data.size());
        
        if (result != Z_OK)
        {
            return {{}, "Decompression failed"};
        }
        
        decompressed.resize(decompressed_size);
        return {decompressed, std::nullopt};
#else
        // No compression available
        return {data, std::nullopt};
#endif
    }
    
    auto PacketProcessor::enable_encryption(bool enable) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        encryption_enabled_ = enable;
    }
    
    auto PacketProcessor::is_encryption_enabled() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return encryption_enabled_;
    }
    
    auto PacketProcessor::set_encryption_key(const std::vector<uint8_t>& key) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        encryption_key_ = key;
    }
    
    auto PacketProcessor::encrypt_packet(const std::vector<uint8_t>& data) 
        -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>
    {
        // Simple XOR encryption for demonstration
        // In production, use proper encryption like AES
        if (encryption_key_.empty())
        {
            return {{}, "Encryption key not set"};
        }
        
        std::vector<uint8_t> encrypted = data;
        for (size_t i = 0; i < encrypted.size(); ++i)
        {
            encrypted[i] ^= encryption_key_[i % encryption_key_.size()];
        }
        
        return {encrypted, std::nullopt};
    }
    
    auto PacketProcessor::decrypt_packet(const std::vector<uint8_t>& data) 
        -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>
    {
        // XOR decryption (same as encryption for XOR)
        return encrypt_packet(data);
    }
    
    auto PacketProcessor::enable_batching(bool enable) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batching_enabled_ = enable;
        
        if (!enable && !current_batch_.serialized_packets.empty())
        {
            // Flush current batch
            flush_batch();
        }
    }
    
    auto PacketProcessor::is_batching_enabled() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return batching_enabled_;
    }
    
    auto PacketProcessor::add_to_batch(const GamePacket& packet) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!batching_enabled_)
        {
            return;
        }
        
        // Check if we need to flush based on size or time
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - current_batch_.created_time;
        
        if (current_batch_.total_size >= max_batch_size_ || 
            elapsed >= batch_timeout_)
        {
            flush_batch();
        }
        
        // Add packet to batch
        auto packet_data = packet.serialize();
        current_batch_.total_size += packet_data.size();
        current_batch_.serialized_packets.push_back(std::move(packet_data));
        
        stats_.packets_batched++;
    }
    
    auto PacketProcessor::flush_batch() -> std::vector<std::vector<uint8_t>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto result = process_batch();
        
        // Clear current batch
        current_batch_.serialized_packets.clear();
        current_batch_.total_size = 0;
        current_batch_.created_time = std::chrono::steady_clock::now();
        
        return result;
    }
    
    auto PacketProcessor::validate_packet(const GamePacket& packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Basic validation
        if (!validate_packet_type(packet.type()))
        {
            return {false, "Invalid packet type"};
        }
        
        // Packet-specific validation
        auto [valid, error] = packet.validate();
        if (!valid)
        {
            return {false, error};
        }
        
        return {true, std::nullopt};
    }
    
    auto PacketProcessor::validate_packet_size(size_t size) const -> bool
    {
        return size <= MAX_PACKET_SIZE;
    }
    
    auto PacketProcessor::validate_packet_type(PacketType type) const -> bool
    {
        // All packet types are valid for now
        return true;
    }
    
    auto PacketProcessor::get_stats() const -> ProcessorStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Calculate average processing time
        if (stats_.packets_processed > 0)
        {
            // This would be calculated from actual timing data
            stats_.average_processing_time_us = 100;  // Placeholder
        }
        
        return stats_;
    }
    
    auto PacketProcessor::reset_stats() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = {};
    }
    
    auto PacketProcessor::calculate_checksum(const std::vector<uint8_t>& data) const -> uint32_t
    {
        // Simple checksum for demonstration
        uint32_t checksum = 0;
        for (const auto& byte : data)
        {
            checksum = (checksum << 1) ^ byte;
        }
        return checksum;
    }
    
    auto PacketProcessor::process_batch() -> std::vector<std::vector<uint8_t>>
    {
        std::vector<std::vector<uint8_t>> result;
        
        // Create batch packet containing all packets
        // In real implementation, would create a special BatchPacket type
        
        for (const auto& packet_data : current_batch_.serialized_packets)
        {
            // Data is already serialized
            result.push_back(packet_data);
        }
        
        return result;
    }
    
    auto PacketProcessor::should_compress(const GamePacket& packet) const -> bool
    {
        // Compress packets larger than 100 bytes  
        // First serialize to check size
        auto data = packet.serialize();
        return data.size() > 100;
    }
}
