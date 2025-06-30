#include "GamePacket_Binary.h"
#include "../../Utilities/Logger.h"
#include <cstring>
#include <zlib.h>

namespace GameNetwork
{
	// GamePacket implementation
	GamePacket::GamePacket()
		: type_(PacketType::Invalid)
		, serialization_type_(PacketSerializationType::Binary)
		, timestamp_(std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch()))
	{
	}

	GamePacket::GamePacket(PacketType type)
		: type_(type)
		, serialization_type_(PacketSerializationType::Binary)
		, timestamp_(std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch()))
	{
	}

	GamePacket::~GamePacket() = default;

	auto GamePacket::get_type() const -> PacketType
	{
		return type_;
	}

	auto GamePacket::get_timestamp() const -> std::chrono::microseconds
	{
		return timestamp_;
	}

	auto GamePacket::set_type(PacketType type) -> void
	{
		type_ = type;
	}

	auto GamePacket::get_serialization_type() const -> PacketSerializationType
	{
		return serialization_type_;
	}

	auto GamePacket::set_serialization_type(PacketSerializationType type) -> void
	{
		serialization_type_ = type;
	}
	auto GamePacket::to_binary() const -> std::vector<uint8_t>
	{
		Serialization::BinarySerializer serializer;
        
		// Write packet header
		BinaryPacketHeader header;
		header.version = kCurrentProtocolVersion;
		header.type = serialization_type_;
		header.packet_type = static_cast<uint16_t>(type_);
		header.timestamp = timestamp_.count();
		header.payload_size = 0;  // Will be updated later
		header.checksum = 0;      // Will be calculated later
        
		// Reserve space for header
		serializer.write_bytes(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
        
		// Mark payload start position
		size_t payload_start = serializer.get_size();
        
		// Write packet-specific data
		serialize_to_binary(serializer);
        
		// Write generic data map
		serializer.write_uint32(static_cast<uint32_t>(data_.size()));
		for (const auto& [key, value] : data_)
		{
			serializer.write_string(key);
			// Serialize boost::json::value to binary
			// We'll use a simple type prefix system
			if (value.is_null())
			{
				serializer.write_uint8(0); // Type: null
			}
			else if (value.is_bool())
			{
				serializer.write_uint8(1); // Type: bool
				serializer.write_uint8(value.as_bool() ? 1 : 0);
			}
			else if (value.is_int64())
			{
				serializer.write_uint8(2); // Type: int64
				serializer.write_int64(value.as_int64());
			}
			else if (value.is_uint64())
			{
				serializer.write_uint8(3); // Type: uint64
				serializer.write_uint64(value.as_uint64());
			}
			else if (value.is_double())
			{
				serializer.write_uint8(4); // Type: double
				serializer.write_float64(value.as_double());
			}
			else if (value.is_string())
			{
				serializer.write_uint8(5); // Type: string
				serializer.write_string(value.as_string().c_str());
			}
			else if (value.is_array())
			{
				serializer.write_uint8(6); // Type: array
				serializer.write_string(boost::json::serialize(value));
			}
			else if (value.is_object())
			{
				serializer.write_uint8(7); // Type: object
				serializer.write_string(boost::json::serialize(value));
			}
			else
			{
				// Default: serialize as string
				serializer.write_uint8(5); // Type: string
				serializer.write_string(boost::json::serialize(value));
			}
		}
        
		// Update payload size in header
		auto data = serializer.get_data();
		size_t payload_size = data.size() - payload_start;
        
		BinaryPacketHeader* header_ptr = reinterpret_cast<BinaryPacketHeader*>(
			const_cast<uint8_t*>(data.data()));
		header_ptr->payload_size = static_cast<uint32_t>(payload_size);
        
		// Calculate checksum (CRC32)
		header_ptr->checksum = crc32(0, data.data() + payload_start, payload_size);
        
		return serializer.extract_data();
	}

	auto GamePacket::from_binary(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < sizeof(BinaryPacketHeader))
		{
			Utilities::Logger::error("Binary packet too small for header");
			return false;
		}
        
		// Read header
		BinaryPacketHeader header;
		std::memcpy(&header, data.data(), sizeof(header));
        
		// Validate header
		if (header.version != kCurrentProtocolVersion)
		{
			Utilities::Logger::error("Unsupported protocol version: " + 
				std::to_string(header.version));
			return false;
		}
        
		if (data.size() < sizeof(header) + header.payload_size)
		{
			Utilities::Logger::error("Binary packet size mismatch");
			return false;
		}
        
		// Verify checksum
		uint32_t calculated_checksum = crc32(0, 
			data.data() + sizeof(header), header.payload_size);
		if (calculated_checksum != header.checksum)
		{
			Utilities::Logger::error("Binary packet checksum mismatch");
			return false;
		}
        
		// Set packet properties
		type_ = static_cast<PacketType>(header.packet_type);
		serialization_type_ = header.type;
		timestamp_ = std::chrono::microseconds(header.timestamp);
        
		// Deserialize payload
		Serialization::BinaryDeserializer deserializer(
			data.data() + sizeof(header), header.payload_size);
        
		// Read packet-specific data
		if (!deserialize_from_binary(deserializer))
		{
			return false;
		}
        
		// Read generic data map
		auto [data_size, size_ok] = deserializer.read_uint32();
		if (!size_ok)
		{
			return false;
		}
        
		data_.clear();
		for (uint32_t i = 0; i < data_size; ++i)
		{
			auto [key, key_ok] = deserializer.read_string();
			auto [value_str, value_ok] = deserializer.read_string();
            
			if (!key_ok || !value_ok)
			{
				return false;
			}
            
			try
			{
				data_[key] = boost::json::parse(value_str);
			}
			catch (const std::exception& e)
			{
				Utilities::Logger::error("Failed to parse JSON in binary packet: " + 
					std::string(e.what()));
				return false;
			}
		}
        
		return true;
	}

	auto GamePacket::serialize() const -> std::vector<uint8_t>
	{
		switch (serialization_type_)
		{
			case PacketSerializationType::Binary:
			case PacketSerializationType::Compressed:
				return to_binary();
                
			case PacketSerializationType::Json:
			default:
			{
				auto json_str = boost::json::serialize(to_json());
				return std::vector<uint8_t>(json_str.begin(), json_str.end());
			}
		}
	}

	auto GamePacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		// Try to detect format
		if (data.size() >= sizeof(BinaryPacketHeader))
		{
			BinaryPacketHeader header;
			std::memcpy(&header, data.data(), sizeof(header));
            
			if (header.version == kCurrentProtocolVersion &&
				(header.type == PacketSerializationType::Binary ||
				 header.type == PacketSerializationType::Compressed))
			{
				return from_binary(data);
			}
		}
        
		// Fall back to JSON
		try
		{
			std::string json_str(data.begin(), data.end());
			auto json = boost::json::parse(json_str).as_object();
			return from_json(json);
		}
		catch (const std::exception& e)
		{
			Utilities::Logger::error("Failed to deserialize packet: " + 
				std::string(e.what()));
			return false;
		}
	}

	// Default implementations for virtual functions
	auto GamePacket::serialize_to_binary(Serialization::BinarySerializer& serializer) const -> void
	{
		// Base class has no specific data
	}

	auto GamePacket::deserialize_from_binary(Serialization::BinaryDeserializer& deserializer) -> bool
	{
		// Base class has no specific data
		return true;
	}
