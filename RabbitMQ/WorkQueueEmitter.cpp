#include "WorkQueueEmitter.h"

#include "Logger.h"

#include <format>

#include <iostream>

using namespace Utilities;

namespace RabbitMQ
{
	WorkQueueEmitter::WorkQueueEmitter(
		const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options)
		: RabbitMQBase(host, port, user_name, password, ssl_options)
	{
	}

	auto WorkQueueEmitter::publish(int target_channel_id,
									   const std::string& queue_name,
									   const std::string& message,
									   const std::string& content_type,
									   const std::optional<uint32_t>& expiration_millisecond) -> std::expected<void, std::string>
	{
		return basic_publish(target_channel_id, "", queue_name, message, "", content_type, DeliveryMode::Persistent, true, expiration_millisecond);
	}
}