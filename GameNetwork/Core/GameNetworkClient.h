#pragma once

#include "GameNetworkConstants.h"
#include <NetworkClient.h>
#include <NetworkSession.h>
#include <ThreadPool.h>
#include <Logger.h>

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
				if (data.size() < 4)
					return std::nullopt;
				Packet packet;
				packet.data = data;
				packet.type = 0; // TODO: parse actual packet type
				return packet;
			}
		};

		class MessageDispatcher
		{
		public:
			void dispatch(PacketProcessor::Packet&& packet)
			{
				// TODO: Implement message dispatching
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
