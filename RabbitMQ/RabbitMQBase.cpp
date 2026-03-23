#include "RabbitMQBase.h"

#include "Job.h"
#include "Logger.h"
#include "Converter.h"
#include "ThreadWorker.h"

#include <format>

#ifdef _WIN32
#include "winsock.h"
#endif

#include <future>

using namespace Thread;
using namespace Utilities;

#ifndef RABBITMQ_SSL_AVAILABLE
#define RABBITMQ_SSL_AVAILABLE 0
#endif

namespace RabbitMQ
{
	RabbitMQBase::RabbitMQBase(const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options)
		: host_(host)
		, port_(port)
		, user_name_(user_name)
		, password_(password)
		, ssl_options_(ssl_options)
		, conn_(nullptr)
		, thread_pool_(nullptr)
		, stop_future_()
		, continue_receiving_(false)
		, consume_information_container_(nullptr)
	{
	}

	RabbitMQBase::~RabbitMQBase()
	{
		stop();
	}

	auto RabbitMQBase::start() -> std::expected<void, std::string>
	{
		stop();

		auto created = create_thread_pool();
		if (!created)
		{
			return std::unexpected(created.error());
		}

		auto started = thread_pool_->start();
		if (!started)
		{
			return std::unexpected(started.error());
		}

		return {};
	}

	auto RabbitMQBase::wait_stop() -> std::expected<void, std::string>
	{
		stop_promise_ = std::make_unique<std::promise<void>>();
		stop_future_ = stop_promise_->get_future();

		stop_future_.wait();

		return {};
	}

	auto RabbitMQBase::stop() -> void
	{
		basic_disconnect();

		destroy_thread_pool();

		if (stop_promise_ != nullptr)
		{
			try
			{
				stop_promise_->set_value();
			}
			catch (const std::future_error&)
			{
			}
			stop_promise_.reset();
		}
	}

