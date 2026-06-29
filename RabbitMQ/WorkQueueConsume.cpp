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

	auto WorkQueueConsume::connect(int heartbeat) -> std::expected<void, std::string>
	{
		declare_.reset();

		return basic_connect(heartbeat);
	}

	auto WorkQueueConsume::start_consume(void) -> std::expected<void, std::string> { return basic_start_consume(); }

	auto WorkQueueConsume::stop_consume(void) -> std::expected<void, std::string> { return basic_stop_consume(); }

	auto WorkQueueConsume::disconnect(void) -> std::expected<void, std::string>
	{
		declare_.reset();

		return basic_disconnect();
	}

	auto WorkQueueConsume::channel_open(int channel_id, const std::string& queue_name) -> std::expected<std::string, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("Connection is not established");
		}

		if (declare_.has_value())
		{
			return std::unexpected("Channel is already opened");
		}

		declare_ = { channel_id, queue_name };

		auto channel_opened = basic_channel_open(channel_id);
		if (!channel_opened)
		{
			declare_.reset();
			return std::unexpected(channel_opened.error());
		}

		auto declared = declare_queue();
		if (!declared)
		{
			declare_.reset();
			return std::unexpected(declared.error());
		}

		return declared.value();
	}

	auto WorkQueueConsume::channel_close(void) -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("Connection is not established");
		}

		if (!declare_.has_value())
		{
			return std::unexpected("Channel is not opened");
		}

		int channel_id = 0;
		std::tie(channel_id, std::ignore) = declare_.value();

		auto closed = basic_channel_close(channel_id);
		if (!closed)
		{
			return std::unexpected(closed.error());
		}

		declare_.reset();

		return {};
	}

	auto WorkQueueConsume::prepare_consume(void) -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("Connection is not established");
		}

		if (!declare_.has_value())
		{
			return std::unexpected("Channel is not opened");
		}

		int channel_id = 0;
		std::tie(channel_id, std::ignore) = declare_.value();

		amqp_basic_qos_ok_t* result = amqp_basic_qos(conn_, channel_id, 0, 1, 0);
		auto reply = amqp_get_rpc_reply(conn_);
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(reply_message(reply));
		}

		return {};
	}

	auto WorkQueueConsume::register_consume(
		int target_channel_id,
		const std::string& queue_name,
		const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>& callback)
		-> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("Connection is not established");
		}

		auto consumed = basic_register_consume(target_channel_id, queue_name, callback);
		if (!consumed)
		{
			return std::unexpected(consumed.error());
		}

		return {};
	}

	auto WorkQueueConsume::unregister_consume(int target_channel_id, const std::string& queue_name) -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("Connection is not established");
		}

		return basic_unregister_consume(target_channel_id, queue_name);
	}

	auto WorkQueueConsume::declare_queue(void) -> std::expected<std::string, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("Connection is not established");
		}

		if (!declare_.has_value())
		{
			return std::unexpected("Channel is not opened");
		}

		auto [channel_id, queue_name] = declare_.value();

		bool passive = false;
		bool durable = true;
		bool exclusive = false;
		bool auto_delete = true;

		auto declared = basic_declare_queue(conn_, channel_id, queue_name, passive, durable, exclusive, auto_delete);
		if (!declared)
		{
			return std::unexpected(declared.error());
		}

		return declared.value();
	}

	auto WorkQueueConsume::redeclare_channel(void) -> std::expected<void, std::string>
	{
		if (conn_ == nullptr)
		{
			return std::unexpected("Connection is not established");
		}

		if (!declare_.has_value())
		{
			return std::unexpected("Channel is not opened");
		}

		int channel_id = 0;
		std::tie(channel_id, std::ignore) = declare_.value();

		auto declared = declare_queue();
		if (!declared)
		{
			return std::unexpected(declared.error());
		}

		amqp_basic_qos_ok_t* result = amqp_basic_qos(conn_, channel_id, 0, 1, 0);
		auto reply = amqp_get_rpc_reply(conn_);
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return std::unexpected(reply_message(reply));
		}

		return {};
	}
}