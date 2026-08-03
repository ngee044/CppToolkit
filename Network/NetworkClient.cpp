#include "NetworkClient.h"

#include "Job.h"
#include "File.h"
#include "Logger.h"
#include "Combiner.h"
#include "Converter.h"
#include "ConnectionJob.h"
#include "ThreadWorker.h"

#include <format>

#include "boost/json.hpp"
#include "boost/json/parse.hpp"

#include <locale>
#include <string>
#include <expected>

using namespace Thread;
using namespace Utilities;

namespace Network
{
	NetworkClient::NetworkClient(const std::string& client_id,
								 uint16_t high_priority_count,
								 uint16_t normal_priority_count,
								 uint16_t low_priority_count)
		: DataHandler(high_priority_count, normal_priority_count, low_priority_count)
		, io_context_(nullptr)
		, promise_status_(nullptr)
		, file_manager_(std::make_unique<FileManager>())
	{
		id(client_id);
		message_handlers_.insert({ DataModes::Connection, std::bind(&NetworkClient::received_connection, this, std::placeholders::_1) });
		message_handlers_.insert({ DataModes::Binary, std::bind(&NetworkClient::received_binary, this, std::placeholders::_1) });
		message_handlers_.insert({ DataModes::Message, std::bind(&NetworkClient::received_message, this, std::placeholders::_1) });
		message_handlers_.insert({ DataModes::File, std::bind(&NetworkClient::received_file, this, std::placeholders::_1) });
		file_manager_->received_files_callback(std::bind(&NetworkClient::received_files, this, std::placeholders::_1, std::placeholders::_2));
	}

	NetworkClient::~NetworkClient(void)
	{
		destroy_socket();
		destroy_io_context();

		Logger::handle().write(LogTypes::Sequence, std::format("destroyed NetworkClient on {}", id()));
	}

	auto NetworkClient::get_ptr(void) -> std::shared_ptr<NetworkClient> { return shared_from_this(); }

