#include "ThreadPool.h"

#include "JobPool.h"
#include "Logger.h"
#include "ThreadWorker.h"

#include <format>

#include <functional>

using namespace Utilities;

namespace Thread
{
	ThreadPool::ThreadPool(const std::string& title)
		: job_pool_(std::make_shared<JobPool>(std::format("JobPool on {}", title))), working_(false), thread_title_(title), pause_(false)
	{
		job_pool_->notify_callback(std::bind(&ThreadPool::notify_callback, this, std::placeholders::_1));
	}

	ThreadPool::~ThreadPool(void)
	{
		stop(true);

		thread_workers_.clear();
		job_pool_.reset();

		Logger::handle().write(LogTypes::Debug, std::format("destroyed {}", thread_title_));
	}

	auto ThreadPool::get_ptr(void) -> std::shared_ptr<ThreadPool> { return shared_from_this(); }

	auto ThreadPool::uncompleted_jobs(const std::string& backup_folder) -> std::vector<std::vector<uint8_t>>
	{
		if (job_pool_ == nullptr)
		{
			Logger::handle().write(LogTypes::Error, "cannot get uncompleted jobs by null job_pool");

			return {};
		}

		return job_pool_->uncompleted_jobs(backup_folder);
	}

	auto ThreadPool::push(std::shared_ptr<Job> job) -> std::tuple<bool, std::optional<std::string>>
	{
		if (job_pool_ == nullptr)
		{
			return { false, "cannot push a job into null JobPool" };
		}

		return job_pool_->push(job);
	}

	auto ThreadPool::push(std::shared_ptr<ThreadWorker> worker) -> void
	{
		if (worker == nullptr)
		{
			Logger::handle().write(LogTypes::Error, "cannot push a null ThreadWorker");

			return;
		}

		auto priority = priority_string(worker->priorities());

		std::scoped_lock<std::mutex> lock(mutex_);

		thread_workers_.push_back(worker);

		worker->job_pool(job_pool_);
		worker->pause(pause_.load());
		worker->worker_title(std::format("{} ThreadWorker on {}", priority, thread_title_));

		Logger::handle().write(LogTypes::Parameter, std::format("pushed {} ThreadWorker on {}", priority, thread_title_));

		if (working_.load())
		{
			worker->start();
		}
	}

	auto ThreadPool::remove_workers(const JobPriorities& priority) -> std::tuple<size_t, std::optional<std::string>>
	{
		if (job_pool_ == nullptr)
		{
			return { 0, "cannot remove workers due to null JobPool" };
		}

		job_pool_->clear(priority);

		std::scoped_lock<std::mutex> lock(mutex_);

		auto new_end = std::remove_if(thread_workers_.begin(), thread_workers_.end(),
									  [priority](const std::shared_ptr<ThreadWorker>& worker)
									  {
										  if (worker == nullptr)
										  {
											  return false;
										  }

										  auto priorities = worker->priorities();
										  auto new_end = std::remove_if(priorities.begin(), priorities.end(),
																		[priority](const JobPriorities& target) { return target == priority; });
										  priorities.erase(new_end, priorities.end());
										  worker->priorities(priorities);

										  if (!priorities.empty())
										  {
											  return false;
										  }

										  worker->stop();

										  return true;
									  });
		std::vector<std::shared_ptr<ThreadWorker>> removed_items(new_end, thread_workers_.end());
		if (removed_items.size() == 0)
		{
			return { 0, "no worker to remove" };
		}

		thread_workers_.erase(new_end, thread_workers_.end());

		return { removed_items.size(), std::nullopt };
	}

	auto ThreadPool::lock(const bool& lock_condition) -> void
	{
		if (job_pool_ == nullptr)
		{
			Logger::handle().write(LogTypes::Error, "cannot lock null JobPool");

			return;
		}

		job_pool_->lock(lock_condition);
	}

	auto ThreadPool::lock(void) -> bool
	{
		if (job_pool_ == nullptr)
		{
			Logger::handle().write(LogTypes::Error, "cannot check null JobPool");

			return false;
		}

		return job_pool_->lock();
	}

	auto ThreadPool::thread_title(const std::string& title) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		thread_title_ = title;

		if (job_pool_ != nullptr)
		{
			job_pool_->job_pool_title(title);
		}

		for (auto& worker : thread_workers_)
		{
			if (worker == nullptr)
			{
				continue;
			}

			worker->worker_title(title);
		}
	}

	auto ThreadPool::thread_title(void) -> const std::string { return thread_title_; }

	auto ThreadPool::start(void) -> std::tuple<bool, std::optional<std::string>>
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		if (working_.load())
		{
			return { false, "already started" };
		}

		for (auto& worker : thread_workers_)
		{
			if (worker == nullptr)
			{
				continue;
			}

			auto [started, start_error] = worker->start();
			if (!started)
			{
				return { false, start_error };
			}
		}

		working_.store(true);

		return { true, std::nullopt };
	}

	auto ThreadPool::pause(const bool& pause) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		pause_.store(pause);

		for (auto& worker : thread_workers_)
		{
			if (worker == nullptr)
			{
				continue;
			}

			worker->pause(pause_.load());
		}
	}

	auto ThreadPool::stop(const bool& stop_immediately) -> std::tuple<bool, std::optional<std::string>>
	{
		{
			std::scoped_lock<std::mutex> lock(mutex_);

			if (!working_.load())
			{
				return { false, "not started" };
			}

			job_pool_->lock(true);

			if (stop_immediately)
			{
				job_pool_->clear();
			}

			for (auto& worker : thread_workers_)
			{
				if (worker == nullptr)
				{
					continue;
				}

				auto [stopped, stop_error] = worker->stop();
				if (!stopped)
				{
					return { false, stop_error };
				}
			}

			job_pool_->lock(false);
		}

		working_.store(false);

		return { true, std::nullopt };
	}

	auto ThreadPool::job_pool(void) -> std::shared_ptr<JobPool> { return job_pool_; }

	auto ThreadPool::notify_callback(const JobPriorities& priority) -> void
	{
		Logger::handle().write(LogTypes::Sequence, std::format("notify one for {} priority", priority_string(priority)));

		std::scoped_lock<std::mutex> lock(mutex_);

		for (auto& worker : thread_workers_)
		{
			if (worker == nullptr)
			{
				continue;
			}

			worker->notify_one(priority);
		}
	}
} // namespace Thread
