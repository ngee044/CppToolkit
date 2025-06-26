#pragma once

#include "GamePacket.h"
#include "BinaryBuffer.h"
#include "PacketFragmentation.h"
#include "../GameNetworkConstants.h"

#include <memory>
#include <vector>
#include <cstdint>

namespace GameNetwork 
{
    enum class SerializationType : uint8_t
    {
        Json = 0,
        Binary = 1
    };
    
    class BinaryGamePacket : public GamePacket
    {
    public:
        BinaryGamePacket();
        explicit BinaryGamePacket(PacketType type);
        virtual ~BinaryGamePacket();
        
        // Binary serialization
        auto serialize_binary() const -> std::vector<uint8_t>;
        auto deserialize_binary(const std::vector<uint8_t>& data) -> bool;
        
        // Override virtual functions for binary support
        virtual auto serialize() const -> std::vector<uint8_t> override;
        virtual auto deserialize(const std::vector<uint8_t>& data) -> bool override;
        virtual auto clone() const -> std::unique_ptr<GamePacket> override;
        
        // Get/Set timestamp
        auto get_timestamp() const -> std::chrono::microseconds;
        auto set_timestamp(std::chrono::microseconds timestamp) -> void;
        auto set_serialization_type(SerializationType type) -> void;
        auto get_serialization_type() const -> SerializationType;
        
    protected:
        // Override these in derived classes
        virtual auto write_to_buffer(BinaryBuffer& buffer) const -> void;
        virtual auto read_from_buffer(BinaryBuffer& buffer) -> bool;
        
    private:
        SerializationType serialization_type_;
        std::chrono::microseconds timestamp_;
    };
    
    // Helper for Location serialization
    inline auto write_location(BinaryBuffer& buffer, const Location& loc) -> void
    {
        buffer.write_float(loc.x);
        buffer.write_float(loc.y);
        buffer.write_float(loc.z);
        buffer.write_uint32(loc.map_id);
        buffer.write_uint32(loc.zone_id);
    }
    
    inline auto read_location(BinaryBuffer& buffer) -> std::tuple<bool, Location>
    {
        Location loc;
        
        auto [x_success, x] = buffer.read_float();
        auto [y_success, y] = buffer.read_float();
        auto [z_success, z] = buffer.read_float();
        auto [map_success, map_id] = buffer.read_uint32();
        auto [zone_success, zone_id] = buffer.read_uint32();
        
        if (!x_success || !y_success || !z_success || !map_success || !zone_success)
        {
            return {false, Location{}};
        }
        
        loc.x = x;
        loc.y = y;
        loc.z = z;
        loc.map_id = map_id;
        loc.zone_id = zone_id;
        
        return {true, loc};
    }
}