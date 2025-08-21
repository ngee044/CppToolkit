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
#include <boost/asio/steady_timer.hpp>

#include <locale>
#include <filesystem>

using namespace Thread;
using namespace Utilities;

namespace Network
{
	static const std::string k_heartbeat_ping = "heartbeat:ping";
	static const std::string k_heartbeat_pong = "heartbeat:pong";

#ifdef USE_ENCRYPT_MODULE
	NetworkSession::NetworkSession(const std::string& session_id,
								   const bool& encrypt,
								   const uint16_t& high_priority_count,
								   const uint16_t& normal_priority_count,
								   const uint16_t& low_priority_count,
								   const bool& heartbeat_enabled,
								   const uint32_t& heartbeat_interval_sec)
#else
	NetworkSession::NetworkSession(const std::string& session_id,
								   const uint16_t& high_priority_count,
								   const uint16_t& normal_priority_count,
								   const uint16_t& low_priority_count,
								   const bool& heartbeat_enabled,
								   const uint32_t& heartbeat_interval_sec)
#endif
		: DataHandler(high_priority_count, normal_priority_count, low_priority_count)
		, session_id_(0)
		, state_(SessionState::Create)
		, server_id_(session_id)
		, registered_key_("")
		, file_manager_(std::make_unique<FileManager>())
		, heartbeat_enabled_(heartbeat_enabled)
		, heartbeat_interval_sec_(heartbeat_interval_sec)
		, stopped_(false)
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

	auto NetworkSession::session_id(void) const -> SessionId { return session_id_; }

	auto NetworkSession::state(void) const -> SessionState { return state_; }

