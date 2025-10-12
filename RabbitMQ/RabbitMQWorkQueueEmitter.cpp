#include "RabbitMQWorkQueueEmitter.h"

#include "Job.h"
#include "Logger.h"
#include "ThreadWorker.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include <iostream>

using namespace Utilities;

namespace RabbitMQ
{
	RabbitMQWorkQueueEmitter::RabbitMQWorkQueueEmitter(
		const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options)
		: RabbitMQBase(host, port, user_name, password, ssl_options)
		, thread_pool_(nullptr)
	{
		thread_pool_ = std::make_unique<ThreadPool>("RabbitMQQueueEmitter");
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::High }, "RabbitMQQueueEmitterPublishWorker"));
		thread_pool_->start();
	}

	RabbitMQWorkQueueEmitter::~RabbitMQWorkQueueEmitter()
	{
		if (thread_pool_ != nullptr)
		{
			thread_pool_->stop();
			thread_pool_.reset();
		}
	}

	auto RabbitMQWorkQueueEmitter::publish(const int& target_channel_id,
									   const std::string& queue_name,
									   const std::string& message,
									   const std::string& content_type,
									   const std::optional<uint32_t>& expiration_millisecond) -> std::tuple<bool, std::optional<std::string>>
	{
		return basic_publish(target_channel_id, "", queue_name, message, "", content_type, DeliveryMode::Persistent, true, expiration_millisecond);
	}

	auto RabbitMQWorkQueueEmitter::publish_async(const int& target_channel_id,
											  const std::string& queue_name,
											  const std::string& message,
											  const std::string& content_type,
											  const std::optional<uint32_t>& expiration_millisecond) -> std::tuple<bool, std::optional<std::string>>
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
			JobPriorities::High, [&, target_channel_id, queue_name, message, content_type, expiration_millisecond]() -> std::tuple<bool, std::optional<std::string>>
			{ return publish(target_channel_id, queue_name, message, content_type, expiration_millisecond); }, "RabbitMQQueueEmitterPublishJob", false));
	}
}