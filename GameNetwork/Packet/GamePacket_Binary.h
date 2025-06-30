#pragma once

#include "../GameNetworkConstants.h"
#include "../Serialization/BinarySerializer.h"
#include "PacketFragmentation.h"

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <variant>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <boost/json.hpp>

namespace GameNetwork
{
	enum class PacketSerializationType : uint8_t
	{
		Json = 0,      // Legacy JSON format
		Binary = 1,    // New binary format
		Compressed = 2 // Binary + compression
	};

	// Packet header for binary format
	struct BinaryPacketHeader
	{
		uint8_t version;                    // Protocol version
		PacketSerializationType type;       // Serialization type
		uint16_t packet_type;              // PacketType enum value
		uint32_t payload_size;             // Size of payload
		uint32_t checksum;                 // CRC32 checksum
		uint64_t timestamp;                // Timestamp in microseconds
	};

	constexpr size_t kBinaryPacketHeaderSize = sizeof(BinaryPacketHeader);
	constexpr uint8_t kCurrentProtocolVersion = 1;

	// Binary serialization traits for game types
	template<typename T>
	struct BinarySerializable
	{
		static auto serialize(Serialization::BinarySerializer& serializer, const T& value) -> void;
		static auto deserialize(Serialization::BinaryDeserializer& deserializer) -> std::tuple<T, bool>;
	};

	// Specializations for common game types
	template<>
	struct BinarySerializable<Location>
	{
		static auto serialize(Serialization::BinarySerializer& serializer, const Location& loc) -> void
		{
			serializer.write_float(loc.x);
			serializer.write_float(loc.y);
			serializer.write_float(loc.z);
			serializer.write_uint32(loc.map_id);
			serializer.write_uint32(loc.zone_id);
		}

		static auto deserialize(Serialization::BinaryDeserializer& deserializer) -> std::tuple<Location, bool>
		{
			Location loc;
			auto [x, x_ok] = deserializer.read_float();
			auto [y, y_ok] = deserializer.read_float();
			auto [z, z_ok] = deserializer.read_float();
			auto [map_id, map_ok] = deserializer.read_uint32();
			auto [zone_id, zone_ok] = deserializer.read_uint32();
            
			if (!x_ok || !y_ok || !z_ok || !map_ok || !zone_ok)
			{
				return {Location{}, false};
			}
            
			loc.x = x;
			loc.y = y;
			loc.z = z;
			loc.map_id = map_id;
			loc.zone_id = zone_id;
			return {loc, true};
		}
	};

	// Enhanced GamePacket with binary support
	class GamePacket
	{
	public:
		GamePacket();
		explicit GamePacket(PacketType type);
		virtual ~GamePacket();

		// Type and timestamp
		auto get_type() const -> PacketType;
		auto get_timestamp() const -> std::chrono::microseconds;
		auto set_type(PacketType type) -> void;

		// Serialization format
		auto get_serialization_type() const -> PacketSerializationType;
		auto set_serialization_type(PacketSerializationType type) -> void;

		// JSON serialization (legacy)
		auto to_json() const -> boost::json::object;
		auto from_json(const boost::json::object& json) -> bool;

		// Binary serialization (new)
		auto to_binary() const -> std::vector<uint8_t>;
		auto from_binary(const std::vector<uint8_t>& data) -> bool;

		// Automatic serialization based on type
		auto serialize() const -> std::vector<uint8_t>;
		auto deserialize(const std::vector<uint8_t>& data) -> bool;

		// Data access
		template<typename T>
		auto set_data(const std::string& key, const T& value) -> void;
        
		template<typename T>
		auto get_data(const std::string& key) const -> std::optional<T>;

	protected:
		// Override these for custom packets
		virtual auto serialize_to_json(boost::json::object& json) const -> void;
		virtual auto deserialize_from_json(const boost::json::object& json) -> bool;
        
		virtual auto serialize_to_binary(Serialization::BinarySerializer& serializer) const -> void;
		virtual auto deserialize_from_binary(Serialization::BinaryDeserializer& deserializer) -> bool;

	private:
		PacketType type_;
		PacketSerializationType serialization_type_;
		std::chrono::microseconds timestamp_;
		std::unordered_map<std::string, boost::json::value> data_;
	};

	// Factory for creating packets with proper serialization
	class PacketFactory
	{
	public:
		static auto create_packet(PacketType type, PacketSerializationType serialization = PacketSerializationType::Binary) 
			-> std::unique_ptr<GamePacket>;

		static auto create_from_data(const std::vector<uint8_t>& data) 
			-> std::tuple<std::unique_ptr<GamePacket>, bool>;

		// Register custom packet types
		template<typename T>
		static auto register_packet_type(PacketType type) -> void;
	};

	// Integration with fragmentation
	class PacketTransmitter
	{
	public:
		explicit PacketTransmitter(size_t mtu = kDefaultMTU);
		~PacketTransmitter();

		// Send a packet (may result in multiple fragments)
		auto prepare_packet_for_send(const GamePacket& packet) 
			-> std::vector<std::vector<uint8_t>>;

		// Process received data (may be a fragment)
		auto process_received_data(const std::vector<uint8_t>& data) 
			-> std::tuple<std::unique_ptr<GamePacket>, bool>;

		// Configuration
		auto set_mtu(size_t mtu) -> void;
		auto set_default_serialization(PacketSerializationType type) -> void;
		auto enable_compression(bool enable) -> void;

		// Statistics
		struct Statistics
		{
			uint64_t packets_sent;
			uint64_t packets_received;
			uint64_t bytes_sent;
			uint64_t bytes_received;
			uint64_t packets_fragmented;
			uint64_t fragments_sent;
			uint64_t fragments_received;
			double compression_ratio;
		};

		auto get_statistics() const -> Statistics;

	private:
		std::unique_ptr<Packet::FragmentationManager> fragmentation_manager_;
		PacketSerializationType default_serialization_;
		bool compression_enabled_;
		Statistics stats_;
	};
}