#pragma once

#include "LogTypes.h"

#include <map>
#include <chrono>
#include <string>
#include <optional>

namespace Utilities
{
#pragma region Log
	class Log
	{
	public:
		Log(const LogTypes& type, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt);
		virtual ~Log(void);

		LogTypes log_type(void) const;

		virtual std::string to_json(void) const = 0;
		virtual std::string to_string(void) const = 0;

	protected:
		std::string time_stamp(void) const;
		std::string create_json(const std::string& message) const;
		std::string create_message(const std::string& message) const;
		std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>> start_time_flag(void) const;

	private:
		LogTypes log_type_;
		std::string datetime_format_;
		std::map<LogTypes, std::string> message_types_;
		std::chrono::system_clock::time_point time_point_;
		std::chrono::time_point<std::chrono::high_resolution_clock> end_time_flag_;
		std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>> start_time_flag_;
	};
#pragma endregion

#pragma region StringLog
	class StringLog : public Log
	{
	public:
		StringLog(const LogTypes& type,
				  const std::string& message,
				  const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt);
		virtual ~StringLog(void);

		std::string to_json(void) const override;
		std::string to_string(void) const override;

	private:
		std::string log_message_;
	};
#pragma endregion

#pragma region WStringLog
	class WStringLog : public Log
	{
	public:
		WStringLog(const LogTypes& type,
				   const std::wstring& message,
				   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt);
		virtual ~WStringLog(void);

		std::string to_json(void) const override;
		std::string to_string(void) const override;

	private:
		std::wstring log_message_;
	};
#pragma endregion

#pragma region U16StringLog
	class U16StringLog : public Log
	{
	public:
		U16StringLog(const LogTypes& type,
					 const std::u16string& message,
					 const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt);
		virtual ~U16StringLog(void);

		std::string to_json(void) const override;
		std::string to_string(void) const override;

	private:
		std::u16string log_message_;
	};
#pragma endregion

#pragma region U32StringLog
	class U32StringLog : public Log
	{
	public:
		U32StringLog(const LogTypes& type,
					 const std::u32string& message,
					 const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt);
		virtual ~U32StringLog(void);

		std::string to_json(void) const override;
		std::string to_string(void) const override;

	private:
		std::u32string log_message_;
	};
#pragma endregion
}
