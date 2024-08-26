#pragma once

#include "JobPriorities.h"

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Thread
{
	class Job;
	class JobPool;
	class ThreadWorker;
	class ThreadPool : public std::enable_shared_from_this<ThreadPool>
	{
	public:
		ThreadPool(const std::string& title = "ThreadPool");
		virtual ~ThreadPool(void);

		auto get_ptr(void) -> std::shared_ptr<ThreadPool>;

		auto uncompleted_jobs(const std::string& backup_folder) -> std::vector<std::vector<uint8_t>>;
		auto push(std::shared_ptr<Job> job) -> std::tuple<bool, std::optional<std::string>>;
		auto push(std::shared_ptr<ThreadWorker> worker) -> void;
		auto remove_workers(const JobPriorities& priority) -> std::tuple<size_t, std::optional<std::string>>;

		auto lock(const bool& lock_condition) -> void;
		auto lock(void) -> bool;

		auto thread_title(const std::string& title) -> void;
		auto thread_title(void) -> const std::string;

		auto start(void) -> std::tuple<bool, std::optional<std::string>>;
		auto pause(const bool& pause) -> void;
		auto stop(const bool& stop_immediately = false) -> std::tuple<bool, std::optional<std::string>>;

		auto job_pool(void) -> std::shared_ptr<JobPool>;

	protected:
		auto notify_callback(const JobPriorities& priority) -> void;

	private:
		std::atomic_bool pause_;
		std::atomic_bool working_;
		std::mutex mutex_;
		std::string thread_title_;
		std::shared_ptr<JobPool> job_pool_;
		std::vector<std::shared_ptr<ThreadWorker>> thread_workers_;
	};
} // namespace Thread