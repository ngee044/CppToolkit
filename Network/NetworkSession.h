#pragma once

#include "FileManager.h"
#include "DataHandler.h"
#include "Protocol.h"

#include <map>
#include <memory>
#include <string>
#include <chrono>
#include <expected>
#include <boost/asio/steady_timer.hpp>
#include <atomic>

namespace Network
{
	class NetworkSession : public std::enable_shared_from_this<NetworkSession>, public DataHandler
	{
	public:
#ifdef USE_ENCRYPT_MODULE
		NetworkSession(const std::string& id,
					   bool encrypt,
					   uint16_t high_priority_count,
					   uint16_t normal_priority_count,
					   uint16_t low_priority_count,
					   bool heartbeat_enabled = false,
					   uint32_t heartbeat_interval_sec = 30);
#else
		NetworkSession(const std::string& id,
					   uint16_t high_priority_count,
					   uint16_t normal_priority_count,
					   uint16_t low_priority_count,
					   bool heartbeat_enabled = false,
					   uint32_t heartbeat_interval_sec = 30);
#endif
		virtual ~NetworkSession(void);

		auto session_id(void) const -> SessionId;
		auto session_id(SessionId id) -> void;
		auto state(void) const -> SessionState;
		auto get_ptr(void) -> std::shared_ptr<NetworkSession>;

		// Heartbeat control: enable/disable and set interval (seconds)
		auto heartbeat(bool enable, uint32_t interval_sec) -> void;

		// Configure how many heartbeat intervals can be missed before expiring
		auto set_max_missed_heartbeats(uint32_t count) -> void;

		auto start(std::shared_ptr<boost::asio::ip::tcp::socket> socket, size_t socket_buffer_size) -> void;
		auto stop(void) -> void;

		auto register_key(const std::string& key) -> void;

		auto received_connection_callback(const std::function<std::expected<void, std::string>(const std::vector<uint8_t>&)>& callback) -> void;
		auto received_binary_callback(
			const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>&
				callback) -> void;
		auto received_message_callback(
			const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>& callback) -> void;
		auto received_file_callback(
			const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>&
				callback) -> void;
		auto received_files_callback(
			const std::function<std::expected<void, std::string>(
				const std::string&, const std::string&, const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>& callback) -> void;

	protected:
		auto disconnected(bool by_itself) -> void override;
		auto received_data(DataModes mode, const std::vector<uint8_t>& data) -> std::expected<void, std::string> override;

	private:
		auto received_connection(const std::vector<uint8_t>& data) -> std::expected<void, std::string>;
		auto received_binary(const std::vector<uint8_t>& data) -> std::expected<void, std::string>;
		auto received_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>;
		auto received_file(const std::vector<uint8_t>& data) -> std::expected<void, std::string>;
		auto received_files(const std::vector<std::string>& failures, const std::vector<std::pair<std::string, std::string>>& successes)
			-> std::expected<void, std::string>;

		auto response_connection(bool condition) -> std::expected<void, std::string>;
		auto start_heartbeat(void) -> void;
		auto stop_heartbeat(void) -> void;

	private:
		SessionId session_id_;
		SessionState state_;
		std::string server_id_;
		std::string registered_key_;
		std::map<DataModes, const std::function<std::expected<void, std::string>(const std::vector<uint8_t>&)>> message_handlers_;

		std::unique_ptr<FileManager> file_manager_;

		// Handshake timeout timer; created on start and cancelled on authentication/stop
		std::shared_ptr<boost::asio::steady_timer> handshake_timer_;
		std::atomic<bool> stopped_;

		// Optional heartbeat
		bool heartbeat_enabled_;
		uint32_t heartbeat_interval_sec_;
		std::shared_ptr<boost::asio::steady_timer> heartbeat_timer_;

		// Heartbeat monitoring
		// steady_clock time of last pong, stored as nanoseconds since the clock epoch
		// so it can be read/written atomically across the io_context and worker threads
		std::atomic<int64_t> last_pong_at_{ 0 };
		uint32_t missed_heartbeats_ = 0;
		uint32_t max_missed_heartbeats_ = 3; // expire after N missed intervals

		std::function<std::expected<void, std::string>(const std::vector<uint8_t>&)> received_connection_callback_;
		std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)> received_message_callback_;
		std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>
			received_file_callback_;
		std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>
			received_binary_callback_;
		std::function<std::expected<void, std::string>(
			const std::string&, const std::string&, const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>
			received_files_callback_;
	};
}
