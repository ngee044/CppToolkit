#include "BinaryGamePacket.h"
#include "../../Utilities/Logger.h"
#include "../../Utilities/Converter.h"
#include <boost/json.hpp>

using namespace Utilities;

namespace GameNetwork
{
    BinaryGamePacket::BinaryGamePacket()
        : GamePacket()
        , serialization_type_(SerializationType::Binary)
        , timestamp_(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()))
    {
    }
    
    BinaryGamePacket::BinaryGamePacket(PacketType type)
        : GamePacket(type)
        , serialization_type_(SerializationType::Binary)
        , timestamp_(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()))
    {
    }
    
    BinaryGamePacket::~BinaryGamePacket() = default;
    
    auto BinaryGamePacket::serialize_binary() const -> std::vector<uint8_t>
    {
        BinaryBuffer buffer;
        
        buffer.write_uint8(static_cast<uint8_t>(serialization_type_));
        buffer.write_uint16(static_cast<uint16_t>(get_type()));
        buffer.write_uint64(timestamp_.count());
        
        write_to_buffer(buffer);
        
        return buffer.get_data();
    }
    
    auto BinaryGamePacket::deserialize_binary(const std::vector<uint8_t>& data) -> bool
    {
        BinaryBuffer buffer;
        buffer.write_bytes(data.data(), data.size());
        buffer.reset_read_position();
        
        auto [type_success, type_value] = buffer.read_uint8();
        auto [packet_type_success, packet_type_value] = buffer.read_uint16();
        auto [timestamp_success, timestamp_value] = buffer.read_uint64();
        
        if (!type_success || !packet_type_success || !timestamp_success)
        {
            Logger::handle().write(LogTypes::Error, "BinaryGamePacket: Failed to read header");
            return false;
        }
        
        serialization_type_ = static_cast<SerializationType>(type_value);
        set_type(static_cast<PacketType>(packet_type_value));
        timestamp_ = std::chrono::microseconds(timestamp_value);
        
        return read_from_buffer(buffer);
    }
    
    auto BinaryGamePacket::serialize() const -> std::vector<uint8_t>
    {
        if (serialization_type_ == SerializationType::Binary)
        {
            return serialize_binary();
        }
        else
        {
            // Fall back to JSON
            return GamePacket::serialize();
        }
    }
    
    auto BinaryGamePacket::deserialize(const std::vector<uint8_t>& data) -> bool
    {
        // Try to detect format
        if (!data.empty())
        {
            uint8_t first_byte = data[0];
            if (first_byte == static_cast<uint8_t>(SerializationType::Binary))
            {
                return deserialize_binary(data);
            }
        }
        
        // Fall back to JSON
        return GamePacket::deserialize(data);
    }
    
    auto BinaryGamePacket::set_serialization_type(SerializationType type) -> void
    {
        serialization_type_ = type;
    }
    
    auto BinaryGamePacket::get_serialization_type() const -> SerializationType
    {
        return serialization_type_;
    }
    
    auto BinaryGamePacket::write_to_buffer(BinaryBuffer& buffer) const -> void
    {
        // Base class has no specific data
    }
    
    auto BinaryGamePacket::read_from_buffer(BinaryBuffer& buffer) -> bool
    {
        // Base class has no specific data
        return true;
    }

    auto BinaryGamePacket::get_timestamp() const -> std::chrono::microseconds
    {
        return timestamp_;
    }
    
    auto BinaryGamePacket::set_timestamp(std::chrono::microseconds timestamp) -> void
    {
        timestamp_ = timestamp;
    }

    auto BinaryGamePacket::clone() const -> std::unique_ptr<GamePacket>
    {
        auto cloned = std::make_unique<BinaryGamePacket>();
        cloned->packet_type = packet_type;
        cloned->data = data;
        cloned->set_sequence_number(get_sequence_number());
        cloned->set_sender_id(get_sender_id());
        cloned->set_target_id(get_target_id());
        cloned->serialization_type_ = serialization_type_;
        cloned->timestamp_ = timestamp_;
        return cloned;
    }
}