#include "RabbitMQWorkQueueConsume.h"

#include "Logger.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include <iostream>

using namespace Utilities;

namespace RabbitMQ
{
RabbitMQWorkQueueConsume::RabbitMQWorkQueueConsume(const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options)
		: RabbitMQBase(host, port, user_name, password, ssl_options), declare_(std::nullopt)
	{
	}

void RabbitMQWorkQueueConsume::set_queue_policies(const std::optional<std::string>& dlx_exchange,
											  const std::optional<std::string>& dlx_routing_key,
											  const std::optional<uint32_t>& message_ttl_ms)
{
	dlx_exchange_ = dlx_exchange;
	dlx_routing_key_ = dlx_routing_key;
	message_ttl_ms_ = message_ttl_ms;
}

	auto RabbitMQWorkQueueConsume::connect(const int& heartbeat) -> std::tuple<bool, std::optional<std::string>>
	{
		declare_.reset();

		return basic_connect(heartbeat);
	}

	auto RabbitMQWorkQueueConsume::start_consume(void) -> std::tuple<bool, std::optional<std::string>> { return basic_start_consume(); }

	auto RabbitMQWorkQueueConsume::stop_consume(void) -> std::tuple<bool, std::optional<std::string>> { return basic_stop_consume(); }

	auto RabbitMQWorkQueueConsume::disconnect(void) -> std::tuple<bool, std::optional<std::string>>
	{
		declare_.reset();

		return basic_disconnect();
	}

	auto RabbitMQWorkQueueConsume::channel_open(const int& channel_id, const std::string& queue_name) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
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

	auto RabbitMQWorkQueueConsume::channel_close(void) -> std::tuple<bool, std::optional<std::string>>
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

	auto RabbitMQWorkQueueConsume::prepare_consume(void) -> std::tuple<bool, std::optional<std::string>>
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
			return { false, fmt::format("server exception: {}", amqp_get_rpc_reply(conn_).reply.id) };
		case AMQP_RESPONSE_LIBRARY_EXCEPTION:
			return { false, fmt::format("library exception: {}", amqp_get_rpc_reply(conn_).reply.id) };
		case AMQP_RESPONSE_NONE:
			return { false, "no response from server" };
		default:
			break;
		}

		return { true, std::nullopt };
	}

	auto RabbitMQWorkQueueConsume::register_consume(
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

	auto RabbitMQWorkQueueConsume::unregister_consume(const int& target_channel_id, const std::string& queue_name) -> std::tuple<bool, std::optional<std::string>>
	{
		if (conn_ == nullptr)
		{
			return { false, "Connection is not established" };
		}

		return basic_unregister_consume(target_channel_id, queue_name);
	}

	auto RabbitMQWorkQueueConsume::declare_queue(void) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
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
		bool auto_delete = false;

		// If no arguments configured, use the base helper
		if (!dlx_exchange_.has_value() && !message_ttl_ms_.has_value())
		{
			auto [declared, declare_error] = basic_declare_queue(conn_, channel_id, queue_name, passive, durable, exclusive, auto_delete);
			if (!declared.has_value())
			{
				return { std::nullopt, declare_error };
			}

			return { declared, std::nullopt };
		}

		// Build AMQP table with optional DLX/TTL settings
		std::vector<amqp_table_entry_t> entries;
		std::vector<std::string> keys_storage;
		std::vector<std::string> str_values_storage; // keep string memory alive until declare returns

		if (dlx_exchange_.has_value())
		{
			keys_storage.emplace_back("x-dead-letter-exchange");
			str_values_storage.emplace_back(dlx_exchange_.value());
			amqp_table_entry_t e{};
			e.key = amqp_cstring_bytes(keys_storage.back().c_str());
			e.value.kind = AMQP_FIELD_KIND_UTF8;
			e.value.value.bytes = amqp_cstring_bytes(str_values_storage.back().c_str());
			entries.push_back(e);
		}
		if (dlx_routing_key_.has_value())
		{
			keys_storage.emplace_back("x-dead-letter-routing-key");
			str_values_storage.emplace_back(dlx_routing_key_.value());
			amqp_table_entry_t e{};
			e.key = amqp_cstring_bytes(keys_storage.back().c_str());
			e.value.kind = AMQP_FIELD_KIND_UTF8;
			e.value.value.bytes = amqp_cstring_bytes(str_values_storage.back().c_str());
			entries.push_back(e);
		}
		if (message_ttl_ms_.has_value())
		{
			keys_storage.emplace_back("x-message-ttl");
			amqp_table_entry_t e{};
			e.key = amqp_cstring_bytes(keys_storage.back().c_str());
			e.value.kind = AMQP_FIELD_KIND_I32;
			e.value.value.i32 = static_cast<int32_t>(message_ttl_ms_.value());
			entries.push_back(e);
		}

		amqp_table_t args;
		args.num_entries = static_cast<int>(entries.size());
		args.entries = entries.empty() ? nullptr : entries.data();

		std::unique_lock<std::mutex> unique(mutex_);
		amqp_queue_declare_ok_t* declare_result
			= amqp_queue_declare(conn_, channel_id, amqp_cstring_bytes(queue_name.c_str()), passive, durable, exclusive, auto_delete, args);
		auto declare_reply = amqp_get_rpc_reply(conn_);
		unique.unlock();

		if (declare_reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			return { std::nullopt, fmt::format("queue declaration failed: {}", reply_message(declare_reply)) };
		}

		return { std::string((char*)declare_result->queue.bytes, declare_result->queue.len), std::nullopt };
	}

	auto RabbitMQWorkQueueConsume::redeclare_channel(void) -> std::tuple<bool, std::optional<std::string>>
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
			return { false, fmt::format("server exception: {}", amqp_get_rpc_reply(conn_).reply.id) };
		case AMQP_RESPONSE_LIBRARY_EXCEPTION:
			return { false, fmt::format("library exception: {}", amqp_get_rpc_reply(conn_).reply.id) };
		case AMQP_RESPONSE_NONE:
			return { false, "no response from server" };
		default:
			break;
		}

		return { true, std::nullopt };
	}
}
