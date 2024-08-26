#pragma once

#include "JobPriorities.h"

#include <tuple>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <future>
#include <optional>
#include <condition_variable>

namespace Thread
{
	class Job;
	class JobPool;
	class ThreadWorker : public std::enable_shared_from_this<ThreadWorker>
	{
	public:
		ThreadWorker(const std::vector<JobPriorities>& priorities, const std::string& worker_title = "ThreadWorker");
		virtual ~ThreadWorker(void);

		auto get_ptr(void) -> std::shared_ptr<ThreadWorker>;

		auto start(void) -> std::tuple<bool, std::optional<std::string>>;
		auto pause(const bool& pause) -> void;
		auto notify_one(const JobPriorities& target) -> void;
		auto stop(void) -> std::tuple<bool, std::optional<std::string>>;

		auto job_pool(std::shared_ptr<JobPool> pool) -> void;

		auto worker_title(const std::string& title) -> void;
		auto worker_title(void) -> std::string;

		auto priorities(void) -> const std::vector<JobPriorities>&;
		auto priorities(const std::vector<JobPriorities>& priorities) -> void;

	private:
		auto run(void) -> void;
		auto do_run(std::shared_ptr<Job> job) -> bool;
		auto check_condition(void) -> bool;

		auto has_job(void) -> bool;

	private:
		std::mutex mutex_;

		std::atomic_bool pause_;
		std::atomic_bool thread_stop_;

		std::promise<bool> promise_;

		std::weak_ptr<JobPool> job_pool_;
		std::string thread_worker_title_;
		std::condition_variable condition_;
		std::unique_ptr<std::thread> thread_;
		std::vector<JobPriorities> priorities_;
	};
} // namespace Thread