	auto RabbitMQBase::basic_publish(int target_channel_id,
									const std::string& exchange,
									const std::string& routing_key,
									const std::string& message,
									const std::string& exchange_mode,
									const std::string& content_type,
									DeliveryMode delivery_mode,
									bool use_confirm_select,
									const std::optional<uint32_t>& expiration_millisecond) -> std::expected<void, std::string>
	{
		Logger::handle().write(LogTypes::Sequence, std::format("attempt to publish message: routing_key[{}] => {} bytes", routing_key, message.length()));

		auto conn = amqp_new_connection();

		std::string socket_type = "";
		amqp_socket_t* socket = nullptr;
		if (!ssl_options_.use_ssl())
		{
			socket = amqp_tcp_socket_new(conn);
			if (!socket)
			{
				return std::unexpected("create TCP socket failed");
			}

			socket_type = "TCP";
		}
#if RABBITMQ_SSL_AVAILABLE
		else
		{
			socket = amqp_ssl_socket_new(conn);
			if (!socket)
			{
				return std::unexpected("create SSL socket failed");
			}

			auto ssl_setup = basic_ssl_setup(socket);
			if (!ssl_setup)
			{
				return std::unexpected(ssl_setup.error());
			}

			socket_type = "SSL/TLS";
		}
#else
		else
		{
			return std::unexpected("SSL/TLS requested, but rabbitmq-c in this build lacks SSL support");
		}
#endif

		auto status = amqp_socket_open(socket, host_.c_str(), port_);
		if (status != AMQP_STATUS_OK)
		{
			return std::unexpected(std::format("opening {} socket failed: {}", socket_type, status_message(static_cast<amqp_status_enum_>(status))));
		}

		auto reply = amqp_login(conn, "/", AMQP_DEFAULT_MAX_CHANNELS, AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN, user_name_.c_str(), password_.c_str());
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("logging in failed: {}", reply_message(reply)));
		}

		amqp_channel_open(conn, target_channel_id);
		reply = amqp_get_rpc_reply(conn);
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("opening channel failed: {}", reply_message(reply)));
		}

		if (exchange_mode.empty())
		{
			bool passive = false;
			bool durable = true;
			bool exclusive = false;
			bool auto_delete = true;

			auto declare_queue = basic_declare_queue(conn, target_channel_id, exchange, passive, durable, exclusive, auto_delete);
			if (!declare_queue)
			{
				return std::unexpected(declare_queue.error());
			}
		}
		else
		{
			bool passive = false;
			bool durable = true;
			bool auto_delete = false;
			bool internal = false;

			amqp_exchange_declare_ok_t* result
				= amqp_exchange_declare(conn, target_channel_id, amqp_cstring_bytes(exchange.c_str()), amqp_cstring_bytes(exchange_mode.c_str()), passive, durable, auto_delete, internal, amqp_empty_table);
			if (amqp_get_rpc_reply(conn).reply_type != AMQP_RESPONSE_NORMAL)
			{
				return std::unexpected("declare exchange failed");
			}
		}

		if (use_confirm_select)
		{
			amqp_confirm_select_ok_t* confirm_result = amqp_confirm_select(conn, target_channel_id);
			auto confirm_reply = amqp_get_rpc_reply(conn);
			if (confirm_reply.reply_type != AMQP_RESPONSE_NORMAL)
			{
				return std::unexpected(std::format("confirm select failed: {}", reply_message(confirm_reply)));
			}
		}

		amqp_bytes_t message_bytes = amqp_cstring_bytes(message.c_str());
		amqp_basic_properties_t props;
		props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
		props.content_type = amqp_cstring_bytes(content_type.c_str());
		props.delivery_mode = static_cast<uint8_t>(delivery_mode);

		if (expiration_millisecond.has_value())
		{
			props._flags |= AMQP_BASIC_EXPIRATION_FLAG;
			props.expiration = amqp_cstring_bytes(std::to_string(expiration_millisecond.value()).c_str());
		}

		status = amqp_basic_publish(conn, target_channel_id, amqp_cstring_bytes(exchange.c_str()), amqp_cstring_bytes(routing_key.c_str()), 0, 0, &props, message_bytes);
		reply = amqp_get_rpc_reply(conn);

		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("failed to send message: {}", reply_message(reply)));
		}

		if (status != AMQP_STATUS_OK)
		{
			return std::unexpected(std::format("failed to send message: {}", status_message(static_cast<amqp_status_enum_>(status))));
		}

		Logger::handle().write(LogTypes::Sequence, std::format("published message: routing_key[{}] => {} bytes", routing_key, message.length()));

		amqp_channel_close(conn, target_channel_id, AMQP_REPLY_SUCCESS);
		reply = amqp_get_rpc_reply(conn);
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("closing channel failed: {}", reply_message(reply)));
		}

		reply = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("closing connection failed: {}", reply_message(reply)));
		}

		status = amqp_destroy_connection(conn);
		if (status != AMQP_STATUS_OK)
		{
			return std::unexpected(std::format("destroying connection failed: {}", status_message(static_cast<amqp_status_enum_>(status))));
		}

		conn = nullptr;

		return {};
	}

	auto RabbitMQBase::basic_register_consume(int target_channel_id, const std::string& target_queue_name,
												const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>& callback) -> std::expected<void, std::string>
	{
		if (!conn_)
		{
			return std::unexpected("connection is not established");
		}

		auto added = consume_information_container_->add_consume_information(ConsumeInformation(target_channel_id ,target_queue_name, callback));
		if (!added)
		{
			return std::unexpected(added.error());
		}

		auto registered = register_consumer(target_channel_id, target_queue_name);
		if (!registered)
		{
			return std::unexpected(registered.error());
		}

		return {};
	}

	auto RabbitMQBase::basic_unregister_consume(int target_channel_id, const std::string& target_queue) -> std::expected<void, std::string>
	{
		if (!conn_)
		{
			return std::unexpected("connection is not established");
		}

		auto removed_information = consume_information_container_->remove_consume_information(target_queue);
		if (!removed_information)
		{
			return std::unexpected(removed_information.error());
		}

		auto consume_information = removed_information.value();

		auto unregistered = unregister_consumer(consume_information.get_channel_id());
		if (!unregistered)
		{
			return std::unexpected(unregistered.error());
		}

		// TODO: complete unregister implementation — currently unregisters the consumer
		// from the broker but does not fully clean up the local state
		return {};
	}

	auto RabbitMQBase::basic_connect(int heartbeat) -> std::expected<void, std::string>
	{
		basic_disconnect();

		conn_ = amqp_new_connection();

		consume_information_container_ = std::make_unique<ConsumeInformationContainer>(heartbeat);

		std::string socket_type = "";
		amqp_socket_t* socket = nullptr;
		if (!ssl_options_.use_ssl())
		{
			try
			{
				socket = amqp_tcp_socket_new(conn_);
			}
			catch (const std::exception& e)
			{
				return std::unexpected(std::format("creating TCP socket failed: {}", e.what()));
			}

			if (!socket)
			{
				return std::unexpected("creating TCP socket failed");
			}

			socket_type = "TCP";
		}
#if RABBITMQ_SSL_AVAILABLE
		else
		{
			try
			{
				socket = amqp_ssl_socket_new(conn_);
			}
			catch (const std::exception& e)
			{
				return std::unexpected(std::format("creating SSL/TLS socket failed: {}", e.what()));
			}

			if (!socket)
			{
				return std::unexpected("creating SSL/TLS socket failed");
			}

			try
			{
				auto ssl_setup = basic_ssl_setup(socket);

				if (!ssl_setup)
				{
					return std::unexpected(ssl_setup.error());
				}
			}
			catch (const std::exception& e)
			{
				return std::unexpected(std::format("SSL/TLS setup failed: {}", e.what()));
			}

			socket_type = "SSL/TLS";
		}
#else
		else
		{
			return std::unexpected("SSL/TLS requested, but rabbitmq-c in this build lacks SSL support");
		}
#endif

		return basic_login(socket, socket_type, heartbeat);
	}
	auto RabbitMQBase::basic_disconnect() -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return {};
		}

		amqp_rpc_reply_t reply = amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("closing connection failed: {}", reply_message(reply)));
		}

		int status = amqp_destroy_connection(conn_);
		if (status != AMQP_STATUS_OK)
		{
			return std::unexpected(std::format("destroying connection failed: {}", status_message(static_cast<amqp_status_enum_>(status))));
		}

		consume_information_container_.reset();

		conn_ = nullptr;

		return {};
	}
	auto RabbitMQBase::basic_start_consume() -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("cannot start to consuming_start: connection is not established");
		}

		if (thread_pool_ == nullptr)
		{
			return std::unexpected("cannot start to consuming_start: thread pool is not created");
		}

		if (continue_receiving_.load())
		{
			return std::unexpected("cannot start to consuming_start: message receiving is already in progress");
		}

		continue_receiving_.store(true);

		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::LongTerm }));

		return thread_pool_->push(std::make_shared<Job>(
			JobPriorities::LongTerm,
			[this](void) -> std::expected<void, std::string>
			{
				amqp_frame_t frame;
				bool reconnection = false;

				while (continue_receiving_.load())
				{
					if (reconnection)
					{
						auto connected = reconnect();
						if (!connected)
						{
							Logger::handle().write(LogTypes::Error, std::format("cannot reconnect to consume message: {}", connected.error()));
							continue;
						}

						reconnection = false;
					}

					amqp_rpc_reply_t res;
					amqp_envelope_t envelope;

					try
					{
						struct timeval timeout = { 1, 0 };

						amqp_maybe_release_buffers(conn_);
						res = amqp_consume_message(conn_, &envelope, &timeout, 0);

						if (res.reply_type != AMQP_RESPONSE_NORMAL)
						{
							amqp_destroy_envelope(&envelope);

							if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && res.library_error == AMQP_STATUS_TIMEOUT)
							{
								continue;
							}

							Logger::handle().write(LogTypes::Sequence, std::format("attempt to reconnect due to: {}", reply_message(res)));

							reconnection = true;

							continue;
						}

						auto exchange = std::string((char*)envelope.exchange.bytes, envelope.exchange.len);
						auto routing_key = std::string((char*)envelope.routing_key.bytes, envelope.routing_key.len);

						std::vector<uint8_t> content_type;
						if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG)
						{
							content_type = std::vector<uint8_t>((uint8_t*)envelope.message.properties.content_type.bytes,
																(uint8_t*)envelope.message.properties.content_type.bytes + envelope.message.properties.content_type.len);
						}

						std::vector<uint8_t> received_message((uint8_t*)envelope.message.body.bytes, (uint8_t*)envelope.message.body.bytes + envelope.message.body.len);

						auto callback_opt = consume_information_container_->get_consume_callback(routing_key);
						if (!callback_opt.has_value())
						{
							Logger::handle().write(LogTypes::Warning, std::format("message consume error: routing key not found"));

							amqp_basic_nack(conn_, envelope.channel, envelope.delivery_tag, 0, 1);

							continue;
						}

						try
						{
							auto callback = callback_opt.value();
							auto result = callback(routing_key, Converter::to_string(received_message), Converter::to_string(content_type));
							if (!result)
							{
								Logger::handle().write(LogTypes::Error, std::format("message consume error: {}", result.error()));

								amqp_basic_nack(conn_, envelope.channel, envelope.delivery_tag, 0, 1);
							}
							else
							{
								amqp_basic_ack(conn_, envelope.channel, envelope.delivery_tag, 0);
							}
						}
						catch (const std::exception& e)
						{
							Logger::handle().write(LogTypes::Exception, std::format("message consume exception: {}", e.what()));
							reconnection = true;

							amqp_basic_nack(conn_, envelope.channel, envelope.delivery_tag, 0, 1);
						}
						catch (...)
						{
							Logger::handle().write(LogTypes::Exception, "message consume exception: unexpected error");
							reconnection = true;

							amqp_basic_nack(conn_, envelope.channel, envelope.delivery_tag, 0, 1);
						}
					}
					catch (const std::exception& message)
					{
						Logger::handle().write(LogTypes::Exception, std::format("message consume exception: {}", message.what()));
						reconnection = true;

						amqp_basic_nack(conn_, envelope.channel, envelope.delivery_tag, 0, 1);
					}
					catch (...)
					{
						Logger::handle().write(LogTypes::Exception, "message consume exception: unexpected error");
						reconnection = true;

						amqp_basic_nack(conn_, envelope.channel, envelope.delivery_tag, 0, 1);
					}

					amqp_destroy_envelope(&envelope);
				}

				return {};
			},
			"main_job_for_rabbitmq", false));
	}

	auto RabbitMQBase::basic_stop_consume() -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("cannot start to consuming_stop: connection is not established");
		}

		if (thread_pool_ == nullptr)
		{
			return std::unexpected("cannot start to consuming_stop: thread pool is not created");
		}

		continue_receiving_.store(false);

		if (thread_pool_ != nullptr)
		{
			thread_pool_->remove_workers(JobPriorities::LongTerm);
		}

		return {};
	}

