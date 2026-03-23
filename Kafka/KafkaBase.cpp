#include "KafkaBase.h"

#include "Job.h"
#include "Logger.h"
#include "Converter.h"
#include "ThreadWorker.h"

#include <format>

#include <future>

using namespace Thread;
using namespace Utilities;

namespace Kafka
{
	KafkaBase::KafkaBase(const KafkaConfig& config)
		: config_(config)
		, thread_pool_(nullptr)
		, status_(KafkaStatus::Disconnected)
	{
	}

	KafkaBase::~KafkaBase()
	{
		if (status_ != KafkaStatus::Disconnected)
		{
			stop();
		}
	}

	auto KafkaBase::start() -> std::expected<void, std::string>
	{
		stop();

		auto created = create_thread_pool();
		if (!created)
		{
			return std::unexpected(created.error());
		}

		auto connected = connect();
		if (!connected)
		{
			Logger::handle().write(LogTypes::Error, std::format("connect start error = {}", connected.error()));
			thread_pool_.reset();
			return std::unexpected(connected.error());
		}

		auto started = thread_pool_->start();
		if (!started)
		{
			return std::unexpected(started.error());
		}

		return {};
	}

	auto KafkaBase::wait_stop() -> std::expected<void, std::string>
	{
		stop_promise_ = std::make_unique<std::promise<void>>();
		stop_future_ = stop_promise_->get_future();

		stop_future_.wait();

		return {};
	}

	auto KafkaBase::stop() -> std::expected<void, std::string>
	{
		disconnect();

		destroy_thread_pool();

		if (stop_promise_ != nullptr)
		{
			try
			{
				stop_promise_->set_value();
			}
			catch (const std::future_error&)
			{
			}
			stop_promise_.reset();
		}

		return {};
	}

	auto KafkaBase::create_thread_pool() -> std::expected<void, std::string>
	{
		auto destroyed = destroy_thread_pool();
		if (!destroyed)
		{
			return std::unexpected(destroyed.error());
		}

		try
		{
			thread_pool_ = std::make_shared<ThreadPool>();
		}
		catch (const std::bad_alloc& e)
		{
			return std::unexpected(std::format("thread pool creation failed: {}", e.what()));
		}

		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal }));

		return {};
	}

	auto KafkaBase::destroy_thread_pool() -> std::expected<void, std::string>
	{
		if (thread_pool_ == nullptr)
		{
			return {};
		}

		thread_pool_->stop();
		thread_pool_.reset();

		return {};
	}
}