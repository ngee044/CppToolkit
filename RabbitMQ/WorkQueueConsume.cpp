#include "WorkQueueConsume.h"

#include "Logger.h"

#include <format>

#include <iostream>

using namespace Utilities;

namespace RabbitMQ
{
	WorkQueueConsume::WorkQueueConsume(const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options)
		: RabbitMQBase(host, port, user_name, password, ssl_options), declare_(std::nullopt)
	{
	}

	auto WorkQueueConsume::connect(const int& heartbeat) -> std::tuple<bool, std::optional<std::string>>
	{
		declare_.reset();

		return basic_connect(heartbeat);
	}

	auto WorkQueueConsume::start_consume(void) -> std::tuple<bool, std::optional<std::string>> { return basic_start_consume(); }

	auto WorkQueueConsume::stop_consume(void) -> std::tuple<bool, std::optional<std::string>> { return basic_stop_consume(); }

	auto WorkQueueConsume::disconnect(void) -> std::tuple<bool, std::optional<std::string>>
	{
		declare_.reset();

		return basic_disconnect();
	}

	auto WorkQueueConsume::channel_open(const int& channel_id, const std::string& queue_name) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
	{
		if (conn_ == nullptr)
		{
			return { std::nullopt, "Connection is not established" };
		}

		if (declare_.has_value())
		{
			return { std::nullopt, "Channel is already opened" };
		}

		declare_ = { channel_id, queue_name };

		auto [channel_opened, channel_open_error] = basic_channel_open(channel_id);
		if (!channel_opened)
		{
			declare_.reset();
			return { std::nullopt, channel_open_error };
		}

		auto [declared, declare_error] = declare_queue();
		if (!declared.has_value())
		{
			declare_.reset();
			return { std::nullopt, declare_error };
		}

		return { declared, std::nullopt };
	}

	auto WorkQueueConsume::channel_close(void) -> std::tuple<bool, std::optional<std::string>>
	{
		if (conn_ == nullptr)
		{
			return { false, "Connection is not established" };
		}

		if (!declare_.has_value())
		{
			return { false, "Channel is not opened" };
		}

		int channel_id = 0;
		std::tie(channel_id, std::ignore) = declare_.value();

		auto [closed, close_error] = basic_channel_close(channel_id);
		if (!closed)
		{
			return { false, close_error };
		}

		declare_.reset();

		return { true, std::nullopt };
	}

	auto WorkQueueConsume::prepare_consume(void) -> std::tuple<bool, std::optional<std::string>>
	{
		if (conn_ == nullptr)
		{
			return { false, "Connection is not established" };
		}

		if (!declare_.has_value())
		{
			return { false, "Channel is not opened" };
		}

		int channel_id = 0;
		std::tie(channel_id, std::ignore) = declare_.value();

		amqp_basic_qos_ok_t* result = amqp_basic_qos(conn_, channel_id, 0, 1, 0);
		switch (amqp_get_rpc_reply(conn_).reply_type)
		{
		case AMQP_RESPONSE_SERVER_EXCEPTION:
			return { false, std::format("server exception: {}", amqp_get_rpc_reply(conn_).reply.id) };
		case AMQP_RESPONSE_LIBRARY_EXCEPTION:
			return { false, std::format("library exception: {}", amqp_get_rpc_reply(conn_).reply.id) };
		case AMQP_RESPONSE_NONE:
			return { false, "no response from server" };
		default:
			break;
		}

		return { true, std::nullopt };
	}

	auto WorkQueueConsume::register_consume(
		const int& target_channel_id,
		const std::string& queue_name,
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>& callback)
		-> std::tuple<bool, std::optional<std::string>>
	{
		if (conn_ == nullptr)
		{
			return { false, "Connection is not established" };
		}

		auto [consumed, consume_error] = basic_register_consume(target_channel_id, queue_name, callback);
		if (!consumed)
		{
			return { false, consume_error };
		}

		return { true, std::nullopt };
	}

	auto WorkQueueConsume::unregister_consume(const int& target_channel_id, const std::string& queue_name) -> std::tuple<bool, std::optional<std::string>>
	{
		if (conn_ == nullptr)
		{
			return { false, "Connection is not established" };
		}

		return basic_unregister_consume(target_channel_id, queue_name);
	}

	auto WorkQueueConsume::declare_queue(void) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
	{
		if (conn_ == nullptr)
		{
			return { std::nullopt, "Connection is not established" };
		}

		if (!declare_.has_value())
		{
			return { std::nullopt, "Channel is not opened" };
		}

		auto [channel_id, queue_name] = declare_.value();

		bool passive = false;
		bool durable = true;
		bool exclusive = false;
		bool auto_delete = true;

		auto [declared, declare_error] = basic_declare_queue(conn_, channel_id, queue_name, passive, durable, exclusive, auto_delete);
		if (!declared.has_value())
		{
			return { std::nullopt, declare_error };
		}

		return { declared, std::nullopt };
	}

	auto WorkQueueConsume::redeclare_channel(void) -> std::tuple<bool, std::optional<std::string>>
	{
		if (conn_ == nullptr)
		{
			return { false, "Connection is not established" };
		}

		if (!declare_.has_value())
		{
			return { false, "Channel is not opened" };
		}

		int channel_id = 0;
		std::tie(channel_id, std::ignore) = declare_.value();

		auto [declared, declare_error] = declare_queue();
		if (!declared.has_value())
		{
			return { false, declare_error };
		}

		amqp_basic_qos_ok_t* result = amqp_basic_qos(conn_, channel_id, 0, 1, 0);
		switch (amqp_get_rpc_reply(conn_).reply_type)
		{
		case AMQP_RESPONSE_SERVER_EXCEPTION:
			return { false, std::format("server exception: {}", amqp_get_rpc_reply(conn_).reply.id) };
		case AMQP_RESPONSE_LIBRARY_EXCEPTION:
			return { false, std::format("library exception: {}", amqp_get_rpc_reply(conn_).reply.id) };
		case AMQP_RESPONSE_NONE:
			return { false, "no response from server" };
		default:
			break;
		}

		return { true, std::nullopt };
	}
}