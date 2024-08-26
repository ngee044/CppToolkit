#include "NetworkSession.h"

#include "Job.h"
#include "File.h"
#include "Logger.h"
#include "Combiner.h"
#include "Converter.h"
#include "Generator.h"
#include "Encryptor.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

#include "boost/json.hpp"

#include <locale>
#include <filesystem>

using namespace Thread;
using namespace Utilities;

namespace Network
{
#ifdef USE_ENCRYPT_MODULE
	NetworkSession::NetworkSession(const std::string& session_id,
								   const bool& encrypt,
								   const uint16_t& high_priority_count,
								   const uint16_t& normal_priority_count,
								   const uint16_t& low_priority_count)
#else
	NetworkSession::NetworkSession(const std::string& session_id,
								   const uint16_t& high_priority_count,
								   const uint16_t& normal_priority_count,
								   const uint16_t& low_priority_count)
#endif
		: DataHandler(high_priority_count, normal_priority_count, low_priority_count)
		, server_id_(session_id)
		, registered_key_("")
		, file_manager_(std::make_unique<FileManager>())
	{
		id("unauthorized_client");
		sub_id(Generator::guid());

#ifdef USE_ENCRYPT_MODULE
		encrypt_mode(encrypt);
#endif

		message_handlers_.insert({ DataModes::Connection, std::bind(&NetworkSession::received_connection, this, std::placeholders::_1) });
		message_handlers_.insert({ DataModes::Binary, std::bind(&NetworkSession::received_binary, this, std::placeholders::_1) });
		message_handlers_.insert({ DataModes::Message, std::bind(&NetworkSession::received_message, this, std::placeholders::_1) });
		message_handlers_.insert({ DataModes::File, std::bind(&NetworkSession::received_file, this, std::placeholders::_1) });
		file_manager_->received_files_callback(std::bind(&NetworkSession::received_files, this, std::placeholders::_1, std::placeholders::_2));
	}

	NetworkSession::~NetworkSession(void)
	{
		stop();

		Logger::handle().write(LogTypes::Sequence, fmt::format("destroyed NetworkSession on {}", id()));
	}

	auto NetworkSession::get_ptr(void) -> std::shared_ptr<NetworkSession> { return shared_from_this(); }

	auto NetworkSession::start(std::shared_ptr<boost::asio::ip::tcp::socket> connected_socket, const size_t& socket_buffer_size) -> void
	{
		condition(ConnectConditions::None);

		if (connected_socket == nullptr)
		{
			return;
		}

		buffer_size(socket_buffer_size);

		connected_socket->set_option(boost::asio::ip::tcp::no_delay(true));
		connected_socket->set_option(boost::asio::socket_base::keep_alive(true));
		connected_socket->set_option(boost::asio::socket_base::receive_buffer_size(buffer_size()));
		connected_socket->set_option(boost::asio::socket_base::send_buffer_size(buffer_size()));

		socket(connected_socket);
		condition(ConnectConditions::Waiting);

		create_thread_pool(fmt::format("ThreadPool on NetworkSession on {}", id()));

		file_manager_->thread_pool(thread_pool());

		read_start_code();
	}

	auto NetworkSession::stop(void) -> void
	{
		condition(ConnectConditions::Expired, true);

		file_manager_->thread_pool(nullptr);

		destroy_thread_pool();
		destroy_socket();
	}

	auto NetworkSession::register_key(const std::string& key) -> void { registered_key_ = key; }