	auto NetworkClient::start(const std::string& ip, uint16_t port, size_t socket_buffer_size) -> bool
	{
		destroy_socket();
		destroy_io_context();

		condition(ConnectConditions::None);

		buffer_size(socket_buffer_size);
		create_io_context();

		if (!create_socket(ip, port))
		{
			destroy_io_context();
			return false;
		}

		condition(ConnectConditions::Waiting);

		set_self_guard(shared_from_this());
		read_start_code();

		request_connection();

		auto pool = thread_pool();
		if (pool)
		{
			pool->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::LongTerm }));
			pool->push(std::make_shared<Job>(JobPriorities::LongTerm, std::bind(&NetworkClient::run, this), "main_job_for_client", false));
		}
		file_manager_->thread_pool(pool);

		return true;
	}

	auto NetworkClient::wait_stop(uint32_t seconds) -> void
	{
		if (io_context_ == nullptr)
		{
			return;
		}

		promise_status_ = std::make_unique<std::promise<bool>>();
		future_status_ = promise_status_->get_future();

		Logger::handle().write(LogTypes::Sequence, "attempt to call wait_stop on NetworkClient");

		if (seconds == 0)
		{
			future_status_.wait();
		}
		else
		{
			future_status_.wait_for(std::chrono::seconds(seconds));
		}

		file_manager_->thread_pool(nullptr);

		destroy_socket();
		destroy_io_context();
	}

	auto NetworkClient::stop(void) -> void
	{
		if (io_context_ == nullptr)
		{
			return;
		}

		Logger::handle().write(LogTypes::Sequence, "attempt to call stop on NetworkClient");

		condition(ConnectConditions::Expired, true);

		if (promise_status_ != nullptr)
		{
			try
			{
				promise_status_->set_value(true);
			}
			catch (const std::future_error&)
			{
			}
			promise_status_.reset();

			return;
		}

		file_manager_->thread_pool(nullptr);

		destroy_socket();
		destroy_io_context();
	}

	auto NetworkClient::register_key(const std::string& key) -> void { registered_key_ = key; }

	auto NetworkClient::received_connection_callback(const std::function<std::expected<void, std::string>(bool, bool)>& callback) -> void
	{
		received_connection_callback_ = callback;
	}

	auto NetworkClient::received_binary_callback(
		const std::function<std::expected<void, std::string>(const std::string&, const std::vector<uint8_t>&)>& callback) -> void
	{
		received_binary_callback_ = callback;
	}

	auto NetworkClient::received_message_callback(const std::function<std::expected<void, std::string>(const std::string&)>& callback) -> void
	{
		received_message_callback_ = callback;
	}

	auto NetworkClient::received_file_callback(
		const std::function<std::expected<void, std::string>(const std::string&, const std::vector<uint8_t>&)>& callback) -> void
	{
		received_file_callback_ = callback;
	}

	auto NetworkClient::received_files_callback(
		const std::function<std::expected<void, std::string>(const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>&
			callback) -> void
	{
		received_files_callback_ = callback;
	}

	auto NetworkClient::disconnected(bool by_itself) -> void
	{
		key("");
		iv("");

		if (received_connection_callback_)
		{
			received_connection_callback_(false, by_itself);
		}
	}

	auto NetworkClient::received_data(DataModes mode, const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() == ConnectConditions::Expired)
		{
			return std::unexpected("the line has expired");
		}

		auto iter = message_handlers_.find(mode);
		if (iter == message_handlers_.end())
		{
			return std::unexpected("there is no matched mode");
		}

		return iter->second(data);
	}

	auto NetworkClient::request_connection(void) -> void
	{
		boost::json::object message{ { "id", id() }, { "sub_id", sub_id() }, { "registered_key", registered_key_ }, { "condition", true } };

		send(DataModes::Connection, Converter::to_array(boost::json::serialize(message)));
	}

	auto NetworkClient::create_io_context(void) -> void
	{
		destroy_io_context();

		io_context_ = std::make_shared<boost::asio::io_context>();

		create_thread_pool(std::format("ThreadPool on NetworkClient on {}", id()));
	}

	auto NetworkClient::destroy_io_context(void) -> void
	{
		Logger::handle().write(LogTypes::Sequence, "attempt to call destroy_io_context on NetworkClient");

		auto pool = thread_pool();
		if (pool)
		{
			pool->lock(true);
			pool.reset();
		}

		if (io_context_ != nullptr)
		{
			io_context_->stop();
		}

		// Asio requires every run() to return before the io_context is destroyed, so release it only after the join
		destroy_thread_pool();

		io_context_.reset();
	}

	auto NetworkClient::create_socket(const std::string& ip, uint16_t port) -> bool
	{
		destroy_socket();

		auto current_socket = std::make_shared<boost::asio::ip::tcp::socket>(*io_context_);

    try
    {
			current_socket->open(boost::asio::ip::tcp::v4());
			current_socket->bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
#if BOOST_VERSION >= 106600
			current_socket->connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(ip), port));
#else
			current_socket->connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip), port));
