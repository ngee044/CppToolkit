#pragma once

#include "GameNetworkConstants.h"
#include "GamePacket.h"

#include <NetworkClient.h>
#include <NetworkSession.h>
#include <ThreadPool.h>
#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <optional>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <mutex>

using namespace Utilities;

namespace GameNetwork
{
	// Forward declarations
	class DisconnectionHandler;
	class GameSession;
	class GamePacket;

	struct ClientConfig
	{
		std::string server_ip;
		uint16_t server_port;
		std::string client_id;
		bool auto_reconnect = false;
		uint32_t max_buffer_size = 1024 * 1024;
		bool enable_heartbeat = true;
		uint32_t heartbeat_interval = 30000;

		// Reconnection settings
		uint32_t max_reconnect_attempts = 5;
		uint32_t reconnect_timeout_seconds = 300;
		bool enable_exponential_backoff = true;
	};

	class GameNetworkClient
	{
	public:
		explicit GameNetworkClient(const ClientConfig& config);
		virtual ~GameNetworkClient();

		// Basic connection methods
		auto initialize() -> std::tuple<bool, std::optional<std::string>>;
		auto shutdown() -> void;
		auto connect() -> std::tuple<bool, std::optional<std::string>>;
		auto disconnect() -> void;
		auto is_connected() const -> bool;
		auto get_connection_state() const -> ConnectionState;

		// Packet handling
		auto send_packet(const GamePacket& packet) -> bool;

	private:
		// Event handlers
		auto on_connected(std::shared_ptr<Network::NetworkSession> session) -> void;
		auto on_disconnected(std::shared_ptr<Network::NetworkSession> session) -> void;
		auto on_data_received(std::shared_ptr<Network::NetworkSession> session, const std::vector<uint8_t>& data) -> void;

		// Internal processing
		auto process_received_data(const std::vector<uint8_t>& data) -> void;
		auto handle_packet(const GamePacket& packet) -> void;

	private:
		ClientConfig config_;
		std::unique_ptr<Network::NetworkClient> network_client_;
		std::shared_ptr<Thread::ThreadPool> thread_pool_;
		std::shared_ptr<Network::NetworkSession> current_session_;

		// Forward declarations for missing members
		class PacketProcessor
		{
		public:
			struct Packet
			{
				std::vector<uint8_t> data;
				uint32_t type;
			};

			std::optional<Packet> deserialize_binary(const std::vector<uint8_t>& data)
			{
				if (data.size() < 8) // Minimum header size
					return std::nullopt;
				
				Packet packet;
				
				// Read packet header
				uint32_t packet_type = *reinterpret_cast<const uint32_t*>(data.data());
				uint32_t payload_size = *reinterpret_cast<const uint32_t*>(data.data() + 4);
				
				if (data.size() < 8 + payload_size)
					return std::nullopt;
				
				packet.type = packet_type;
				packet.data.assign(data.begin() + 8, data.begin() + 8 + payload_size);
				
				return packet;
			}
		};

		class MessageDispatcher
		{
		public:
			void dispatch(PacketProcessor::Packet&& packet)
			{
				switch (packet.type)
				{
				case static_cast<uint32_t>(PacketType::Heartbeat):
					Logger::handle().write(LogTypes::Debug, "Received heartbeat packet");
					break;
					
				case static_cast<uint32_t>(PacketType::GameData):
					Logger::handle().write(LogTypes::Debug, fmt::format("Received game data packet: {} bytes", packet.data.size()));
					break;
					
				case static_cast<uint32_t>(PacketType::ServerCommand):
					Logger::handle().write(LogTypes::Debug, "Received server command packet");
					break;
					
				default:
					Logger::handle().write(LogTypes::Error, fmt::format("Unknown packet type received: {}", packet.type));
					break;
				}
			}
		};

		struct NetworkStats
		{
			std::atomic<uint64_t> packets_received{ 0 };
			std::atomic<uint64_t> bytes_received{ 0 };
			std::atomic<uint64_t> packets_sent{ 0 };
			std::atomic<uint64_t> bytes_sent{ 0 };
		};

		std::unique_ptr<PacketProcessor> packet_processor_;
		std::unique_ptr<MessageDispatcher> message_dispatcher_;
		NetworkStats stats_;

		bool is_running_;
		ConnectionState connection_state_;

		mutable std::mutex mutex_;
	};
}
