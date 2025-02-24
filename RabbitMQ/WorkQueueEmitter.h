#pragma once

#include "RabbitMQBase.h"

#include <functional>
#include <string>

namespace RabbitMQ
{
	class WorkQueueEmitter : public RabbitMQBase
	{
	public:
		WorkQueueEmitter(const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options = SSLOptions());

		auto publish(const int& target_channel_id,
					 const std::string& queue_name,
					 const std::string& message,
					 const std::string& content_type = "text/plain",
					 const std::optional<uint32_t>& expiration_millisecond = std::nullopt) -> std::tuple<bool, std::optional<std::string>>;
		auto publish_async(const int& target_channel_id,
						   const std::string& queue_name,
						   const std::string& message,
						   const std::string& content_type = "text/plain",
						   const std::optional<uint32_t>& expiration_millisecond = std::nullopt) -> std::tuple<bool, std::optional<std::string>>;
	};
}
