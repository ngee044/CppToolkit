#pragma once

#include "ThreadPool.h"
#include "RedisClient.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <future>
#include <string>
#include <thread>
#include <optional>
#include <functional>

using namespace Thread;

namespace Redis
{
	class RedisWorkQueueEmitter
	{
	public:
		RedisWorkQueueEmitter(const std::string& address, int port = 6379, const TLSOptions& tls_options = TLSOptions(), int db_index = 0);
		~RedisWorkQueueEmitter();

		auto publish(const std::string& queue_name,
					 const std::string& task,
					 const std::string& message_type,
					 const std::optional<long>& ttl_seconds = std::nullopt) -> std::tuple<bool, std::optional<std::string>>;
		auto publish_async(const std::string& queue_name,
						   const std::string& task,
						   const std::string& message_type,
						   const std::optional<long>& ttl_seconds = std::nullopt) -> std::tuple<bool, std::optional<std::string>>;

	private:
		RedisClient client_;
		std::unique_ptr<ThreadPool> thread_pool_;
	};
}
