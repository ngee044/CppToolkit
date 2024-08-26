#pragma once

#include "ThreadPool.h"

#include "boost/asio.hpp"

#include <mutex>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include <optional>

using namespace Thread;

namespace Network
{
	class NetworkSession;
	class NetworkServer : public std::enable_shared_from_this<NetworkServer>
	{
	public:
		NetworkServer(const std::string& id, const uint16_t& high_priority_count = 3, const uint16_t& normal_priority_count = 3, const uint16_t& low_priority_count = 3);
		virtual ~NetworkServer(void);

		auto get_ptr(void) -> std::shared_ptr<NetworkServer>;

		auto id(const std::string& new_id) -> void;

		auto id(void) const -> std::string;

#ifdef USE_ENCRYPT_MODULE
		auto encrypt_mode(const bool& mode) -> void;
		auto encrypt_mode(void) -> const bool;
#endif

		auto register_key(const std::string& key) -> void;

		auto start(const uint16_t& port, const size_t& socket_buffer_size) -> std::tuple<bool, std::optional<std::string>>;
		auto send_binary(const std::vector<uint8_t>& binary, const std::string& message, const std::string& id = "", const std::string& sub_id = "")
			-> std::tuple<bool, std::optional<std::string>>;
		auto send_message(const std::string& message, const std::string& id = "", const std::string& sub_id = "") -> std::tuple<bool, std::optional<std::string>>;
		auto send_files(const std::vector<std::pair<std::string, std::string>>& file_informations, const std::string& id = "", const std::string& sub_id = "")
			-> std::tuple<bool, std::optional<std::string>>;
		auto wait_stop(const uint32_t& seconds = 0) -> std::tuple<bool, std::optional<std::string>>;
		auto stop(void) -> std::tuple<bool, std::optional<std::string>>;

		auto received_connection_callback(
			const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const bool&)>& callback) -> void;
		auto received_binary_callback(
			const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>&
				callback) -> void;
		auto received_message_callback(
			const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>& callback) -> void;
		auto received_file_callback(
			const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>&
				callback) -> void;
		auto received_files_callback(
			const std::function<std::tuple<bool, std::optional<std::string>>(
				const std::string&, const std::string&, const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>& callback) -> void;

		auto drop_session(const std::string& id, const std::string& sub_id) -> void;
		auto drop_sessions(const std::string& id) -> void;

	protected:
		auto received_connection(const std::vector<uint8_t>& condition) -> std::tuple<bool, std::optional<std::string>>;
		auto received_binary(const std::string& id, const std::string& sub_id, const std::string& message, const std::vector<uint8_t>& data)
			-> std::tuple<bool, std::optional<std::string>>;
		auto received_message(const std::string& id, const std::string& sub_id, const std::string& message) -> std::tuple<bool, std::optional<std::string>>;
		auto received_file(const std::string& id, const std::string& sub_id, const std::string& message, const std::vector<uint8_t>& file_path)
			-> std::tuple<bool, std::optional<std::string>>;
		auto received_files(const std::string& id,
							const std::string& sub_id,
							const std::vector<std::string>& failures,
							const std::vector<std::pair<std::string, std::string>>& successes) -> std::tuple<bool, std::optional<std::string>>;

	private:
		auto create_io_context(const uint16_t& port) -> bool;
		auto destroy_io_context(void) -> void;

		auto create_thread_pool(void) -> void;
		auto destroy_thread_pool(void) -> void;

		auto drop_sessions(void) -> void;

		auto start_main_job(void) -> void;

		auto wait_connection(void) -> void;
		auto received_connection_handler(const std::vector<uint8_t>& condition) -> std::tuple<bool, std::optional<std::string>>;
		auto run(void) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::string id_;
		std::string registered_key_;

		size_t buffer_size_;

		uint16_t high_priority_count_;
		uint16_t normal_priority_count_;
		uint16_t low_priority_count_;

#ifdef USE_ENCRYPT_MODULE
		bool encrypt_mode_;
#endif

		std::mutex mutex_;
		std::vector<std::shared_ptr<NetworkSession>> sessions_;

		std::future<bool> future_status_;
		std::unique_ptr<std::promise<bool>> promise_status_;

		std::shared_ptr<ThreadPool> thread_pool_;
		std::shared_ptr<boost::asio::io_context> io_context_;
		std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;

		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const bool&)> received_connection_callback_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)> received_message_callback_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>
			received_file_callback_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>
			received_binary_callback_;
		std::function<std::tuple<bool, std::optional<std::string>>(
			const std::string&, const std::string&, const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>
			received_files_callback_;
	};
}