#if RABBITMQ_SSL_AVAILABLE
	auto RabbitMQBase::basic_ssl_setup(amqp_socket_t* socket) -> std::expected<void, std::string>
	{
		amqp_ssl_socket_set_verify_peer(socket, 0);
		amqp_ssl_socket_set_verify_hostname(socket, 0);

		int status = AMQP_STATUS_OK;
		if (!ssl_options_.ca_cert().empty())
		{
			status = amqp_ssl_socket_set_cacert(socket, ssl_options_.ca_cert().c_str());
			if (status != AMQP_STATUS_OK)
			{
				return std::unexpected(std::format("setting CA certificate failed: {}", status_message(static_cast<amqp_status_enum_>(status))));
			}

			if (!ssl_options_.engine().empty())
			{
				status = amqp_set_ssl_engine(ssl_options_.engine().c_str());
				if (status != AMQP_STATUS_OK)
				{
					return std::unexpected(std::format("setting SSL engine failed: {}", status_message(static_cast<amqp_status_enum_>(status))));
				}
			}

			amqp_ssl_socket_set_verify_peer(socket, ssl_options_.verify_peer() ? 1 : 0);
			amqp_ssl_socket_set_verify_hostname(socket, ssl_options_.verify_hostname() ? 1 : 0);
		}

		if (!ssl_options_.client_cert().empty() && !ssl_options_.client_key().empty())
		{
			status = amqp_ssl_socket_set_key(socket, ssl_options_.client_cert().c_str(), ssl_options_.client_key().c_str());
			if (status != AMQP_STATUS_OK)
			{
				return std::unexpected(std::format("setting client certificate and key failed: {}", status_message(static_cast<amqp_status_enum_>(status))));
			}
		}

		return {};
	}
