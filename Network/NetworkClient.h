#pragma once

#include "FileManager.h"
#include "DataHandler.h"

#include <map>
#include <mutex>
#include <future>
#include <memory>
#include <thread>
#include <optional>

namespace Network
{
	class NetworkClient : public std::enable_shared_from_this<NetworkClient>, public DataHandler
	{
	public:
		NetworkClient(const std::string& client_id,
					  const uint16_t& high_priority_count = 3,
					  const uint16_t& normal_priority_count = 3,
					  const uint16_t& low_priority_count = 3);
		virtual ~NetworkClient(void);

		auto get_ptr(void) -> std::shared_ptr<NetworkClient>;

		auto start(const std::string& ip, const uint16_t& port, const size_t& socket_buffer_size) -> bool;
		auto wait_stop(const uint32_t& seconds = 0) -> void;
		auto stop(void) -> void;

		auto register_key(const std::string& key) -> void;

		auto received_connection_callback(const std::function<std::tuple<bool, std::optional<std::string>>(const bool&, const bool&)>& callback) -> void;
		auto received_binary_callback(const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::vector<uint8_t>&)>& callback)
			-> void;
		auto received_message_callback(const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&)>& callback) -> void;
		auto received_file_callback(const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::vector<uint8_t>&)>& callback) -> void;
		auto received_files_callback(const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<std::string>&,
																									  const std::vector<std::pair<std::string, std::string>>&)>& callback)
			-> void;

	protected:
		auto disconnected(const bool& by_itself) -> void override;
		auto received_data(const DataModes& mode, const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>> override;
		auto request_connection(void) -> void;

	private:
		auto create_io_context(void) -> void;
		auto destroy_io_context(void) -> void;

		auto create_socket(const std::string& ip, const uint16_t& port) -> bool;
		auto run(void) -> std::tuple<bool, std::optional<std::string>>;

		auto received_connection(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto received_binary(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto received_message(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto received_file(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto received_files(const std::vector<std::string>& failures, const std::vector<std::pair<std::string, std::string>>& successes)
			-> std::tuple<bool, std::optional<std::string>>;

	private:
		std::string server_id_;
		std::string registered_key_;

		std::unique_ptr<FileManager> file_manager_;

		std::shared_ptr<boost::asio::io_context> io_context_;
		std::map<DataModes, const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<uint8_t>&)>> message_handlers_;

		std::future<bool> future_status_;
		std::unique_ptr<std::promise<bool>> promise_status_;

		std::function<std::tuple<bool, std::optional<std::string>>(const bool&, const bool&)> received_connection_callback_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&)> received_message_callback_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::vector<uint8_t>&)> received_file_callback_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::vector<uint8_t>&)> received_binary_callback_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>
			received_files_callback_;
	};
}