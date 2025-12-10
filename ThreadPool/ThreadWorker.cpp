#include "ThreadWorker.h"

#include "Converter.h"
#include "Job.h"
#include "JobPool.h"
#include "Logger.h"

#include <format>

using namespace Utilities;

namespace Thread
{
	ThreadWorker::ThreadWorker(const std::vector<JobPriorities>& priorities, const std::string& worker_title)
		: thread_(nullptr), priorities_(priorities), thread_worker_title_(worker_title), pause_(false), thread_stop_(false)
	{
	}

	ThreadWorker::~ThreadWorker(void) { stop(); }

	std::shared_ptr<ThreadWorker> ThreadWorker::get_ptr(void) { return shared_from_this(); }

	auto ThreadWorker::start(void) -> std::tuple<bool, std::optional<std::string>>
	{
		stop();

		Logger::handle().write(LogTypes::Sequence, std::format("attempt to start for {}", thread_worker_title_));

		if (priorities_.empty())
		{
			return { false, "cannot start by empty priorities" };
		}

		thread_stop_.store(false);

		std::future<bool> future = promise_.get_future();

		try
		{
			thread_ = std::make_unique<std::thread>(&ThreadWorker::run, this);
		}
		catch (const std::bad_alloc& e)
		{
			return { false, "Failed to create thread instance." };
		}

		Logger::handle().write(LogTypes::Sequence, std::format("waiting for {} to start", thread_worker_title_));
		future.wait();

		return { true, std::nullopt };
	}

	auto ThreadWorker::pause(const bool& pause) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);
		pause_.store(pause);
		condition_.notify_one();
	}

	auto ThreadWorker::notify_one(const JobPriorities& target) -> void
	{
		if (thread_ == nullptr)
		{
			return;
		}

		if (priorities_.empty())
		{
			return;
		}

		auto iter = std::find(priorities_.begin(), priorities_.end(), target);
		if (iter == priorities_.end())
		{
			return;
		}

		std::scoped_lock<std::mutex> lock(mutex_);
		condition_.notify_one();
	}

	auto ThreadWorker::stop(void) -> std::tuple<bool, std::optional<std::string>>
	{
		if (thread_ == nullptr)
		{
			return { false, "Thread is not running." };
		}

		if (thread_->joinable())
		{
			Logger::handle().write(LogTypes::Sequence, std::format("attempt to join for {} to stop", thread_worker_title_));

			{
				std::scoped_lock<std::mutex> lock(mutex_);

				thread_stop_.store(true);
				condition_.notify_one();
			}

			thread_->join();
			Logger::handle().write(LogTypes::Sequence, std::format("completed to join for {} to stop", thread_worker_title_));
		}
		thread_.reset();

		return { true, std::nullopt };
	}

	auto ThreadWorker::job_pool(std::shared_ptr<JobPool> pool) -> void { job_pool_ = pool; }

	auto ThreadWorker::worker_title(const std::string& title) -> void { thread_worker_title_ = title; }

	auto ThreadWorker::worker_title(void) -> std::string { return thread_worker_title_; }

	auto ThreadWorker::priorities(void) -> const std::vector<JobPriorities>& { return priorities_; }

	auto ThreadWorker::priorities(const std::vector<JobPriorities>& priorities) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);
		priorities_ = priorities;
	}

	auto ThreadWorker::run(void) -> void
	{
		Logger::handle().write(LogTypes::Sequence, std::format("started thread for {}", thread_worker_title_));
		promise_.set_value(true);

		while (true)
		{
			Logger::handle().write(LogTypes::Parameter, std::format("attempt to wait condition_variable for {}", thread_worker_title_));
			std::unique_lock<std::mutex> unique(mutex_);
			condition_.wait(unique,
							[this]()
							{
								auto result = check_condition();
								Logger::handle().write(LogTypes::Parameter, std::format("checked condition_variable for {}", thread_worker_title_));
								return result;
							});
			Logger::handle().write(LogTypes::Parameter, std::format("notified condition_variable for {}", thread_worker_title_));

			if (thread_stop_.load() && !has_job())
			{
				break;
			}

			if (!thread_stop_.load() && pause_.load())
			{
				Logger::handle().write(LogTypes::Sequence, std::format("paused thread for {}", thread_worker_title_));

				continue;
			}

			auto job_pool = job_pool_.lock();
			if (job_pool == nullptr)
			{
				break;
			}

			Logger::handle().write(LogTypes::Sequence, std::format("attempt to pop job for {}", thread_worker_title_));

			auto current_job = job_pool->pop(priorities_);
			unique.unlock();
			if (current_job == nullptr)
			{
				if (thread_stop_.load())
				{
					break;
				}

				Logger::handle().write(LogTypes::Sequence, std::format("there is no job for {}", thread_worker_title_));

				continue;
			}

			if (do_run(current_job))
			{
				Logger::handle().write(LogTypes::Sequence, std::format("completed work {} [ {} ] on {}", current_job->title(), priority_string(current_job->priority()),
																	   thread_worker_title_));
			}
		}

		thread_stop_.store(false);

		Logger::handle().write(LogTypes::Sequence, std::format("stopped thread for {}", thread_worker_title_));
	}

	auto ThreadWorker::do_run(std::shared_ptr<Job> job) -> bool
	{
		try
		{
			auto [result_condition, error_message] = job->work();
			if (!result_condition)
			{
				Logger::handle().write(LogTypes::Error, std::format("cannot complete {} [ {} ] on {} : {},\n{}", job->title(), priority_string(job->priority()),
																	thread_worker_title_, error_message.value(), job->to_json()));

				return false;
			}

			return true;
		}
		catch (const std::overflow_error& message)
		{
			Logger::handle().write(LogTypes::Exception, std::format("cannot complete {} [ {} ] on {} : {},\n{}", job->title(), priority_string(job->priority()),
																	thread_worker_title_, message.what(), job->to_json()));

			return false;
		}
		catch (const std::runtime_error& message)
		{
			Logger::handle().write(LogTypes::Exception, std::format("cannot complete {} [ {} ] on {} : {},\n{}", job->title(), priority_string(job->priority()),
																	thread_worker_title_, message.what(), job->to_json()));

			return false;
		}
		catch (const std::exception& message)
		{
			Logger::handle().write(LogTypes::Exception, std::format("cannot complete {} [ {} ] on {} : {},\n{}", job->title(), priority_string(job->priority()),
																	thread_worker_title_, message.what(), job->to_json()));

			return false;
		}
		catch (...)
		{
			Logger::handle().write(LogTypes::Exception, std::format("cannot complete {} [ {} ] on {} : unexpected error,\n{}", job->title(),
																	priority_string(job->priority()), thread_worker_title_, job->to_json()));

			return false;
		}
	}

	auto ThreadWorker::check_condition(void) -> bool
	{
		if (thread_stop_.load())
		{
			return true;
		}

		if (pause_.load())
		{
			return false;
		}

		return has_job();
	}

	auto ThreadWorker::has_job(void) -> bool
	{
		auto job_pool = job_pool_.lock();
		if (job_pool == nullptr)
		{
			Logger::handle().write(LogTypes::Error, std::format("cannot check job count due to empty job pool for {}", thread_worker_title_));

			return true;
		}

		return job_pool->job_count(priorities_) > 0;
	}
} // namespace Thread
