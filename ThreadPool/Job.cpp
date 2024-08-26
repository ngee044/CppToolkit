#include "Job.h"

#include "Converter.h"
#include "File.h"
#include "Generator.h"
#include "JobPool.h"
#include "Logger.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include "boost/json.hpp"

#include <filesystem>
#include <optional>

using namespace Utilities;

namespace Thread
{
	Job::Job(const JobPriorities& priority, const std::string& title, const bool& use_time_stamp)
		: title_(title), priority_(priority), callback1_(nullptr), callback2_(nullptr), callback3_(nullptr), use_time_stamp_(use_time_stamp)
	{
	}

	Job::Job(const JobPriorities& priority, const std::vector<uint8_t>& data, const std::string& title, const bool& use_time_stamp)
		: title_(title), priority_(priority), data_(data), callback1_(nullptr), callback2_(nullptr), callback3_(nullptr), use_time_stamp_(use_time_stamp)
	{
	}

	Job::Job(const JobPriorities& priority,
			 const std::function<std::tuple<bool, std::optional<std::string>>(void)>& callback,
			 const std::string& title,
			 const bool& use_time_stamp)
		: title_(title), priority_(priority), callback1_(callback), callback2_(nullptr), callback3_(nullptr), callback4_(nullptr), use_time_stamp_(use_time_stamp)
	{
	}

	Job::Job(const JobPriorities& priority,
			 const bool& condition,
			 const std::function<std::tuple<bool, std::optional<std::string>>(const bool&)>& callback,
			 const std::string& title,
			 const bool& use_time_stamp)
		: title_(title)
		, priority_(priority)
		, data_({ (condition ? (uint8_t)1 : (uint8_t)0) })
		, callback1_(nullptr)
		, callback2_(callback)
		, callback3_(nullptr)
		, callback4_(nullptr)
		, use_time_stamp_(use_time_stamp)
	{
	}

	Job::Job(const JobPriorities& priority,
			 const int32_t& condition,
			 const std::function<std::tuple<bool, std::optional<std::string>>(const int&)>& callback,
			 const std::string& title,
			 const bool& use_time_stamp)
		: title_(title), priority_(priority), callback1_(nullptr), callback2_(nullptr), callback3_(callback), callback4_(nullptr), use_time_stamp_(use_time_stamp)
	{
		auto size = sizeof(int32_t);

		data_.reserve(size);
		for (size_t i = 0; i < size; ++i)
		{
			data_[i] = (condition >> (i * 8)) & 0xFF;
		}
	}

