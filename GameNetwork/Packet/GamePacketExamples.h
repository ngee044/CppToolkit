#pragma once

#include "BinaryGamePacket.h"
#include "../GameNetworkConstants.h"

namespace GameNetwork
{
	// Example: Move packet with binary serialization
	class MovePacket : public BinaryGamePacket
	{
	public:
		MovePacket() : BinaryGamePacket(PacketType::MoveTo) {}
        
		uint64_t entity_id_;
		Location from_location_;
		Location to_location_;
		float speed_;
		uint32_t movement_flags_;
        
	protected:
		auto write_to_buffer(BinaryBuffer& buffer) const -> void override
		{
			buffer.write_uint64(entity_id_);
			write_location(buffer, from_location_);
			write_location(buffer, to_location_);
			buffer.write_float(speed_);
			buffer.write_uint32(movement_flags_);
		}
        
		auto read_from_buffer(BinaryBuffer& buffer) -> bool override
		{
			auto [id_success, id] = buffer.read_uint64();
			auto [from_success, from] = read_location(buffer);
			auto [to_success, to] = read_location(buffer);
			auto [speed_success, speed] = buffer.read_float();
			auto [flags_success, flags] = buffer.read_uint32();
            
			if (!id_success || !from_success || !to_success || 
				!speed_success || !flags_success)
			{
				return false;
			}
            
			entity_id_ = id;
			from_location_ = from;
			to_location_ = to;
			speed_ = speed;
			movement_flags_ = flags;
            
			return true;
		}
        
		auto clone() const -> std::unique_ptr<GamePacket> override
		{
			auto packet = std::make_unique<MovePacket>();
			packet->entity_id_ = entity_id_;
			packet->from_location_ = from_location_;
			packet->to_location_ = to_location_;
			packet->speed_ = speed_;
			packet->movement_flags_ = movement_flags_;
			return packet;
		}
	};
    
	// Example: Chat packet with binary serialization
	class ChatPacket : public BinaryGamePacket
	{
	public:
		ChatPacket() : BinaryGamePacket(PacketType::ChatMessage) {}
        
		uint64_t sender_id_;
		std::string sender_name_;
		std::string message_;
		uint8_t channel_;
        
	protected:
		auto write_to_buffer(BinaryBuffer& buffer) const -> void override
		{
			buffer.write_uint64(sender_id_);
			buffer.write_string(sender_name_);
			buffer.write_string(message_);
			buffer.write_uint8(channel_);
		}
        
		auto read_from_buffer(BinaryBuffer& buffer) -> bool override
		{
			auto [id_success, id] = buffer.read_uint64();
			auto [name_success, name] = buffer.read_string();
			auto [msg_success, msg] = buffer.read_string();
			auto [ch_success, ch] = buffer.read_uint8();
            
			if (!id_success || !name_success || !msg_success || !ch_success)
			{
				return false;
			}
            
			sender_id_ = id;
			sender_name_ = name;
			message_ = msg;
			channel_ = ch;
            
			return true;
		}
        
		auto clone() const -> std::unique_ptr<GamePacket> override
		{
			auto packet = std::make_unique<ChatPacket>();
			packet->sender_id_ = sender_id_;
			packet->sender_name_ = sender_name_;
			packet->message_ = message_;
			packet->channel_ = channel_;
			return packet;
		}
	};
}