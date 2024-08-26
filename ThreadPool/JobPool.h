#pragma once

#include "JobPriorities.h"

#include <map>
#include <deque>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Thread
{
	class Job;
	class JobPool : public std::enable_shared_from_this<JobPool>
	{
	public:
		JobPool(const std::string& job_pool_title = "JobPool");
		virtual ~JobPool(void);

		auto get_ptr(void) -> std::shared_ptr<JobPool>;

		auto clear(void) -> void;
		auto clear(const JobPriorities& priority) -> void;
		auto uncompleted_jobs(const std::string& backup_folder) -> std::vector<std::vector<uint8_t>>;

		auto push(std::shared_ptr<Job> job) -> std::tuple<bool, std::optional<std::string>>;
		auto pop(const std::vector<JobPriorities>& priorities) -> std::shared_ptr<Job>;

		auto notify_callback(const std::function<void(const JobPriorities&)>& callback) -> void;

		auto job_pool_title(const std::string& title) -> void;
		auto job_pool_title(void) -> const std::string;

		auto job_count(std::vector<JobPriorities>& priorities) -> const size_t;

		auto lock(const bool& condition) -> void;
		auto lock(void) -> const bool;

	private:
		std::mutex mutex_;
		std::string job_pool_title_;
		std::atomic_bool lock_condition_;
		std::function<void(const JobPriorities&)> notify_callback_;
		std::map<std::string, JobPriorities> backup_extensions_;
		std::map<JobPriorities, std::deque<std::shared_ptr<Job>>> job_queues_;
	};
} // namespace Thread