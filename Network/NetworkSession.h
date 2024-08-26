#pragma once

#include "FileManager.h"
#include "DataHandler.h"

#include <map>
#include <memory>
#include <string>

namespace Network
{
	class NetworkSession : public std::enable_shared_from_this<NetworkSession>, public DataHandler
	{
	public:
#ifdef USE_ENCRYPT_MODULE
		NetworkSession(
			const std::string& id, const bool& encrypt, const uint16_t& high_priority_count, const uint16_t& normal_priority_count, const uint16_t& low_priority_count);
#else
		NetworkSession(const std::string& id, const uint16_t& high_priority_count, const uint16_t& normal_priority_count, const uint16_t& low_priority_count);
#endif
		virtual ~NetworkSession(void);

		auto get_ptr(void) -> std::shared_ptr<NetworkSession>;

		auto start(std::shared_ptr<boost::asio::ip::tcp::socket> socket, const size_t& socket_buffer_size) -> void;
		auto stop(void) -> void;

		auto register_key(const std::string& key) -> void;

		auto received_connection_callback(const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<uint8_t>&)>& callback) -> void;
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

	protected:
		auto disconnected(const bool& by_itself) -> void override;
		auto received_data(const DataModes& mode, const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>> override;

	private:
		auto received_connection(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto received_binary(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto received_message(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto received_file(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto received_files(const std::vector<std::string>& failures, const std::vector<std::pair<std::string, std::string>>& successes)
			-> std::tuple<bool, std::optional<std::string>>;

		auto response_connection(const bool& condition) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::string server_id_;
		std::string registered_key_;
		std::map<DataModes, const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<uint8_t>&)>> message_handlers_;

		std::unique_ptr<FileManager> file_manager_;

		std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<uint8_t>&)> received_connection_callback_;
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