	auto NetworkSession::received_connection_callback(const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<uint8_t>&)>& callback) -> void
	{
		received_connection_callback_ = callback;
	}

	auto NetworkSession::received_binary_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>&
			callback) -> void
	{
		received_binary_callback_ = callback;
	}

	auto NetworkSession::received_message_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>& callback) -> void
	{
		received_message_callback_ = callback;
	}

	auto NetworkSession::received_file_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>&
			callback) -> void
	{
		received_file_callback_ = callback;
	}

	auto NetworkSession::received_files_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(
			const std::string&, const std::string&, const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>& callback) -> void
	{
		received_files_callback_ = callback;
	}

	auto NetworkSession::disconnected(const bool& by_itself) -> void { response_connection(false); }

	auto NetworkSession::received_connection(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
	{
		if (condition() != ConnectConditions::Waiting)
		{
			return { false, "the line is not waiting for connection" };
		}

		if (data.empty())
		{
			return { false, "received empty data for connection" };
		}

		boost::json::object received_message = boost::json::parse(Converter::to_string(data)).as_object();

		id(received_message.at("id").as_string().data());

		Logger::handle().write(LogTypes::Debug, fmt::format("received connection message from NetworkClient: ({})", id()));

		if (!received_message.at("registered_key").is_string() || received_message.at("registered_key").as_string().data() != registered_key_)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("the registered key of the NetworkClient is not compatible with the server: ({})", id()));

			condition(ConnectConditions::Expired);

			return response_connection(false);
		}

		condition(ConnectConditions::Confirmed);

		return response_connection(true);
	}

	auto NetworkSession::received_binary(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
	{
		if (condition() != ConnectConditions::Confirmed)
		{
			condition(ConnectConditions::Expired);

			return { false, "cannot handle binary message until receiving confirm message related to connection." };
		}

		if (data.empty())
		{
			return { false, "cannot handle empty message." };
		}

		size_t index = 0;
		auto message = Converter::to_string(Combiner::divide(data, index));
		auto binary = Combiner::divide(data, index);
		if (binary.empty())
		{
			return { false, "cannot handle empty binary message." };
		}

		if (received_binary_callback_ == nullptr)
		{
			return { false, "there is no callback to handle binary data" };
		}

		return received_binary_callback_(id(), sub_id(), message, binary);
	}

	auto NetworkSession::received_data(const DataModes& mode, const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
	{
		auto iter = message_handlers_.find(mode);
		if (iter == message_handlers_.end())
		{
			return { false, "there is no matched mode" };
		}

		return iter->second(data);
	}

	auto NetworkSession::received_message(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
	{
		if (condition() != ConnectConditions::Confirmed)
		{
			condition(ConnectConditions::Expired);

			return { false, "cannot handle normal message until receiving confirm message related to connection." };
		}

		if (data.empty())
		{
			return { false, "cannot handle empty message." };
		}

		if (received_message_callback_ == nullptr)
		{
			return { false, "there is no callback to handle message data" };
		}

		return received_message_callback_(id(), sub_id(), Converter::to_string(data));
	}

	auto NetworkSession::received_file(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
	{
		if (condition() != ConnectConditions::Confirmed)
		{
			condition(ConnectConditions::Expired);

			return { false, "cannot handle file message until receiving confirm message related to connection." };
		}

		if (data.empty())
		{
			return { false, "cannot handle empty file message." };
		}

		size_t index = 0;
		auto guid = Converter::to_string(Combiner::divide(data, index));
		auto file_mode = Combiner::divide(data, index);
		auto file_index = Combiner::divide(data, index);

		size_t file_count = 0;
		std::copy(file_index.begin(), file_index.end(), reinterpret_cast<uint8_t*>(&file_count));

		if ((FileModes)file_mode[0] == FileModes::Start)
		{
			Logger::handle().write(LogTypes::Debug, fmt::format("start receiving files [{}]: {} files", guid, file_count));

			return file_manager_->start(guid, file_count);
		}

		auto message = Converter::to_string(Combiner::divide(data, index));

		if ((FileModes)file_mode[0] == FileModes::Failure)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("cannot complete file receiving [{}]: index[{}] => {}", guid, file_count, message));

			return file_manager_->failure(guid, message);
		}

		auto file_data = Combiner::divide(data, index);
		auto temp_file_path = save_temp_path(file_data);
		if (temp_file_path == std::nullopt)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("cannot complete file receiving [{}]: index[{}] => {}", guid, file_count, message));

			return file_manager_->failure(guid, message);
		}

		Logger::handle().write(LogTypes::Debug, fmt::format("completed file receiving [{}]: index[{}] => {}", guid, file_count, message));

		if (received_file_callback_)
		{
			received_file_callback_(id(), sub_id(), message, Converter::to_array(temp_file_path.value()));
		}

		return file_manager_->success(guid, message, temp_file_path.value());
	}

	auto NetworkSession::received_files(const std::vector<std::string>& failures, const std::vector<std::pair<std::string, std::string>>& successes)
		-> std::tuple<bool, std::optional<std::string>>
	{
		if (received_files_callback_ == nullptr)
		{
			return { false, "there is no callback to handle received files" };
		}

		return received_files_callback_(id(), sub_id(), failures, successes);
	}

	auto NetworkSession::response_connection(const bool& condition) -> std::tuple<bool, std::optional<std::string>>
	{
#ifdef USE_ENCRYPT_MODULE
		if (encrypt_mode())
		{
			auto value = Encryptor::create_key();

			key(value.first);
			iv(value.second);
		}
#endif

		boost::json::object message{ { "id", id() },
									 { "sub_id", sub_id() },
#ifdef USE_ENCRYPT_MODULE
									 { "key", key() },
									 { "iv", iv() },
									 { "encrypt_mode", encrypt_mode() },
#endif
									 { "condition", condition } };

		auto array_data = Converter::to_array(boost::json::serialize(message));

		auto pool = thread_pool();
		if (pool)
		{
			pool->push(std::make_shared<Job>(JobPriorities::Normal, array_data, received_connection_callback_, "received_connection_job"));

			return send(DataModes::Connection, array_data);
		}

		if (received_connection_callback_ == nullptr)
		{
			return { false, "there is no callback to handle connection" };
		}

		return received_connection_callback_(array_data);
	}
}