#include "KafkaBase.h"

#include "Job.h"
#include "Logger.h"
#include "Converter.h"
#include "ThreadWorker.h"

#include "fmt/format.h"

#include <future>

namespace Kafka
{
	KafkaBase::KafkaBase(const KafkaConfig& config)
		: config_(config)
		, thread_pool_(nullptr)
	{
	}

	KafkaBase::~KafkaBase()
	{
	}

	auto KafkaBase::start() -> std::tuple<bool, std::optional<std::string>>
	{
		stop();

		auto [created, create_error] = create_thread_pool();
		if (!created)
		{
			return { false, create_error };
		}

		return thread_pool_->start();
	}

	auto KafkaBase::wait_stop() -> std::tuple<bool, std::optional<std::string>>
	{
		if (stop_future_ != std::nullopt)
		{
			return { false, "already created future object" };
		}

		stop_future_ = stop_promise_.get_future();
		stop_future_.value().wait();
		stop_future_.reset();

		return { true, std::nullopt };
	}

	auto KafkaBase::stop() -> std::tuple<bool, std::optional<std::string>>
	{
		basic_disconnect();

		destroy_thread_pool();

		if (stop_future_ != std::nullopt)
		{
			stop_promise_.set_value();
		}
	}

	auto KafkaBase::basic_disconnect() -> std::tuple<bool, std::optional<std::string>>
	{
		return std::tuple<bool, std::optional<std::string>>();
	}

	auto KafkaBase::create_thread_pool() -> std::tuple<bool, std::optional<std::string>>
	{
		auto [destroyed, destroy_error] = destroy_thread_pool();
		if (!destroyed)
		{
			return { false, destroy_error };
		}

		try
		{
			thread_pool_ = std::make_shared<ThreadPool>();
		}
		catch (const std::bad_alloc& e)
		{
			return { false, fmt::format("thread pool creation failed: {}", e.what()) };
		}

		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal }));

		return { true, std::nullopt };
	}

	auto KafkaBase::destroy_thread_pool() -> std::tuple<bool, std::optional<std::string>>
	{
		if (thread_pool_ == nullptr)
		{
			return { true, std::nullopt };
		}

		auto [stopped, stop_error] = thread_pool_->stop();
		if (!stopped)
		{
			return { false, stop_error };
		}

		thread_pool_.reset();

		return { true, std::nullopt };
	}
	}
