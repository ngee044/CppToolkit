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
	class RedisWorkQueueConsume
	{
	public:
		RedisWorkQueueConsume(
			const std::string& consumer_name, const std::string& address, int port = 6379, const TLSOptions& tls_options = TLSOptions(), int db_index = 0);
		~RedisWorkQueueConsume();

		auto subscribe(const std::string& queue_name,
					   const std::string& consumer_group,
					   std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)> task_callback,
					   std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)> expired_callback,
					   const std::optional<long>& timeout_seconds = 1) -> std::tuple<bool, std::optional<std::string>>;
		auto wait_stop() -> std::tuple<bool, std::optional<std::string>>;
		auto stop() -> std::tuple<bool, std::optional<std::string>>;

	private:
		RedisClient client_;
		std::atomic<bool> running_;
		std::unique_ptr<ThreadPool> thread_pool_;

		std::string queue_name_;
		std::string consumer_name_;
		std::string consumer_group_;

		std::future<void> stop_future_;
		std::optional<std::promise<void>> stop_promise_;
	};
}
