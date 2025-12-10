#include "Logger.h"

#include "File.h"
#include "Converter.h"

#include <tuple>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <filesystem>
#include <codecvt>
#include <fstream>
#include <format>

namespace Utilities
{
	Logger::Logger(void)
		: log_name_("log")
		, max_file_size_(2097152)
		, life_cycle_period_(7)
		, thread_(nullptr)
		, file_mode_(LogTypes::Information)
		, console_mode_(LogTypes::Error)
		, database_mode_(false)
		, write_interval_(0)
		, max_lines_(10000)
		, file_backup_mode_(false)
	{
		locale_mode(std::locale(""));
		log_root_ = std::format("{}/", std::filesystem::current_path().string());

		log_types_ = std::max(file_mode_, console_mode_);
	}

	Logger::~Logger(void) { stop(); }

	void Logger::life_cycle(const uint16_t& cycle) { life_cycle_period_ = cycle; }

	uint16_t Logger::life_cycle(void) const { return life_cycle_period_; }

	void Logger::max_file_size(const size_t& size) { max_file_size_.store(size); }

	size_t Logger::max_file_size(void) const { return max_file_size_.load(); }

	void Logger::max_lines(const size_t& line_count) { max_lines_ = line_count; }

	size_t Logger::max_lines(void) const { return max_lines_; }

	void Logger::locale_mode(const std::locale& mode)
	{
		locale_ = mode;

		std::wcout.imbue(locale_);
	}

	std::locale Logger::locale_mode(void) const { return locale_; }

	void Logger::log_root(const std::string& root)
	{
		log_root_ = root;

		if (log_root_.empty())
		{
			log_root_ = std::format("{}/", std::filesystem::current_path().string());
		}
	}

	std::string Logger::log_root(void) const { return log_root_; }

	void Logger::file_mode(const LogTypes& mode)
	{
		file_mode_ = mode;

		log_types_ = std::max(file_mode_, console_mode_);
	}

	LogTypes Logger::file_mode(void) const { return file_mode_; }

	void Logger::console_mode(const LogTypes& mode)
	{
		console_mode_ = mode;

		log_types_ = std::max(file_mode_, console_mode_);
	}

	LogTypes Logger::console_mode(void) const { return console_mode_; }

	void Logger::database_mode(const bool& mode) { database_mode_ = mode; }

	bool Logger::database_mode(void) const { return database_mode_; }

	void Logger::write_interval(const uint16_t& milli_seconds) { write_interval_ = milli_seconds; }

	uint16_t Logger::write_interval(void) const { return write_interval_; }

	void Logger::set_notification_for_database(const std::function<bool(const std::string&, const std::vector<std::string>&)>& notification)
	{
		notification_ = notification;
	}

	void Logger::start(const std::string& name)
	{
		stop();

		log_name_ = name;

		thread_stop_.store(false);

		thread_ = std::make_unique<std::thread>(&Logger::run, this);
	}

	std::chrono::time_point<std::chrono::high_resolution_clock> Logger::chrono_start(void) const { return std::chrono::high_resolution_clock::now(); }

