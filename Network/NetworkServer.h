#pragma once

#include "ThreadPool.h"

#include "boost/asio.hpp"
#include <boost/asio/steady_timer.hpp>

#include <mutex>
#include <atomic>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include <optional>
#include <expected>

namespace Network
{
	class NetworkSession;
	class NetworkServer : public std::enable_shared_from_this<NetworkServer>
	{
	public:
		NetworkServer(const std::string& id, uint16_t high_priority_count = 3, uint16_t normal_priority_count = 3, uint16_t low_priority_count = 3);
		virtual ~NetworkServer(void);

		auto get_ptr(void) -> std::shared_ptr<NetworkServer>;

		auto id(const std::string& new_id) -> void;

		auto id(void) const -> std::string;

#ifdef USE_ENCRYPT_MODULE
		auto encrypt_mode(bool mode) -> void;
		auto encrypt_mode(void) -> const bool;
#endif

		auto register_key(const std::string& key) -> void;

		// Heartbeat control for all sessions created by this server
		auto heartbeat_mode(bool enable, uint32_t interval_sec = 30) -> void;
		// Set how many heartbeat intervals are tolerated before expiring a session
		auto heartbeat_tolerance(uint32_t missed_count) -> void;

		// Maintenance (session cleaner) interval in seconds
		auto maintenance_interval(uint32_t interval_sec) -> void;

		auto start(uint16_t port, size_t socket_buffer_size) -> std::expected<void, std::string>;
		auto send_binary(const std::vector<uint8_t>& binary, const std::string& message, const std::string& id = "", const std::string& sub_id = "")
			-> std::expected<void, std::string>;
		auto send_message(const std::string& message, const std::string& id = "", const std::string& sub_id = "") -> std::expected<void, std::string>;
		auto send_files(const std::vector<std::pair<std::string, std::string>>& file_informations, const std::string& id = "", const std::string& sub_id = "")
			-> std::expected<void, std::string>;
		auto wait_stop(uint32_t seconds = 0) -> std::expected<void, std::string>;
		auto stop(void) -> std::expected<void, std::string>;

		auto received_connection_callback(
			const std::function<std::expected<void, std::string>(const std::string&, const std::string&, bool)>& callback) -> void;
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

		auto drop_session(const std::string& id, const std::string& sub_id) -> void;
		auto drop_sessions(const std::string& id) -> void;

	protected:
		auto received_connection(const std::vector<uint8_t>& condition) -> std::expected<void, std::string>;
		auto received_binary(const std::string& id, const std::string& sub_id, const std::string& message, const std::vector<uint8_t>& data)
			-> std::expected<void, std::string>;
		auto received_message(const std::string& id, const std::string& sub_id, const std::string& message) -> std::expected<void, std::string>;
		auto received_file(const std::string& id, const std::string& sub_id, const std::string& message, const std::vector<uint8_t>& file_path)
			-> std::expected<void, std::string>;
		auto received_files(const std::string& id,
							const std::string& sub_id,
							const std::vector<std::string>& failures,
							const std::vector<std::pair<std::string, std::string>>& successes) -> std::expected<void, std::string>;

	private:
		auto create_io_context(uint16_t port) -> bool;
		auto destroy_io_context(void) -> void;

		auto create_thread_pool(void) -> void;
		auto destroy_thread_pool(void) -> void;

		auto drop_sessions(void) -> void;

		auto start_main_job(void) -> void;

	auto wait_connection(void) -> void;
		auto received_connection_handler(const std::vector<uint8_t>& condition) -> std::expected<void, std::string>;
		auto run(void) -> std::expected<void, std::string>;

		// Periodic maintenance: cleanup expired/closed sessions
		auto start_maintenance_job(void) -> void;
		auto stop_maintenance_job(void) -> void;

	private:
		std::string id_;
		std::string registered_key_;

		size_t buffer_size_;

		uint16_t high_priority_count_;
		uint16_t normal_priority_count_;
		uint16_t low_priority_count_;

		// Heartbeat settings
		bool heartbeat_enabled_;
		uint32_t heartbeat_interval_sec_;

#ifdef USE_ENCRYPT_MODULE
		bool encrypt_mode_;
#endif

		std::mutex mutex_;
		std::vector<std::shared_ptr<NetworkSession>> sessions_;

		std::future<bool> future_status_;
		std::unique_ptr<std::promise<bool>> promise_status_;

		std::shared_ptr<Thread::ThreadPool> thread_pool_;
		std::shared_ptr<boost::asio::io_context> io_context_;
		std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
		std::shared_ptr<boost::asio::steady_timer> maintenance_timer_;

		std::atomic<bool> tearing_down_;

		// Session cleaner interval (seconds)
		uint32_t maintenance_interval_sec_;

		// Heartbeat tolerance (missed intervals allowed)
		uint32_t heartbeat_missed_tolerance_;

		std::function<std::expected<void, std::string>(const std::string&, const std::string&, bool)> received_connection_callback_;
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