	auto NetworkSession::session_id(const SessionId& id) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);
		session_id_ = id;
	}

	auto NetworkSession::get_ptr(void) -> std::shared_ptr<NetworkSession> { return shared_from_this(); }

	auto NetworkSession::heartbeat(const bool& enable, const uint32_t& interval_sec) -> void
	{
		heartbeat_enabled_ = enable;
		heartbeat_interval_sec_ = interval_sec == 0 ? 1 : interval_sec;
	}

	auto NetworkSession::set_max_missed_heartbeats(const uint32_t& count) -> void
	{
		// Minimum of 1 to avoid immediate expiration
		max_missed_heartbeats_ = count == 0 ? 1 : count;
	}

	auto NetworkSession::start(std::shared_ptr<boost::asio::ip::tcp::socket> connected_socket, const size_t& socket_buffer_size) -> void
	{
		{
			std::scoped_lock<std::mutex> lock(mutex_);
			state_ = SessionState::Handshaking;
		}
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
		set_self_guard(shared_from_this());
		condition(ConnectConditions::Waiting);

		create_thread_pool(fmt::format("ThreadPool on NetworkSession on {}", id()));

		file_manager_->thread_pool(thread_pool());

		// Start handshake timeout timer (10s)
		try
		{
			auto s = strand();
			if (s)
			{
				handshake_timer_ = std::make_shared<boost::asio::steady_timer>(*s);
			}
			else
			{
				auto ex = socket()->get_executor();
				handshake_timer_ = std::make_shared<boost::asio::steady_timer>(ex);
			}
			handshake_timer_->expires_after(std::chrono::seconds(10));
			auto self = shared_from_this();
			handshake_timer_->async_wait(
				[self](const boost::system::error_code& ec)
				{
					if (ec == boost::asio::error::operation_aborted)
					{
						return;
					}
					// If not authenticated yet, expire the connection
					if (self->state() != SessionState::Authenticated && self->condition() != ConnectConditions::Expired)
					{
						Utilities::Logger::handle().write(Utilities::LogTypes::Debug, fmt::format("handshake timeout on session [{}:{}]", self->id(), self->sub_id()));
						self->condition(ConnectConditions::Expired);
					}
				});
		}
		catch (...)
		{
			// ignore timer init failure, proceed without timeout
		}

		read_start_code();
	}

	auto NetworkSession::stop(void) -> void
	{
		if (stopped_.exchange(true))
		{
			return;
		}
		{
			std::scoped_lock<std::mutex> lock(mutex_);
			state_ = SessionState::Closing;
		}
		condition(ConnectConditions::Expired, true);

		stop_heartbeat();

		if (handshake_timer_)
		{
			handshake_timer_->cancel();
			handshake_timer_.reset();
		}

		file_manager_->thread_pool(nullptr);

		destroy_thread_pool();
		destroy_socket();

		{
			std::scoped_lock<std::mutex> lock(mutex_);
			state_ = SessionState::Closed;
		}
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

		boost::json::object received_message;
		try
		{
			received_message = boost::json::parse(Converter::to_string(data)).as_object();
		}
		catch (const std::exception& e)
		{
			Utilities::Logger::handle().write(Utilities::LogTypes::Error, fmt::format("invalid connection json: {}", e.what()));
			return { false, "invalid connection json" };
		}

		if (!received_message.if_contains("id") || !received_message.at("id").is_string())
		{
			return { false, "invalid connection message: missing id" };
		}

		id(received_message.at("id").as_string().data());

		Logger::handle().write(LogTypes::Debug, fmt::format("received connection message from NetworkClient: ({})", id()));

		if (!received_message.if_contains("registered_key") || !received_message.at("registered_key").is_string()
			|| received_message.at("registered_key").as_string().data() != registered_key_)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("the registered key of the NetworkClient is not compatible with the server: ({})", id()));

			condition(ConnectConditions::Expired);

			return response_connection(false);
		}

		condition(ConnectConditions::Confirmed);
		{
			std::scoped_lock<std::mutex> lock(mutex_);
			state_ = SessionState::Authenticated;
		}
		if (handshake_timer_)
		{
			handshake_timer_->cancel();
			handshake_timer_.reset();
		}

		// Start heartbeat after authentication if enabled
		if (heartbeat_enabled_)
		{
			// initialize pong timestamp on successful authentication
			last_pong_at_ = std::chrono::steady_clock::now();
			missed_heartbeats_ = 0;
			start_heartbeat();
		}
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
			Logger::handle().write(LogTypes::Error, fmt::format("no binary-callback on [{}:{}] state:{}", id(), sub_id(), to_string(state_)));
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
			Logger::handle().write(LogTypes::Error, fmt::format("no message-callback on [{}:{}] state:{}", id(), sub_id(), to_string(state_)));
			return { false, "there is no callback to handle message data" };
		}

		auto msg = Converter::to_string(data);
		// Heartbeat handling
		if (msg.rfind(k_heartbeat_ping, 0) == 0)
		{
			Logger::handle().write(LogTypes::Debug, fmt::format("received heartbeat:ping from [{}:{}]", id(), sub_id()));
			// Reply pong and swallow
			send_message(k_heartbeat_pong);
			return { true, std::nullopt };
		}
		if (msg.rfind(k_heartbeat_pong, 0) == 0)
		{
			Logger::handle().write(LogTypes::Debug, fmt::format("received heartbeat:pong from [{}:{}]", id(), sub_id()));
			last_pong_at_ = std::chrono::steady_clock::now();
			missed_heartbeats_ = 0;
			return { true, std::nullopt };
		}

		return received_message_callback_(id(), sub_id(), msg);
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

		if (file_mode.empty())
		{
			return { false, "invalid file message: missing mode" };
		}

		size_t file_count = 0;
		if (file_index.size() != sizeof(size_t))
		{
			return { false, "invalid file message: bad index length" };
		}
		memcpy(&file_count, file_index.data(), sizeof(size_t));

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

	auto NetworkSession::start_heartbeat(void) -> void
	{
		try
		{
			auto s = strand();
			if (s)
			{
				heartbeat_timer_ = std::make_shared<boost::asio::steady_timer>(*s);
			}
			else
			{
				auto ex = socket()->get_executor();
				heartbeat_timer_ = std::make_shared<boost::asio::steady_timer>(ex);
			}
			heartbeat_timer_->expires_after(std::chrono::seconds(heartbeat_interval_sec_));
			auto self = shared_from_this();
			heartbeat_timer_->async_wait(
				[self](const boost::system::error_code& ec)
				{
					if (ec == boost::asio::error::operation_aborted)
					{
						return;
					}
					if (self->condition() == ConnectConditions::Expired)
					{
						return;
					}
					// Check missed heartbeats
					auto now = std::chrono::steady_clock::now();
					auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - self->last_pong_at_).count();
					if (elapsed >= static_cast<long long>(self->heartbeat_interval_sec_) * self->max_missed_heartbeats_)
					{
						Utilities::Logger::handle().write(Utilities::LogTypes::Error,
														  fmt::format("heartbeat timeout on [{}:{}], elapsed {}s >= {}s; expiring", self->id(), self->sub_id(), elapsed,
																	  static_cast<long long>(self->heartbeat_interval_sec_) * self->max_missed_heartbeats_));
						self->condition(ConnectConditions::Expired);
						return;
					}

					Utilities::Logger::handle().write(Utilities::LogTypes::Debug, fmt::format("send heartbeat:ping to [{}:{}]", self->id(), self->sub_id()));
					self->send_message(k_heartbeat_ping);
					// reschedule
					self->start_heartbeat();
				});
		}
		catch (...)
		{
			// ignore heartbeat init failure
		}
	}

	auto NetworkSession::stop_heartbeat(void) -> void
	{
		if (heartbeat_timer_)
		{
			heartbeat_timer_->cancel();
			heartbeat_timer_.reset();
		}
	}
}
