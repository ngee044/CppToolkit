#pragma once

#include "JobPriorities.h"

#include <tuple>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace Thread
{
	class JobPool;
	class Job : public std::enable_shared_from_this<Job>
	{
	public:
		Job(const JobPriorities& priority, const std::string& title = "Job", const bool& use_time_stamp = true);
		Job(const JobPriorities& priority, const std::vector<uint8_t>& data, const std::string& title = "Job", const bool& use_time_stamp = true);
		Job(const JobPriorities& priority,
			const std::function<std::tuple<bool, std::optional<std::string>>(void)>& callback,
			const std::string& title = "Job",
			const bool& use_time_stamp = true);
		Job(const JobPriorities& priority,
			const bool& condition,
			const std::function<std::tuple<bool, std::optional<std::string>>(const bool&)>& callback,
			const std::string& title = "Job",
			const bool& use_time_stamp = true);
		Job(const JobPriorities& priority,
			const int32_t& condition,
			const std::function<std::tuple<bool, std::optional<std::string>>(const int&)>& callback,
			const std::string& title = "Job",
			const bool& use_time_stamp = true);
		Job(const JobPriorities& priority,
			const std::vector<uint8_t>& data,
			const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<uint8_t>&)>& callback,
			const std::string& title = "Job",
			const bool& use_time_stamp = true);
		virtual ~Job(void) = default;

		auto get_ptr(void) -> std::shared_ptr<Job>;

		auto job_pool(std::shared_ptr<JobPool> pool) -> void;
		auto priority(void) -> const JobPriorities;

		auto title(const std::string& new_title) -> void;
		auto title(void) -> const std::string;

		auto data(const std::vector<uint8_t>& data_array) -> void;

		auto work(void) -> std::tuple<bool, std::optional<std::string>>;

		auto destroy(void) -> void;

		auto to_json(void) -> const std::string;

	protected:
		auto save(const std::string& folder_name) -> void;
		auto load(void) -> void;

		auto get_data(void) -> std::vector<uint8_t>&;
		auto get_data(void) const -> const std::vector<uint8_t>&;
		auto get_job_pool(void) -> std::shared_ptr<JobPool>;

		virtual auto working(void) -> std::tuple<bool, std::optional<std::string>>;

	private:
		auto callback_safe_caller(const std::function<std::tuple<bool, std::optional<std::string>>()>& func) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::string title_;
		bool use_time_stamp_;
		std::vector<uint8_t> data_;
		std::string temporary_file_;
		JobPriorities priority_;
		std::weak_ptr<JobPool> job_pool_;
		std::function<std::tuple<bool, std::optional<std::string>>(void)> callback1_;
		std::function<std::tuple<bool, std::optional<std::string>>(const bool&)> callback2_;
		std::function<std::tuple<bool, std::optional<std::string>>(const int&)> callback3_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<uint8_t>&)> callback4_;
	};
} // namespace Thread