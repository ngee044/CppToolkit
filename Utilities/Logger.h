#pragma once

#include "Log.h"

#include <map>
#include <deque>
#include <mutex>
#include <atomic>
#include <locale>
#include <memory>
#include <thread>
#include <vector>
#include <functional>
#include <condition_variable>

namespace Utilities
{
	class Logger
	{
	private:
		Logger(void);

	public:
		~Logger(void);

		auto life_cycle(const uint16_t& cycle) -> void;
		auto life_cycle(void) const -> uint16_t;

		auto max_file_size(const size_t& size) -> void;
		auto max_file_size(void) const -> size_t;

		auto max_lines(const size_t& line_count) -> void;
		auto max_lines(void) const -> size_t;

		auto locale_mode(const std::locale& mode) -> void;
		auto locale_mode(void) const -> std::locale;

		auto log_root(const std::string& root) -> void;
		auto log_root(void) const -> std::string;

		auto file_mode(const LogTypes& mode) -> void;
		auto file_mode(void) const -> LogTypes;

		auto console_mode(const LogTypes& mode) -> void;
		auto console_mode(void) const -> LogTypes;

		auto database_mode(const bool& mode) -> void;
		auto database_mode(void) const -> bool;

		auto write_interval(const uint16_t& milli_seconds) -> void;
		auto write_interval(void) const -> uint16_t;

		auto set_notification_for_database(const std::function<bool(const std::string&, const std::vector<std::string>&)>& notification) -> void;

		auto start(const std::string& name) -> void;

		auto chrono_start(void) const -> std::chrono::time_point<std::chrono::high_resolution_clock>;

		auto write(const LogTypes& type,
				   const std::string& message,
				   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt) -> void;
		auto write(const LogTypes& type,
				   const std::wstring& message,
				   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt) -> void;
		auto write(const LogTypes& type,
				   const std::u16string& message,
				   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt) -> void;
		auto write(const LogTypes& type,
				   const std::u32string& message,
				   const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time = std::nullopt) -> void;

		auto stop(void) -> void;

	private:
		auto run(void) -> void;
		auto write_log(const std::vector<std::shared_ptr<Log>>& messages) -> void;
		auto convert_log(const std::vector<std::shared_ptr<Log>>& messages) -> std::tuple<std::vector<std::string>, std::vector<std::string>, std::vector<std::string>>;
		auto write_console(const std::vector<std::string>& messages) -> void;
		auto write_database(const std::vector<std::string>& messages) -> void;
		auto write_file(const std::string& target_path, const std::vector<std::string>& messages) -> std::deque<std::string>;
		auto backup_file(const std::string& source_path, const std::string& backup_path) -> void;
		auto backup_file(const std::deque<std::string>& source_data, const std::string& backup_path) -> void;
		auto check_life_cycle(const std::string& previous_check_flag = "") -> std::string;

	private:
		std::atomic<bool> thread_stop_;
		std::mutex mutex_;
		std::unique_ptr<std::thread> thread_;
		std::condition_variable condition_;

		bool database_mode_;
		bool file_backup_mode_;
		uint16_t write_interval_;
		uint16_t max_lines_;

		LogTypes file_mode_;
		LogTypes console_mode_;
		LogTypes log_types_;

		std::locale locale_;
		std::string log_name_;
		std::string log_root_;
		std::atomic<size_t> max_file_size_;
		std::atomic<uint16_t> life_cycle_period_;
		std::vector<std::shared_ptr<Log>> messages_;

		std::function<bool(const std::string&, const std::vector<std::string>&)> notification_;

#pragma region Handle
	public:
		static Logger& handle(void);
		static void destroy(void);

	private:
		static std::unique_ptr<Logger> handle_;
		static std::once_flag once_;
#pragma endregion
	};
}