#else
	auto RabbitMQBase::basic_ssl_setup(amqp_socket_t* /*socket*/) -> std::expected<void, std::string>
	{
		return std::unexpected("SSL/TLS support is not available in rabbitmq-c for this build");
	}
#endif

	auto RabbitMQBase::basic_login(amqp_socket_t* socket, const std::string& socket_type, int heartbeat) -> std::expected<void, std::string>
	{
		int status = amqp_socket_open(socket, host_.c_str(), port_);
		if (status != AMQP_STATUS_OK)
		{
			return std::unexpected(std::format("opening {} socket failed: {}", socket_type, status_message(static_cast<amqp_status_enum_>(status))));
		}

		auto reply = amqp_login(conn_, "/", AMQP_DEFAULT_MAX_CHANNELS, AMQP_DEFAULT_FRAME_SIZE, heartbeat, AMQP_SASL_METHOD_PLAIN, user_name_.c_str(), password_.c_str());
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("logging in failed: {}", reply_message(reply)));
		}

		return {};
	}

	auto RabbitMQBase::basic_start(amqp_socket_t* socket, const std::string& socket_type) -> std::expected<void, std::string>
	{
		auto logged_in = basic_login(socket, socket_type);
		if (!logged_in)
		{
			return std::unexpected(logged_in.error());
		}

		auto created = create_thread_pool();
		if (!created)
		{
			return std::unexpected(created.error());
		}

		auto started = thread_pool_->start();
		if (!started)
		{
			return std::unexpected(started.error());
		}

		return {};
	}

	auto RabbitMQBase::basic_channel_open(int channel_id) -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("connection is not established");
		}

		amqp_channel_open(conn_, channel_id);

		auto reply = amqp_get_rpc_reply(conn_);
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("opening channel failed: {}", reply_message(reply)));
		}

		return {};
	}

	auto RabbitMQBase::basic_channel_close(int channel_id) -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("connection is not established");
		}

		amqp_channel_close(conn_, channel_id, AMQP_REPLY_SUCCESS);

		auto reply = amqp_get_rpc_reply(conn_);
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("closing channel failed: {}", reply_message(reply)));
		}

		return {};
	}

	auto RabbitMQBase::basic_declare_queue(amqp_connection_state_t conn, int target_channel_id, const std::string& queue_name, bool passive, bool durable, bool exclusive, bool auto_delete) -> std::expected<std::string, std::string>
	{
		std::unique_lock<std::mutex> unique(mutex_);
		amqp_queue_declare_ok_t* declare_result
			= amqp_queue_declare(conn, target_channel_id, amqp_cstring_bytes(queue_name.c_str()), passive, durable, exclusive, auto_delete, amqp_empty_table);
		auto declare_reply = amqp_get_rpc_reply(conn);
		unique.unlock();

		if (declare_reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("queue declaration failed: {}", reply_message(declare_reply)));
		}

		return std::string((char*)declare_result->queue.bytes, declare_result->queue.len);
	}

	auto RabbitMQBase::basic_delete_queue(amqp_connection_state_t conn, int target_channel_id, const std::string& queue_name, bool if_unused, bool if_empty) -> std::expected<void, std::string>
	{
		std::unique_lock<std::mutex> unique(mutex_);
		amqp_queue_delete_ok_t* result = amqp_queue_delete(conn, target_channel_id, amqp_cstring_bytes(queue_name.c_str()), if_unused, if_empty);
		auto reply = amqp_get_rpc_reply(conn);
		unique.unlock();

		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("deleting queue failed: {}", reply_message(reply)));
		}

		return {};
	}

	auto RabbitMQBase::basic_bind_queue(amqp_connection_state_t conn,
		int target_channel_id,
		const std::string& queue_name,
		const std::string& exchange,
		const std::string& routing_key) -> std::expected<void, std::string>
	{
		std::unique_lock<std::mutex> unique(mutex_);
		amqp_queue_bind_ok_t* result = amqp_queue_bind(conn, target_channel_id, amqp_cstring_bytes(queue_name.c_str()), amqp_cstring_bytes(exchange.c_str()),
							amqp_cstring_bytes(routing_key.c_str()), amqp_empty_table);
		auto reply = amqp_get_rpc_reply(conn);
		unique.unlock();

		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("binding queue failed: {}", reply_message(reply)));
		}

		return {};
	}

	auto RabbitMQBase::create_thread_pool() -> std::expected<void, std::string>
	{
		auto destroyed = destroy_thread_pool();
		if (!destroyed)
		{
			return std::unexpected(destroyed.error());
		}

		try
		{
			thread_pool_ = std::make_shared<ThreadPool>();
		}
		catch (const std::bad_alloc& e)
		{
			return std::unexpected(std::format("thread pool creation failed: {}", e.what()));
		}

		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal }));

		return {};
	}

	auto RabbitMQBase::destroy_thread_pool() -> std::expected<void, std::string>
	{
		if (thread_pool_ == nullptr)
		{
			return {};
		}

		thread_pool_->stop();

		thread_pool_.reset();

		return {};
	}

	auto RabbitMQBase::status_message(const amqp_status_enum_& status) -> std::string
	{
		std::string result = "";

		switch (status)
		{
		case AMQP_STATUS_OK:
			result = "Operation successful";
			break;
		case AMQP_STATUS_NO_MEMORY:
			result = "Memory allocation failed";
			break;
		case AMQP_STATUS_BAD_AMQP_DATA:
			result = "Incorrect or corrupt data received from the broker";
			break;
		case AMQP_STATUS_UNKNOWN_CLASS:
			result = "Unknown AMQP class received";
			break;
		case AMQP_STATUS_UNKNOWN_METHOD:
			result = "Unknown AMQP method received";
			break;
		case AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED:
			result = "Unable to resolve the hostname";
			break;
		case AMQP_STATUS_INCOMPATIBLE_AMQP_VERSION:
			result = "Incompatible AMQP version advertised by broker";
			break;
		case AMQP_STATUS_CONNECTION_CLOSED:
			result = "Connection to the broker has been closed";
			break;
		case AMQP_STATUS_BAD_URL:
			result = "Malformed AMQP URL";
			break;
		case AMQP_STATUS_SOCKET_ERROR:
			result = "Socket error occurred";
			break;
		case AMQP_STATUS_INVALID_PARAMETER:
			result = "Invalid parameter passed into the function";
			break;
		case AMQP_STATUS_TABLE_TOO_BIG:
			result = "amqp_table_t object cannot be serialized due to small output buffer";
			break;
		case AMQP_STATUS_WRONG_METHOD:
			result = "Wrong method received";
			break;
		case AMQP_STATUS_TIMEOUT:
			result = "Operation timed out";
			break;
		case AMQP_STATUS_TIMER_FAILURE:
			result = "Underlying system timer facility failed";
			break;
		case AMQP_STATUS_HEARTBEAT_TIMEOUT:
			result = "Timed out waiting for heartbeat";
			break;
		case AMQP_STATUS_UNEXPECTED_STATE:
			result = "Unexpected protocol state";
			break;
		case AMQP_STATUS_SOCKET_CLOSED:
			result = "Underlying socket is closed";
			break;
		case AMQP_STATUS_SOCKET_INUSE:
			result = "Underlying socket is already open";
			break;
		case AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD:
			result = "Broker does not support the requested SASL mechanism";
			break;
		case AMQP_STATUS_UNSUPPORTED:
			result = "Parameter is unsupported in this version";
			break;
		case AMQP_STATUS_TCP_ERROR:
			result = "Generic TCP error occurred";
			break;
		case AMQP_STATUS_TCP_SOCKETLIB_INIT_ERROR:
			result = "Error initializing the socket library";
			break;
		case AMQP_STATUS_SSL_ERROR:
			result = "Generic SSL error occurred";
			break;
		case AMQP_STATUS_SSL_HOSTNAME_VERIFY_FAILED:
			result = "SSL validation of hostname against peer certificate failed";
			break;
		case AMQP_STATUS_SSL_PEER_VERIFY_FAILED:
			result = "SSL validation of peer certificate failed";
			break;
		case AMQP_STATUS_SSL_CONNECTION_FAILED:
			result = "SSL handshake failed";
			break;
		case AMQP_STATUS_SSL_SET_ENGINE_FAILED:
			result = "SSL setting engine failed";
			break;
		case AMQP_STATUS_SSL_UNIMPLEMENTED:
			result = "SSL API is not implemented";
			break;
		default:
			result = std::format("Unknown status: {}", static_cast<int>(status));
			break;
		}

		return result;
	}

	auto RabbitMQBase::reply_message(const amqp_rpc_reply_t& reply) -> std::string
	{
		std::string result = "";

		switch (reply.reply_type)
		{
		case AMQP_RESPONSE_NORMAL:
			result = "response normal, the RPC completed successfully";
			break;
		case AMQP_RESPONSE_NONE:
			result = "the library got an EOF from the socket";
			break;
		case AMQP_RESPONSE_LIBRARY_EXCEPTION:
			result = std::format("library exception: {}", status_message(static_cast<amqp_status_enum_>(reply.library_error)));
			break;
		case AMQP_RESPONSE_SERVER_EXCEPTION:
		{
			switch (reply.reply.id)
			{
			case AMQP_CONNECTION_CLOSE_METHOD:
			{
				amqp_channel_close_t* m = (amqp_channel_close_t*)reply.reply.decoded;
				result = std::format("server connection error {}h, message: {}", m->reply_code, std::string((char*)m->reply_text.bytes, m->reply_text.len));
				break;
			}
			case AMQP_CHANNEL_CLOSE_METHOD:
			{
				amqp_channel_close_t* m = (amqp_channel_close_t*)reply.reply.decoded;
				result = std::format("server channel error {}h, message: {}", m->reply_code, std::string((char*)m->reply_text.bytes, m->reply_text.len));
				break;
			}
			default:
				result = std::format("unknown server error, method id 0x{:08X}", reply.reply.id);
				break;
			}
		}
		break;
		}

		return result;
	}

	auto RabbitMQBase::register_consumer(int target_channel_id, const std::string& target_queue) -> std::expected<void, std::string>
	{
		std::unique_lock<std::mutex> unique(mutex_);
		amqp_basic_consume_ok_t* result
			= amqp_basic_consume(conn_, target_channel_id, amqp_cstring_bytes(target_queue.c_str()), amqp_empty_bytes, 0, 0, 0, amqp_empty_table);
		auto reply = amqp_get_rpc_reply(conn_);
		unique.unlock();

		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("consuming_start failed: {}", reply_message(reply)));
		}

		return {};
	}

	auto RabbitMQBase::unregister_consumer(int target_channel_id) -> std::expected<void, std::string>
	{
		std::unique_lock<std::mutex> unique(mutex_);
		amqp_basic_cancel_ok_t* result = amqp_basic_cancel(conn_, target_channel_id, amqp_empty_bytes);
		auto reply = amqp_get_rpc_reply(conn_);
		unique.unlock();

		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(std::format("consuming_stop failed: {}", reply_message(reply)));
		}

		return {};
	}

	auto RabbitMQBase::reconnect(void) -> std::expected<void, std::string>
	{
		if (consume_information_container_ == nullptr)
		{
			return std::unexpected("cannot reconnect: consume information container is not created");
		}

		auto heartbeat = consume_information_container_->get_heartbeat();
		auto consume_informations = consume_information_container_->get_consume_informations();

		auto connected = basic_connect(heartbeat);
		if (!connected)
		{
			return std::unexpected(connected.error());
		}

		for (auto& consume_information : consume_informations)
		{
			auto opened = basic_channel_open(consume_information.get_channel_id());
			if (!opened)
			{
				consume_information_container_.reset();
				consume_information_container_ = std::make_unique<ConsumeInformationContainer>(heartbeat, consume_informations);

				return std::unexpected(opened.error());
			}

			auto declared = redeclare_channel();
			if (!declared)
			{
				consume_information_container_.reset();
				consume_information_container_ = std::make_unique<ConsumeInformationContainer>(heartbeat, consume_informations);

				return std::unexpected(declared.error());
			}

			auto registered
				= basic_register_consume(consume_information.get_channel_id(), consume_information.get_queue_name(), consume_information.get_callback());
			if (!registered)
			{
				consume_information_container_.reset();
				consume_information_container_ = std::make_unique<ConsumeInformationContainer>(heartbeat, consume_informations);

				return std::unexpected(registered.error());
			}
		}

		return {};
	}

}