#endif	
			current_socket->set_option(boost::asio::ip::tcp::no_delay(true));
			current_socket->set_option(boost::asio::socket_base::keep_alive(true));
			current_socket->set_option(boost::asio::socket_base::receive_buffer_size(buffer_size()));
			current_socket->set_option(boost::asio::socket_base::send_buffer_size(buffer_size()));
		}
    catch (const std::overflow_error& message)
    {
        destroy_socket();
        Logger::handle().write(LogTypes::Exception,
                               std::format("cannot create socket on NetworkClient on {} => {}:{} {}",
                                           id(), ip, port, message.what()));

        return false;
    }
    catch (const std::runtime_error& message)
    {
        destroy_socket();
        Logger::handle().write(LogTypes::Exception,
                               std::format("cannot create socket on NetworkClient on {} => {}:{} {}",
                                           id(), ip, port, message.what()));

        return false;
    }
    catch (const std::exception& message)
    {
        destroy_socket();
        Logger::handle().write(LogTypes::Exception,
                               std::format("cannot create socket on NetworkClient on {} => {}:{} {}",
                                           id(), ip, port, message.what()));

        return false;
    }
    catch (...)
    {
        destroy_socket();
        Logger::handle().write(LogTypes::Exception,
                               std::format("cannot create socket on NetworkClient on {} => {}:{} unexpected error",
                                           id(), ip, port));

        return false;
    }

		socket(current_socket);

		return true;
	}

	auto NetworkClient::run(void) -> std::expected<void, std::string>
	{
		Logger::handle().write(LogTypes::Information, std::format("started io_context on NetworkClient on {}", id()));

		try
		{
			io_context_->run();
		}
		catch (const std::overflow_error& message)
		{
			return std::unexpected(std::format("restart io_context on NetworkClient for {} => {}", id(), message.what()));
		}
		catch (const std::runtime_error& message)
		{
			return std::unexpected(std::format("restart io_context on NetworkClient for {} => {}", id(), message.what()));
		}
		catch (const std::exception& message)
		{
			return std::unexpected(std::format("restart io_context on NetworkClient for {} => {}", id(), message.what()));
		}
		catch (...)
		{
			return std::unexpected(std::format("restart io_context on NetworkClient for {} => unexpected error", id()));
		}

		Logger::handle().write(LogTypes::Debug, std::format("stopped io_context on NetworkClient for {}", id()));

		return {};
	}

	auto NetworkClient::received_connection(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() != ConnectConditions::Waiting)
		{
			return std::unexpected("the line is not waiting for connection");
		}

		if (data.empty())
		{
			return std::unexpected("received empty data for connection");
		}

    boost::json::object received_message;
    try
    {
        received_message = boost::json::parse(Converter::to_string(data)).as_object();
    }
    catch (const std::exception& e)
    {
        Logger::handle().write(LogTypes::Error, std::format("invalid connection json on client: {}", e.what()));
        return std::unexpected("invalid connection json");
    }

    if (!received_message.if_contains("id") || !received_message.at("id").is_string())
    {
        return std::unexpected("invalid connection response: missing id");
    }
    if (!received_message.if_contains("sub_id") || !received_message.at("sub_id").is_string())
    {
        return std::unexpected("invalid connection response: missing sub_id");
    }

    server_id_ = received_message.at("id").as_string();
    sub_id(received_message.at("sub_id").as_string().c_str());

#ifdef USE_ENCRYPT_MODULE
		if (received_message.at("key").is_string())
		{
			key(received_message.at("key").as_string().data());
		}
		if (received_message.at("iv").is_string())
		{
			iv(received_message.at("iv").as_string().data());
		}
		if (received_message.at("encrypt_mode").is_bool())
		{
			encrypt_mode(received_message.at("encrypt_mode").as_bool());
		}
