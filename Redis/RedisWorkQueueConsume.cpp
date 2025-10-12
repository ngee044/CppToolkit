#include "RedisWorkQueueConsume.h"

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
	RedisWorkQueueConsume::RedisWorkQueueConsume(const std::string& consumer_name, const std::string& address, int port, const TLSOptions& tls_options, int db_index)
		: client_(address, port, tls_options, db_index)
		, consumer_name_(consumer_name)
		, running_(false)
		, thread_pool_(nullptr)
	{
		auto [connected, connect_error] = client_.connect();
		if (connect_error.has_value())
		{
			Logger::handle().write(LogTypes::Error, fmt::format("Cannot connect to Redis: {}", connect_error.value()));
			return;
		}

		thread_pool_ = std::make_unique<ThreadPool>("RedisQueueWorker");
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::LongTerm }, "RedisQueueWorkerSubscribeWorker"));
		thread_pool_->start();
	}

	RedisWorkQueueConsume::~RedisWorkQueueConsume()
	{
		stop();

		if (thread_pool_ != nullptr)
		{
			thread_pool_.reset();
		}

		client_.disconnect();
	}

	auto RedisWorkQueueConsume::subscribe(
		const std::string& queue_name,
		const std::string& consumer_group,
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)> task_callback,
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)> expired_callback,
		const std::optional<long>& poll_timeout_seconds) -> std::tuple<bool, std::optional<std::string>>
	{
		if (running_)
		{
			return { false, "Worker is already running." };
		}

		if (thread_pool_ == nullptr)
		{
			return { false, "Thread pool is not created." };
		}

		queue_name_ = queue_name;
		consumer_group_ = consumer_group;
		running_ = true;

		auto handle_error_and_stop = [&](const std::optional<std::string>& error) -> std::tuple<bool, std::optional<std::string>>
		{
			running_ = false;
			if (stop_promise_.has_value())
			{
				stop_promise_->set_value();
			}
			return { false, error };
		};

		auto [group_created, group_error] = client_.xgroup_create(queue_name_, consumer_group_, "0", true);
		if (group_error.has_value())
		{
			if (group_error->find("BUSYGROUP") == std::string::npos)
			{
				Logger::handle().write(LogTypes::Error, fmt::format("cannot create consumer group: {}", group_error.value()));
				return handle_error_and_stop(group_error);
			}
		}

		return thread_pool_->push(std::make_shared<Job>(
			JobPriorities::LongTerm,
			[&, task_callback, expired_callback, poll_timeout_seconds, handle_error_and_stop]() -> std::tuple<bool, std::optional<std::string>>
			{
				std::vector<std::string> keys = { queue_name_ };
				std::vector<std::string> ids = { ">" };

				while (running_)
				{
					auto [messages, read_error] = client_.xreadgroup(consumer_group_, consumer_name_, keys, ids, 1, poll_timeout_seconds.value_or(1));
					if (read_error.has_value())
					{
						Logger::handle().write(LogTypes::Sequence, fmt::format("cannot read from stream: {}", read_error.value()));

						auto [group_created, group_error] = client_.xgroup_create(queue_name_, consumer_group_, "0", true);
						if (group_error.has_value())
						{
							if (group_error->find("BUSYGROUP") == std::string::npos)
							{
								Logger::handle().write(LogTypes::Error, fmt::format("cannot create consumer group: {}", group_error.value()));

								return handle_error_and_stop(group_error);
							}
						}

						continue;
					}

					if (messages.empty())
					{
						continue;
					}

					for (const auto& stream : messages)
					{
						for (const auto& message : stream.second)
						{
							const auto& message_id = message.first;
							const auto& fields = message.second;

							auto task_it = fields.find("task");
							auto type_it = fields.find("type");
							auto timestamp_it = fields.find("timestamp");

							auto ack_and_del = [&](const std::string& msg_id)
							{
								auto [ack_count, ack_error] = client_.xack(queue_name_, consumer_group_, { msg_id });
								if (ack_error.has_value())
								{
									Logger::handle().write(LogTypes::Error, fmt::format("Failed to acknowledge message {}: {}", msg_id, ack_error.value()));
								}

								auto [xdel_count, xdel_error] = client_.xdel(queue_name_, { msg_id });
								if (xdel_error.has_value())
								{
									Logger::handle().write(LogTypes::Error, fmt::format("Failed to delete message {}: {}", msg_id, xdel_error.value()));
								}
							};

							if (task_it != fields.end() && type_it != fields.end())
							{
								bool is_expired = false;

								if (timestamp_it != fields.end() && poll_timeout_seconds.has_value())
								{
									try
									{
										auto msg_time = std::stoull(timestamp_it->second);
										auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
										auto diff = static_cast<long long>(now) - static_cast<long long>(msg_time);
										auto timeout_ms = static_cast<long long>(poll_timeout_seconds.value()) * 1000ULL;

										if (diff > 0 && diff > timeout_ms)
										{
											is_expired = true;
											if (expired_callback)
											{
												auto [expired_success, expired_error] = expired_callback(queue_name_, task_it->second, type_it->second);
												if (expired_error.has_value())
												{
													Logger::handle().write(LogTypes::Error, fmt::format("Expired callback failed: {}", expired_error.value()));
												}
											}
											ack_and_del(message_id);
										}
									}
									catch (const std::exception& e)
									{
										Logger::handle().write(LogTypes::Exception, fmt::format("Exception in timestamp processing: {}", e.what()));
										ack_and_del(message_id);
									}
								}

								if (!is_expired)
								{
									try
									{
										auto [success, error] = task_callback(queue_name_, task_it->second, type_it->second);
										if (error.has_value())
										{
											Logger::handle().write(LogTypes::Error, fmt::format("Task callback failed: {}", error.value()));
										}
										ack_and_del(message_id);
									}
									catch (const std::exception& e)
									{
										Logger::handle().write(LogTypes::Exception, fmt::format("Exception in task processing: {}", e.what()));
										ack_and_del(message_id);
									}
								}
							}
							else
							{
								ack_and_del(message_id);
							}
						}
					}
				}

				return { true, std::nullopt };
			},
			"RedisQueueWorkerSubscribeJob", false));
	}

	auto RedisWorkQueueConsume::wait_stop() -> std::tuple<bool, std::optional<std::string>>
	{
		if (stop_promise_.has_value())
		{
			return { false, "Worker is already stopping." };
		}

		stop_promise_ = std::make_optional<std::promise<void>>();

		stop_future_ = stop_promise_->get_future();
		stop_future_.wait();

		stop_promise_.reset();

		return { true, std::nullopt };
	}

	auto RedisWorkQueueConsume::stop() -> std::tuple<bool, std::optional<std::string>>
	{
		if (thread_pool_ == nullptr)
		{
			return { false, "Thread pool is not created." };
		}

		if (!thread_pool_->is_running())
		{
			return { true, std::nullopt };
		}

		running_ = false;

		thread_pool_->stop();

		if (stop_promise_.has_value())
		{
			stop_promise_->set_value();
		}

		return { true, std::nullopt };
	}
}