	void Logger::write(const LogTypes& type, const std::string& message, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
	{
		std::scoped_lock<std::mutex> guard(mutex_);

		if (thread_stop_.load())
		{
			return;
		}

		messages_.push_back(std::dynamic_pointer_cast<Log>(std::make_shared<StringLog>(type, message, time)));

		if (type > log_types_)
		{
			return;
		}

		if (write_interval_ == 0)
		{
			condition_.notify_one();
		}
	}

	void Logger::write(const LogTypes& type, const std::wstring& message, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
	{
		std::scoped_lock<std::mutex> guard(mutex_);

		if (thread_stop_.load())
		{
			return;
		}

		messages_.push_back(std::dynamic_pointer_cast<Log>(std::make_shared<WStringLog>(type, message, time)));

		if (type > log_types_)
		{
			return;
		}

		if (write_interval_ == 0)
		{
			condition_.notify_one();
		}
	}

	void Logger::write(const LogTypes& type, const std::u16string& message, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
	{
		std::scoped_lock<std::mutex> guard(mutex_);

		if (thread_stop_.load())
		{
			return;
		}

		messages_.push_back(std::dynamic_pointer_cast<Log>(std::make_shared<U16StringLog>(type, message, time)));

		if (type > log_types_)
		{
			return;
		}

		if (write_interval_ == 0)
		{
			condition_.notify_one();
		}
	}

	void Logger::write(const LogTypes& type, const std::u32string& message, const std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>>& time)
	{
		std::scoped_lock<std::mutex> guard(mutex_);

		if (thread_stop_.load())
		{
			return;
		}

		messages_.push_back(std::dynamic_pointer_cast<Log>(std::make_shared<U32StringLog>(type, message, time)));

		if (type > log_types_)
		{
			return;
		}

		if (write_interval_ == 0)
		{
			condition_.notify_one();
		}
	}

	void Logger::stop(void)
	{
		if (thread_ == nullptr)
		{
			return;
		}

		thread_stop_.store(true);
		condition_.notify_one();

		if (thread_->joinable())
		{
			thread_->join();
		}
		thread_.reset();

		thread_stop_.store(false);
	}

	void Logger::run(void)
	{
		check_life_cycle();

		write_log({ std::dynamic_pointer_cast<Log>(std::make_shared<StringLog>(LogTypes::None, "[START]")) });
		while (!thread_stop_.load() || !messages_.empty())
		{
			std::unique_lock<std::mutex> unique(mutex_);

			if (write_interval_ == 0)
			{
				condition_.wait(unique, [this]() { return thread_stop_.load() || !messages_.empty(); });
			}
			else
			{
				condition_.wait_for(unique, std::chrono::milliseconds(write_interval_), [this]() { return thread_stop_.load() || !messages_.empty(); });
			}

			std::vector<std::shared_ptr<Log>> messages;
			messages.swap(messages_);
			unique.unlock();

			if (messages.empty())
			{
				continue;
			}

			write_log(messages);
		}
		write_log({ std::dynamic_pointer_cast<Log>(std::make_shared<StringLog>(LogTypes::None, "[STOP]")) });

		check_life_cycle();
	}

	auto Logger::write_log(const std::vector<std::shared_ptr<Log>>& messages) -> void
	{
		const auto now = std::chrono::system_clock::now();
		std::string file_name = std::format("{}{}_{:%Y-%m-%d}", log_root_, log_name_, now);

		const auto [console_messages, file_messages, database_messages] = convert_log(messages);

		write_console(console_messages);
		write_database(database_messages);
		auto removed_lines = write_file(std::format("{}.log", file_name), file_messages);

		if (!file_backup_mode_)
		{
			return;
		}

		if (max_lines_ > 0)
		{
			backup_file(removed_lines, std::format("{}.backup", file_name));
			return;
		}

		backup_file(std::format("{}.log", file_name), std::format("{}.backup", file_name));
	}

	std::tuple<std::vector<std::string>, std::vector<std::string>, std::vector<std::string>> Logger::convert_log(const std::vector<std::shared_ptr<Log>>& messages)
	{
		std::string temp_json;
		std::string temp_string;
		std::vector<std::string> database_messages;
		std::vector<std::string> console_messages;
		std::vector<std::string> file_messages;
		for (auto& message : messages)
		{
			temp_json = message->to_json();
			temp_string = message->to_string();

			database_messages.push_back(temp_json);

			if (message->log_type() <= console_mode_)
			{
				console_messages.push_back(temp_string);
			}

			if (message->log_type() <= file_mode_)
			{
				file_messages.push_back(temp_string);
			}
		}

		return { console_messages, file_messages, database_messages };
	}

	void Logger::write_console(const std::vector<std::string>& messages)
	{
		if (console_mode_ == LogTypes::None)
		{
			return;
		}

		if (messages.empty())
		{
			return;
		}

		for (const auto& msg : messages)
		{
			std::cout << msg;
		}
	}

	void Logger::write_database(const std::vector<std::string>& messages)
	{
		if (!database_mode_)
		{
			return;
		}

		if (notification_ == nullptr)
		{
			return;
		}

		if (messages.empty())
		{
			return;
		}

		notification_(log_name_, messages);
	}

	std::deque<std::string> Logger::write_file(const std::string& target_path, const std::vector<std::string>& messages)
	{
		std::deque<std::string> removed_lines;

		if (file_mode_ == LogTypes::None)
		{
			return removed_lines;
		}

		if (messages.empty())
		{
			return removed_lines;
		}

		if (max_lines_ == 0)
		{
			File file;
			const auto [condition, message] = file.open(target_path, std::ios::out | std::ios::app, locale_);
			if (condition)
			{
				file.write_lines(messages);
				file.close();
			}

			return removed_lines;
		}

		File file;
		std::deque<std::string> read_lines;
		const auto [condition, open_message] = file.open(target_path, std::ios::in, locale_);
		if (condition)
		{
			const auto [file_lines, read_message] = file.read_lines();
			file.close();

			if (file_lines != std::nullopt)
			{
				read_lines = file_lines.value();
			}
		}

		for (const auto& message : messages)
		{
			if (read_lines.size() >= max_lines_)
			{
				removed_lines.push_back(read_lines.front());
				read_lines.pop_front();
			}
			read_lines.push_back(message);
		}

		file.open(target_path, std::ios::out | std::ios::trunc, locale_);
		file.write_lines(read_lines);
		file.close();

		return removed_lines;
	}

	void Logger::backup_file(const std::string& source_path, const std::string& backup_path)
	{
		if (file_mode_ == LogTypes::None)
		{
			return;
		}

		if (!std::filesystem::exists(source_path))
		{
			return;
		}

		if (std::filesystem::file_size(source_path) < max_file_size_.load())
		{
			return;
		}

		File source;
		source.open(source_path, std::ios::in | std::ios::binary, locale_);
		auto [source_data, message] = source.read_bytes();
		if (source_data == std::nullopt)
		{
			return;
		}
		source.close();

		File backup;
		backup.open(backup_path, std::ios::out | std::ios::binary | std::ios::app, locale_);
		backup.write_bytes(source_data.value());
		backup.close();

		std::filesystem::resize_file(source_path, 0);
	}

	void Logger::backup_file(const std::deque<std::string>& source_data, const std::string& backup_path)
	{
		if (file_mode_ == LogTypes::None)
		{
			return;
		}

		if (source_data.empty())
		{
			return;
		}

		File backup;
		backup.open(backup_path, std::ios::out | std::ios::app, locale_);
		backup.write_lines(source_data, true);
		backup.close();
	}

	std::string Logger::check_life_cycle(const std::string& previous_check_flag)
	{
		const auto now = std::chrono::system_clock::now();
		std::string current = std::format("{:%Y-%m-%d}", now);
		if (current == previous_check_flag)
		{
			return previous_check_flag;
		}

		std::filesystem::path targetDir(log_root_);
		if (std::filesystem::exists(log_root_) != true)
		{
			return previous_check_flag;
		}

		std::string extension;
		std::chrono::system_clock::time_point file_time;
		std::filesystem::directory_iterator iterator(targetDir), endItr;

		for (; iterator != endItr; ++iterator)
		{
			if (std::filesystem::is_regular_file(iterator->path()) != true)
			{
				continue;
			}

			if (iterator->path().extension().string().compare(".log") != 0)
			{
				continue;
			}

			auto hours = std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now().time_since_epoch()
																		- std::filesystem::last_write_time(iterator->path().string()).time_since_epoch());

			if ((hours.count() / 24) > life_cycle_period_.load())
			{
				std::filesystem::remove(iterator->path());
			}
		}

		return std::format("{:%Y-%m-%d}", std::chrono::system_clock::now());
		return std::format("{:%Y-%m-%d}", std::chrono::system_clock::now());
	}

#pragma region Handle
	std::unique_ptr<Logger> Logger::handle_;
	std::once_flag Logger::once_;

	Logger& Logger::handle(void)
	{
		std::call_once(once_, []() { handle_.reset(new Logger); });

		return *handle_.get();
	}

	void Logger::destroy(void) { handle_.reset(); }
#pragma endregion
}
