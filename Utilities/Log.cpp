#include "Log.h"

#include "Converter.h"

#include "fmt/chrono.h"
#include "fmt/format.h"
#include "fmt/xchar.h"

#include "boost/json.hpp"

#include <chrono>

namespace Utilities
{
#pragma region Log
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

	Log::~Log() {}

	LogTypes Log::log_type(void) const { return log_type_; }

	std::string Log::time_stamp(void) const
	{
		std::chrono::duration<double, std::milli> diff = end_time_flag_ - start_time_flag_.value();

		return fmt::format("[{} ms]", diff.count());
	}

	std::string Log::create_json(const std::string& message) const
	{
		auto message_type = message_types_.find(log_type_);
		if (message_type == message_types_.end())
		{
			auto base_time = time_point_.time_since_epoch();

			std::string result = fmt::format(datetime_format_, fmt::localtime(std::chrono::system_clock::to_time_t(time_point_)),
											 std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000,
											 std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000, message);

			if (start_time_flag() == std::nullopt)
			{
				boost::json::object json_message{ { "datetime", fmt::format(datetime_format_, fmt::localtime(std::chrono::system_clock::to_time_t(time_point_)),
																			std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000,
																			std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000) },
												  { "message", message } };

				return boost::json::serialize(json_message);
			}

			boost::json::object json_message{ { "time_stamp", time_stamp() },
											  { "datetime", fmt::format(datetime_format_, fmt::localtime(std::chrono::system_clock::to_time_t(time_point_)),
																		std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000,
																		std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000) },
											  { "message", message } };

			return boost::json::serialize(json_message);
		}

		auto base_time = time_point_.time_since_epoch();
		if (start_time_flag() == std::nullopt)
		{
			boost::json::object json_message{ { "datetime", fmt::format(datetime_format_, fmt::localtime(std::chrono::system_clock::to_time_t(time_point_)),
																		std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000,
																		std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000) },
											  { "message_type", message_type->second },
											  { "message", message } };

			return boost::json::serialize(json_message);
		}

		boost::json::object json_message{ { "time_stamp", time_stamp() },
										  { "datetime", fmt::format(datetime_format_, fmt::localtime(std::chrono::system_clock::to_time_t(time_point_)),
																	std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000,
																	std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000) },
										  { "message_type", message_type->second },
										  { "message", message } };

		return boost::json::serialize(json_message);
	}

	std::string Log::create_message(const std::string& message) const
	{
		auto message_type = message_types_.find(log_type_);
		if (message_type == message_types_.end())
		{
			std::string format = fmt::format("[{}]{}", datetime_format_, "{}");

			auto base_time = time_point_.time_since_epoch();

			std::string result = fmt::format(format, fmt::localtime(std::chrono::system_clock::to_time_t(time_point_)),
											 std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000,
											 std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000, message);

			if (start_time_flag() == std::nullopt)
			{
				return result;
			}

			return fmt::format("{} {}", result, time_stamp());
		}

		std::string format = fmt::format("[{}][{}] {}", datetime_format_, "{}", "{}");

		auto base_time = time_point_.time_since_epoch();

		std::string result = fmt::format(format, fmt::localtime(std::chrono::system_clock::to_time_t(time_point_)),
										 std::chrono::duration_cast<std::chrono::milliseconds>(base_time).count() % 1000,
										 std::chrono::duration_cast<std::chrono::microseconds>(base_time).count() % 1000, message_type->second, message);
		if (start_time_flag() == std::nullopt)
		{
			return result;
		}

		return fmt::format("{} {}", result, time_stamp());
	}

	std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>> Log::start_time_flag(void) const { return start_time_flag_; }
#pragma endregion

#pragma region StringLog
	StringLog::StringLog(const LogTypes& type, const std::string& message, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: Log(type, time), log_message_(message)
	{
	}

	StringLog::~StringLog(void) {}

	std::string StringLog::to_json(void) const { return create_json(log_message_); }

	std::string StringLog::to_string(void) const { return fmt::format("{}\n", create_message(log_message_)); }
#pragma endregion

#pragma region WStringLog
	WStringLog::WStringLog(const LogTypes& type, const std::wstring& message, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: Log(type, time), log_message_(message)
	{
	}

	WStringLog::~WStringLog(void) {}

	std::string WStringLog::to_json(void) const { return create_json(Converter::to_string(log_message_)); }

	std::string WStringLog::to_string(void) const { return fmt::format("{}\n", create_message(Converter::to_string(log_message_))); }
#pragma endregion

#pragma region U16StringLog
	U16StringLog::U16StringLog(const LogTypes& type,
							   const std::u16string& message,
							   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: Log(type, time), log_message_(message)
	{
	}

	U16StringLog::~U16StringLog(void) {}

	std::string U16StringLog::to_json(void) const { return create_json(Converter::to_string(log_message_)); }

	std::string U16StringLog::to_string(void) const { return fmt::format("{}\n", create_message(Converter::to_string(log_message_))); }
#pragma endregion

#pragma region U32StringLog
	U32StringLog::U32StringLog(const LogTypes& type,
							   const std::u32string& message,
							   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
		: Log(type, time), log_message_(message)
	{
	}

	U32StringLog::~U32StringLog(void) {}

	std::string U32StringLog::to_json(void) const { return create_json(Converter::to_string(log_message_)); }

	std::string U32StringLog::to_string(void) const { return fmt::format("{}\n", create_message(Converter::to_string(log_message_))); }
}
#pragma endregion