	Job::Job(const JobPriorities& priority,
			 const std::vector<uint8_t>& data,
			 const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<uint8_t>&)>& callback,
			 const std::string& title,
			 const bool& use_time_stamp)
		: title_(title)
		, priority_(priority)
		, data_(data)
		, callback1_(nullptr)
		, callback2_(nullptr)
		, callback3_(nullptr)
		, callback4_(callback)
		, use_time_stamp_(use_time_stamp)
	{
	}

	auto Job::get_ptr(void) -> std::shared_ptr<Job> { return shared_from_this(); }

	auto Job::job_pool(std::shared_ptr<JobPool> pool) -> void { job_pool_ = pool; }

	auto Job::priority(void) -> const JobPriorities { return priority_; }

	auto Job::title(const std::string& new_title) -> void { title_ = new_title; }

	auto Job::title(void) -> const std::string { return title_; }

	auto Job::data(const std::vector<uint8_t>& data_array) -> void { data_ = data_array; }

	auto Job::work(void) -> std::tuple<bool, std::optional<std::string>>
	{
		auto start_time_flag = Logger::handle().chrono_start();

		load();

		std::tuple<bool, std::optional<std::string>> result;
		if (callback1_)
		{
			auto result = callback_safe_caller(callback1_);
			const auto& [result_condition, error_message] = result;
			destroy();

			if (result_condition)
			{
				Logger::handle().write(
					LogTypes::Debug, fmt::format("completed work on {} [ {} ] with callback of 'bool(void)' : {}", title_, priority_string(priority_), result_condition),
					(use_time_stamp_ ? std::optional{ start_time_flag } : std::nullopt));
			}

			return result;
		}

		if (callback2_)
		{
			uint8_t condition = data_[0];

			result = callback_safe_caller([&, condition]() -> std::tuple<bool, std::optional<std::string>> { return callback2_((condition == 1) ? true : false); });
			const auto& [result_condition, error_message] = result;
			destroy();

			if (result_condition)
			{
				Logger::handle().write(
					LogTypes::Debug, fmt::format("completed work on {} [ {} ] with callback of 'bool(bool)' : {}", title_, priority_string(priority_), result_condition),
					(use_time_stamp_ ? std::optional{ start_time_flag } : std::nullopt));
			}

			return result;
		}

		if (callback3_)
		{
			int value;
			std::memcpy(&value, data_.data(), sizeof(value));

			result = callback_safe_caller([&, value]() -> std::tuple<bool, std::optional<std::string>> { return callback3_(value); });
			const auto& [result_condition, error_message] = result;
			destroy();

			if (result_condition)
			{
				Logger::handle().write(
					LogTypes::Debug,
					fmt::format("completed work on {} [ {} ] with callback of 'bool(const int&)' : {}", title_, priority_string(priority_), result_condition),
					(use_time_stamp_ ? std::optional{ start_time_flag } : std::nullopt));
			}

			return result;
		}

		if (callback4_)
		{
			result = callback_safe_caller([&]() -> std::tuple<bool, std::optional<std::string>> { return callback4_(data_); });
			const auto& [result_condition, error_message] = result;
			destroy();

			if (result_condition)
			{
				Logger::handle().write(LogTypes::Debug,
									   fmt::format("completed work on {} [ {} ] with callback of 'bool(const std::vector<uint8_t>&)' : {}", title_,
												   priority_string(priority_), result_condition),
									   (use_time_stamp_ ? std::optional{ start_time_flag } : std::nullopt));
			}

			return result;
		}

		result = callback_safe_caller([&]() -> std::tuple<bool, std::optional<std::string>> { return working(); });
		const auto& [result_condition, error_message] = result;
		destroy();

		Logger::handle().write(LogTypes::Debug, fmt::format("completed work on {} [ {} ] : {}", title_, priority_string(priority_), result_condition),
							   (use_time_stamp_ ? std::optional{ start_time_flag } : std::nullopt));

		return result;
	}

	auto Job::destroy(void) -> void
	{
		if (temporary_file_.empty())
		{
			return;
		}

		std::error_code ec;
		std::filesystem::remove(temporary_file_, ec);
		if (ec)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("cannot destroy a file : {} => {}", temporary_file_, ec.message()));
		}

		temporary_file_.clear();
	}

	auto Job::to_json(void) -> const std::string
	{
		boost::json::object message{ { "job_title", title_ }, { "job_data", Converter::to_base64(data_) } };

		return boost::json::serialize(message);
	}

	auto Job::save(const std::string& folder_name) -> void
	{
		if (data_.empty())
		{
			return;
		}

		std::string priority = "";
		switch (priority_)
		{
		case JobPriorities::Low:
			priority = "low";
			break;
		case JobPriorities::Normal:
			priority = "normal";
			break;
		case JobPriorities::High:
			priority = "high";
			break;
		case JobPriorities::Top:
			priority = "top";
			break;
		default:
			return;
		}

		auto temp_filename = fmt::format("{}.{}", Generator::guid(), priority);
		temporary_file_ = (std::filesystem::temp_directory_path() / folder_name / temp_filename).string();

		File target;
		target.open(temporary_file_, std::ios::out | std::ios::binary | std::ios::trunc);
		target.write_bytes(data_);
		target.close();

		data_.clear();
	}

	auto Job::load(void) -> void
	{
		if (temporary_file_.empty())
		{
			return;
		}

		File source;
		source.open(temporary_file_, std::ios::in | std::ios::binary);
		const auto [source_data, message] = source.read_bytes();
		source.close();

		if (source_data == std::nullopt)
		{
			data_.clear();
			return;
		}

		data_ = source_data.value();
	}

	auto Job::get_data(void) -> std::vector<uint8_t>& { return data_; }

	auto Job::get_data(void) const -> const std::vector<uint8_t>& { return data_; }

	auto Job::get_job_pool(void) -> std::shared_ptr<JobPool> { return job_pool_.lock(); }

	auto Job::working(void) -> std::tuple<bool, std::optional<std::string>>
	{
		return { false, fmt::format("cannot complete {}::working because it does not implemented", title_) };
	}

	auto Job::callback_safe_caller(const std::function<std::tuple<bool, std::optional<std::string>>()>& func) -> std::tuple<bool, std::optional<std::string>>
	{
		try
		{
			return func();
		}
		catch (const std::overflow_error& message)
		{
			return { false, fmt::format("cannot complete {} [ {} ] on {} : {},\n{}", title_, priority_string(priority_), "Job", message.what(), to_json()) };
		}
		catch (const std::runtime_error& message)
		{
			return { false, fmt::format("cannot complete {} [ {} ] on {} : {},\n{}", title_, priority_string(priority_), "Job", message.what(), to_json()) };
		}
		catch (const std::exception& message)
		{
			return { false, fmt::format("cannot complete {} [ {} ] on {} : {},\n{}", title_, priority_string(priority_), "Job", message.what(), to_json()) };
		}
		catch (...)
		{
			return { false, fmt::format("cannot complete {} [ {} ] on {} : unexpected error,\n{}", title_, priority_string(priority_), "Job", to_json()) };
		}
	}
} // namespace Thread