(const uint8_t*>(&original_size),
					reinterpret_cast<const uint8_t*>(&original_size) + sizeof(original_size));
				final_data.insert(final_data.end(), compressed_data.begin(), compressed_data.end());
                
				serialized_data = std::move(final_data);
				stats_.compression_ratio = static_cast<double>(compressed_size) / original_size;
                
				Utilities::Logger::debug("Packet compressed: " + 
					std::to_string(original_size) + " -> " + 
					std::to_string(compressed_size) + " bytes");
			}
		}
        
		stats_.packets_sent++;
		stats_.bytes_sent += serialized_data.size();
        
		// Fragment if necessary
		auto fragments = fragmentation_manager_->prepare_for_send(serialized_data);
        
		if (fragments.size() > 1)
		{
			stats_.packets_fragmented++;
			stats_.fragments_sent += fragments.size();
		}
        
		return fragments;
	}

	auto PacketTransmitter::process_received_data(const std::vector<uint8_t>& data) 
		-> std::tuple<std::unique_ptr<GamePacket>, bool>
	{
		stats_.fragments_received++;
        
		// Process through fragmentation manager
		auto [is_complete, complete_data] = fragmentation_manager_->process_received(data);
        
		if (!is_complete)
		{
			return {nullptr, false};
		}
        
		stats_.packets_received++;
		stats_.bytes_received += complete_data->size();
        
		// Check if data is compressed
		std::vector<uint8_t> packet_data = *complete_data;
        
		if (compression_enabled_ && packet_data.size() > sizeof(uint32_t))
		{
			// Check for compression header
			uint32_t possible_original_size;
			std::memcpy(&possible_original_size, packet_data.data(), sizeof(uint32_t));
            
			if (possible_original_size > packet_data.size() && 
				possible_original_size < packet_data.size() * 100)  // Sanity check
			{
				// Looks like compressed data
				std::vector<uint8_t> decompressed_data(possible_original_size);
				uLongf decompressed_size = possible_original_size;
                
				int result = uncompress(
					decompressed_data.data(), &decompressed_size,
					packet_data.data() + sizeof(uint32_t), 
					packet_data.size() - sizeof(uint32_t)
				);
                
				if (result == Z_OK)
				{
					packet_data = std::move(decompressed_data);
					Utilities::Logger::debug("Packet decompressed: " + 
						std::to_string(complete_data->size()) + " -> " + 
						std::to_string(decompressed_size) + " bytes");
				}
			}
		}
        
		// Create packet from data
		auto [packet, success] = PacketFactory::create_from_data(packet_data);
		return {std::move(packet), success};
	}

	auto PacketTransmitter::set_mtu(size_t mtu) -> void
	{
		fragmentation_manager_->set_mtu(mtu);
	}

	auto PacketTransmitter::set_default_serialization(PacketSerializationType type) -> void
	{
		default_serialization_ = type;
	}

	auto PacketTransmitter::enable_compression(bool enable) -> void
	{
		compression_enabled_ = enable;
	}

	auto PacketTransmitter::get_statistics() const -> Statistics
	{
		auto frag_stats = fragmentation_manager_->get_statistics();
        
		Statistics stats = stats_;
		stats.packets_fragmented = frag_stats.messages_fragmented;
		stats.fragments_sent = frag_stats.fragments_sent;
		stats.fragments_received = frag_stats.fragments_received;
        
		return stats;
	}
}
	// PacketFactory implementation
	std::unordered_map<PacketType, std::function<std::unique_ptr<GamePacket>()>> packet_creators;

	auto PacketFactory::create_packet(PacketType type, PacketSerializationType serialization) 
		-> std::unique_ptr<GamePacket>
	{
		auto it = packet_creators.find(type);
		if (it != packet_creators.end())
		{
			auto packet = it->second();
			packet->set_serialization_type(serialization);
			return packet;
		}
        
		// Default packet
		auto packet = std::make_unique<GamePacket>(type);
		packet->set_serialization_type(serialization);
		return packet;
	}

	auto PacketFactory::create_from_data(const std::vector<uint8_t>& data) 
		-> std::tuple<std::unique_ptr<GamePacket>, bool>
	{
		// First, try to determine packet type
		if (data.size() >= sizeof(BinaryPacketHeader))
		{
			BinaryPacketHeader header;
			std::memcpy(&header, data.data(), sizeof(header));
            
			if (header.version == kCurrentProtocolVersion)
			{
				// Binary packet
				auto packet_type = static_cast<PacketType>(header.packet_type);
				auto packet = create_packet(packet_type, header.type);
                
				if (packet->from_binary(data))
				{
					return {std::move(packet), true};
				}
			}
		}
        
		// Try JSON fallback
		try
		{
			std::string json_str(data.begin(), data.end());
			auto json = boost::json::parse(json_str).as_object();
            
			if (json.contains("type"))
			{
				auto type = static_cast<PacketType>(json["type"].as_int64());
				auto packet = create_packet(type, PacketSerializationType::Json);
                
				if (packet->from_json(json))
				{
					return {std::move(packet), true};
				}
			}
		}
		catch (...)
		{
			// Not JSON
		}
        
		return {nullptr, false};
	}

	// Template implementation needs to be in header, but here's the concept:
	// template<typename T>
	// auto PacketFactory::register_packet_type(PacketType type) -> void
	// {
	//     packet_creators[type] = []() { return std::make_unique<T>(); };
	// }
