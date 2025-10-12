#pragma once

#include "RabbitMQBase.h"
#include "ThreadPool.h"

#include <functional>
#include <memory>
#include <string>

using namespace Thread;

namespace RabbitMQ
{
	class RabbitMQWorkQueueEmitter : public RabbitMQBase
	{
	public:
		RabbitMQWorkQueueEmitter(const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options = SSLOptions());
		~RabbitMQWorkQueueEmitter();

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

	private:
		std::unique_ptr<ThreadPool> thread_pool_;
	};
}