#endif

    Logger::handle().write(LogTypes::Debug, std::format("received connection message: ({})", server_id_));

    if (!received_message.if_contains("condition") || !received_message.at("condition").is_bool() || !received_message.at("condition").as_bool())
    {
        condition(ConnectConditions::Expired);

			key("");
			iv("");

			if (received_connection_callback_)
			{
				auto pool = thread_pool();
				if (pool)
				{
					pool->push(std::dynamic_pointer_cast<Job>(std::make_shared<ConnectionJob>(false, true, received_connection_callback_)));
				}
			}

        return std::unexpected("connection has expired by server");
    }

    condition(ConnectConditions::Confirmed);

		if (received_connection_callback_)
		{
			auto pool = thread_pool();
			if (pool)
			{
				pool->push(std::dynamic_pointer_cast<Job>(std::make_shared<ConnectionJob>(true, true, received_connection_callback_)));
			}
		}

		return {};
	}

	auto NetworkClient::received_binary(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() != ConnectConditions::Confirmed)
		{
			condition(ConnectConditions::Expired);

			return std::unexpected("cannot handle binary message until receiving confirm message related to connection.");
		}

		if (data.empty())
		{
			return std::unexpected("cannot handle empty message.");
		}

		size_t index = 0;
		auto message = Converter::to_string(Combiner::divide(data, index));
		auto binary = Combiner::divide(data, index);
		if (binary.empty())
		{
			return std::unexpected("cannot handle empty binary message.");
		}

		if (received_binary_callback_ == nullptr)
		{
			return std::unexpected("there is no callback to handle binary data");
		}

		return received_binary_callback_(message, binary);
	}

	auto NetworkClient::received_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() != ConnectConditions::Confirmed)
		{
			condition(ConnectConditions::Expired);

			return std::unexpected("cannot handle normal message until receiving confirm message related to connection.");
		}

		if (data.empty())
		{
			return std::unexpected("cannot handle empty message.");
		}

    auto msg = Converter::to_string(data);
    // Heartbeat handling: reply pong (optional) and swallow regardless of user callback presence
    if (msg.rfind("heartbeat:ping", 0) == 0)
    {
        Logger::handle().write(LogTypes::Debug, std::format("received heartbeat:ping from server [{}:{}]", id(), sub_id()));
        if (auto_pong_enabled_)
        {
            send_message("heartbeat:pong");
        }
        return {};
    }
    if (msg.rfind("heartbeat:pong", 0) == 0)
    {
        Logger::handle().write(LogTypes::Debug, std::format("received heartbeat:pong from server [{}:{}]", id(), sub_id()));
        return {};
    }

    if (received_message_callback_ == nullptr)
    {
        return std::unexpected("there is no callback to handle message data");
    }

    return received_message_callback_(msg);
	}

	auto NetworkClient::received_file(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() != ConnectConditions::Confirmed)
		{
			condition(ConnectConditions::Expired);

			return std::unexpected("cannot handle file message until receiving confirm message related to connection.");
		}

		if (data.empty())
		{
			return std::unexpected("cannot handle empty file message.");
		}

		size_t index = 0;
		auto guid = Converter::to_string(Combiner::divide(data, index));
		auto file_mode = Combiner::divide(data, index);
		auto file_index = Combiner::divide(data, index);

    size_t file_count = 0;
    if (file_index.size() == sizeof(size_t))
    {
        memcpy(&file_count, file_index.data(), sizeof(size_t));
    }

		if ((FileModes)file_mode[0] == FileModes::Start)
		{
			Logger::handle().write(LogTypes::Debug, std::format("start receiving files [{}]: {} files", guid, file_count));

			return file_manager_->start(guid, file_count);
		}

		auto message = Converter::to_string(Combiner::divide(data, index));

		if ((FileModes)file_mode[0] == FileModes::Failure)
		{
			Logger::handle().write(LogTypes::Error, std::format("cannot complete file receiving [{}]: index[{}] => {}", guid, file_count, message));

			return file_manager_->failure(guid, message);
		}

		auto file_data = Combiner::divide(data, index);
		auto temp_file_path = save_temp_path(file_data);
		if (temp_file_path == std::nullopt)
		{
			Logger::handle().write(LogTypes::Error, std::format("cannot complete file receiving [{}]: index[{}] => {}", guid, file_count, message));

			return file_manager_->failure(guid, message);
		}

		Logger::handle().write(LogTypes::Debug, std::format("completed file receiving [{}]: index[{}] => {}", guid, file_count, message));

		if (received_file_callback_)
		{
			received_file_callback_(message, Converter::to_array(temp_file_path.value()));
		}

		return file_manager_->success(guid, message, temp_file_path.value());
	}

	auto NetworkClient::received_files(const std::vector<std::string>& failures,
									   const std::vector<std::pair<std::string, std::string>>& successes) -> std::expected<void, std::string>
	{
		if (received_files_callback_ == nullptr)
		{
			return std::unexpected("there is no callback to handle received files");
		}

		return received_files_callback_(failures, successes);
	}
}
