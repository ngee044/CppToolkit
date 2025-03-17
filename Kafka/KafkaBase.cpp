#include "KafkaBase.h"

#include "Job.h"
#include "Logger.h"
#include "Converter.h"
#include "ThreadWorker.h"

#include "fmt/format.h"

#include <future>

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

	auto KafkaBase::start() -> std::tuple<bool, std::optional<std::string>>
	{
		stop();

		auto [created, create_error] = create_thread_pool();
		if (!created)
		{
			return { false, create_error };
		}

		auto [success, error_message] = connect();
		if (!success)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("connect start error = {}", error_message.value()));
			return { false, error_message };
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
		disconnect();

		destroy_thread_pool();

		if (stop_future_ != std::nullopt)
		{
			stop_promise_.set_value();
		}

		return { true, std::nullopt };
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
