// Log.cpp – clang‑friendly version (macOS & Windows)
// -----------------------------------------------
// * Wrap Visual Studio‑only pragmas in _MSC_VER checks
// * Tag intentionally unused variables with [[maybe_unused]] to silence -Wunused‑variable when -Werror is on
//
#include "Log.h"

#include "Converter.h"

#include <boost/json.hpp>

#include <chrono>
#include <unordered_map>
#include <format>

namespace Utilities
{
	Log::Log(const LogTypes& type, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: log_type_(type)
		, time_point_(std::chrono::system_clock::now())
		, start_time_flag_(time)
		, end_time_flag_(std::chrono::high_resolution_clock::now())
		, datetime_format_("{:%Y-%m-%d %H:%M:%S}.{:03}{:03}")
	{
		message_types_.insert({ LogTypes::Exception, "EXCEPTION" });
		message_types_.insert({ LogTypes::Error, "ERROR" });
		message_types_.insert({ LogTypes::Information, "INFORMATION" });
		message_types_.insert({ LogTypes::Debug, "DEBUG" });
		message_types_.insert({ LogTypes::Sequence, "SEQUENCE" });
		message_types_.insert({ LogTypes::Parameter, "PARAMETER" });
		message_types_.insert({ LogTypes::Packet, "PACKET" });
	}

	Log::~Log() = default;

	LogTypes Log::log_type() const { return log_type_; }

	std::string Log::time_stamp() const
	{
		std::chrono::duration<double, std::milli> diff = end_time_flag_ - start_time_flag_.value();
		return std::format("[{} ms]", diff.count());
	}

	std::string Log::create_json(const std::string& message) const
	{
		auto message_type = message_types_.find(log_type_);
		auto base_time = time_point_.time_since_epoch();

		const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000;
		const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000;
		const auto datetime_str = std::vformat(datetime_format_, std::make_format_args(time_point_, milliseconds, microseconds));

		// Unused in JSON but kept for parity with the original code – silence warnings.
		[[maybe_unused]] const std::string debug_line = std::format("{} {}", datetime_str, message);

		boost::json::object json_message;
		json_message["datetime"] = datetime_str;
		json_message["message"] = message;

		if (message_type != message_types_.end())
		{
			json_message["message_type"] = message_type->second;
		}

		if (start_time_flag() != std::nullopt)
		{
			json_message["time_stamp"] = time_stamp();
		}

		return boost::json::serialize(json_message);
	}

	std::string Log::create_message(const std::string& message) const
	{
		auto message_type = message_types_.find(log_type_);
		auto base_time = time_point_.time_since_epoch();

		const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000;
		const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000;
		const auto datetime_str = std::vformat(datetime_format_, std::make_format_args(time_point_, milliseconds, microseconds));

		if (message_type == message_types_.end())
		{
			std::string result = std::format("[{}]{}", datetime_str, message);
			return start_time_flag() ? std::format("{} {}", result, time_stamp()) : result;
		}

		std::string result = std::format("[{}][{}] {}", datetime_str, message_type->second, message);
		return start_time_flag() ? std::format("{} {}", result, time_stamp()) : result;
	}
	std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>> Log::start_time_flag() const { return start_time_flag_; }

	StringLog::StringLog(const LogTypes& type, const std::string& message, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: Log(type, time), log_message_(message)
	{
	}

	StringLog::~StringLog() = default;

	std::string StringLog::to_json() const { return create_json(log_message_); }
	std::string StringLog::to_string() const { return std::format("{}\n", create_message(log_message_)); }

	WStringLog::WStringLog(const LogTypes& type, const std::wstring& message, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: Log(type, time), log_message_(message)
	{
	}

	WStringLog::~WStringLog() = default;

	std::string WStringLog::to_json() const { return create_json(Converter::to_string(log_message_)); }
	std::string WStringLog::to_string() const { return std::format("{}\n", create_message(Converter::to_string(log_message_))); }

	U16StringLog::U16StringLog(const LogTypes& type,
							   const std::u16string& message,
							   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: Log(type, time), log_message_(message)
	{
	}

	U16StringLog::~U16StringLog() = default;

	std::string U16StringLog::to_json() const { return create_json(Converter::to_string(log_message_)); }
	std::string U16StringLog::to_string() const { return std::format("{}\n", create_message(Converter::to_string(log_message_))); }

	U32StringLog::U32StringLog(const LogTypes& type,
							   const std::u32string& message,
							   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: Log(type, time), log_message_(message)
	{
	}

	U32StringLog::~U32StringLog() = default;

	std::string U32StringLog::to_json() const { return create_json(Converter::to_string(log_message_)); }
	std::string U32StringLog::to_string() const { return std::format("{}\n", create_message(Converter::to_string(log_message_))); }

} // namespace Utilities
