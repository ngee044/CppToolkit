#include "RedisWorkQueueEmitter.h"

#include "Job.h"
#include "Logger.h"
#include "ThreadWorker.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include <chrono>
#include <iostream>
#include <algorithm>

using namespace Utilities;

namespace Redis
{
	RedisWorkQueueEmitter::RedisWorkQueueEmitter(const std::string& address, int port, const TLSOptions& tls_options, int db_index)
		: client_(address, port, tls_options, db_index)
		, thread_pool_(nullptr)
	{
		auto [connected, connect_error] = client_.connect();
		if (connect_error.has_value())
		{
			Logger::handle().write(LogTypes::Error, fmt::format("Cannot connect to Redis: {}", connect_error.value()));
			return;
		}

		thread_pool_ = std::make_unique<ThreadPool>("RedisQueueEmitter");
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::High }, "RedisQueueEmitterPublishWorker"));
		thread_pool_->start();
	}

	RedisWorkQueueEmitter::~RedisWorkQueueEmitter()
	{
		if (thread_pool_ != nullptr)
		{
			thread_pool_->stop();
			thread_pool_.reset();
		}

		client_.disconnect();
	}

	auto RedisWorkQueueEmitter::publish(const std::string& queue_name,
									const std::string& task,
									const std::string& message_type,
									const std::optional<long>& ttl_seconds) -> std::tuple<bool, std::optional<std::string>>
	{
		if (!client_.is_connected())
		{
			return { false, "Redis is not connected." };
		}

		if (task.empty())
		{
			return { false, "Task cannot be empty." };
		}

		if (thread_pool_ == nullptr)
		{
			return { false, "Thread pool is not created." };
		}

		std::map<std::string, std::string> fields;
		fields["task"] = task;
		fields["type"] = message_type;
		if (ttl_seconds.has_value())
		{
			fields["timestamp"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
		}

		auto [result_id, xadd_error] = client_.xadd(queue_name, fields);
		if (xadd_error.has_value())
		{
			return { false, xadd_error };
		}

		if (result_id.empty())
		{
			return { false, "Failed to publish message." };
		}

		return { true, std::nullopt };
	}

	auto RedisWorkQueueEmitter::publish_async(const std::string& queue_name,
										  const std::string& task,
										  const std::string& message_type,
										  const std::optional<long>& ttl_seconds) -> std::tuple<bool, std::optional<std::string>>
	{
		if (thread_pool_ == nullptr)
		{
			return { false, "Thread pool is not created." };
		}

		if (!thread_pool_->is_running())
		{
			return { false, "Thread pool is not running." };
		}

		return thread_pool_->push(std::make_shared<Job>(
			JobPriorities::High, [&, queue_name, task, message_type, ttl_seconds]() -> std::tuple<bool, std::optional<std::string>>
			{ return publish(queue_name, task, message_type, ttl_seconds); }, "RedisQueueEmitterPublishJob", false));
	}
}
