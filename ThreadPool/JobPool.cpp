#include "JobPool.h"

#include "Converter.h"
#include "File.h"
#include "Job.h"
#include "Logger.h"

#include <format>

#include <filesystem>

using namespace Utilities;

namespace Thread
{
	// Backpressure thresholds per priority (tunable)
	static constexpr size_t MAX_QUEUE_LOW = 4096;
	// static constexpr size_t MAX_QUEUE_NORMAL = 8192; // reserved for future use

	JobPool::JobPool(const std::string& title) : lock_condition_(false), job_pool_title_(title)
	{
		backup_extensions_.insert({ ".top", JobPriorities::Top });
		backup_extensions_.insert({ ".high", JobPriorities::High });
		backup_extensions_.insert({ ".normal", JobPriorities::Normal });
		backup_extensions_.insert({ ".low", JobPriorities::Low });
	}

	JobPool::~JobPool(void)
	{
		lock_condition_.store(true);

		clear();
	}

	auto JobPool::get_ptr(void) -> std::shared_ptr<JobPool> { return shared_from_this(); }

	auto JobPool::clear(void) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		for (auto job : job_queues_)
		{
			for (auto target : job.second)
			{
				target->job_pool(nullptr);
				target->destroy();
			}
		}

		job_queues_.clear();
	}

	auto JobPool::clear(const JobPriorities& priority) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		auto iterator = job_queues_.find(priority);
		if (iterator == job_queues_.end())
		{
			return;
		}

		iterator->second.clear();
		job_queues_.erase(iterator);
	}

	auto JobPool::uncompleted_jobs(const std::string& backup_folder) -> std::vector<std::vector<uint8_t>>
	{
		if (!std::filesystem::is_directory(backup_folder))
		{
			Logger::handle().write(LogTypes::Debug, std::format("cannot get uncompleted jobs by unknown folder path : {}", backup_folder));
			return {};
		}

		std::vector<std::vector<uint8_t>> result;
		std::chrono::system_clock::time_point file_time;
		std::filesystem::directory_iterator iterator(backup_folder), endItr;

		for (; iterator != endItr; ++iterator)
		{
			if (std::filesystem::is_regular_file(iterator->path()) != true)
			{
				continue;
			}

			auto iter = backup_extensions_.find(iterator->path().extension().string());
			if (iter == backup_extensions_.end())
			{
				continue;
			}

			File source;
			source.open(iterator->path().string(), std::ios::in | std::ios::binary, std::locale(""));
			const auto [source_data, message] = source.read_bytes();
			source.close();

			std::error_code ec;
			std::filesystem::remove(iterator->path().string(), ec);
			if (ec)
			{
				Logger::handle().write(LogTypes::Error, std::format("cannot destroy a file : {} => {}", iterator->path().string(), ec.message()));
			}

			if (source_data == std::nullopt)
			{
				continue;
			}

			result.push_back(source_data.value());
		}

		return result;
	}

	auto JobPool::push(std::shared_ptr<Job> job) -> std::tuple<bool, std::optional<std::string>>
	{
		if (job == nullptr)
		{
			return { false, "cannot push empty job" };
		}

		if (lock_condition_.load())
		{
			return { false, "the system is locked and new tasks cannot be created" };
		}

		std::unique_lock<std::mutex> lock(mutex_);

		JobPriorities priority = job->priority();
		job->job_pool(get_ptr());

		// Priority-based backpressure: drop low-priority jobs when the queue is full
		auto iter = job_queues_.find(priority);
		size_t qsize = (iter != job_queues_.end()) ? iter->second.size() : 0;
		if (priority == JobPriorities::Low && qsize >= MAX_QUEUE_LOW)
		{
			lock.unlock();
			return { false, "backpressure: low-priority queue is full" };
		}

		if (iter != job_queues_.end())
		{
			iter->second.push_back(job);
		}
		else
		{
			std::deque<std::shared_ptr<Job>> queue;
			queue.push_back(job);

			job_queues_.insert({ priority, queue });
		}

		Logger::handle().write(LogTypes::Parameter, std::format("contained job : {} [ {} ]", job->title(), priority_string(job->priority())));
		lock.unlock();

		if (notify_callback_)
		{
			notify_callback_(priority);
		}

		return { true, std::nullopt };
	}

	auto JobPool::pop(const std::vector<JobPriorities>& priorities) -> std::shared_ptr<Job>
	{
		if (priorities.empty())
		{
			Logger::handle().write(LogTypes::Error, "cannot pop a job by empty priorities");

			return nullptr;
		}

		std::scoped_lock<std::mutex> lock(mutex_);

		for (const auto& priority : priorities)
		{
			auto iter = job_queues_.find(priority);
			if (iter == job_queues_.end() || iter->second.empty())
			{
				continue;
			}

			std::shared_ptr<Job> result = iter->second.front();
			if (result == nullptr)
			{
				continue;
			}

			iter->second.pop_front();

			Logger::handle().write(LogTypes::Parameter,
								   std::format("consumed job : {} [ {} ] for {}", result->title(), priority_string(result->priority()), priority_string(priorities)));

			return result;
		}

		Logger::handle().write(LogTypes::Sequence, std::format("there is no pop job by priorities : {}", priority_string(priorities)));

		return nullptr;
	}

	auto JobPool::notify_callback(const std::function<void(const JobPriorities&)>& callback) -> void { notify_callback_ = callback; }

	auto JobPool::job_pool_title(const std::string& title) -> void { job_pool_title_ = title; }

	const std::string JobPool::job_pool_title(void) { return job_pool_title_; }

	auto JobPool::job_count(std::vector<JobPriorities>& priorities) -> const size_t
	{
		if (priorities.empty())
		{
			std::scoped_lock<std::mutex> lock(mutex_);

			size_t count = 0;

			for (auto& current : job_queues_)
			{
				count += current.second.size();
			}

			return count;
		}

		size_t count = 0;
		for (auto& priority : priorities)
		{
			std::scoped_lock<std::mutex> lock(mutex_);

			auto iter = job_queues_.find(priority);
			if (iter == job_queues_.end())
			{
				continue;
			}

			count += iter->second.size();
		}

		return count;
	}

	auto JobPool::lock(const bool& condition) -> void { lock_condition_.store(condition); }

	auto JobPool::lock(void) -> const bool { return lock_condition_.load(); }
} // namespace Thread
