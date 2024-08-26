#include "FileManager.h"

#include "Job.h"
#include "Logger.h"
#include "Converter.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

using namespace Utilities;

namespace Network
{
	FileManager::FileManager(void) : thread_pool_(nullptr), callback_(nullptr) {}

	FileManager::~FileManager(void) {}

	auto FileManager::thread_pool(std::shared_ptr<ThreadPool> pool) -> void { thread_pool_ = pool; }

	auto FileManager::received_files_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>&
			callback) -> void
	{
		callback_ = callback;
	}

	auto FileManager::start(const std::string& guid, const size_t& count) -> std::tuple<bool, std::optional<std::string>>
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		auto target = file_conditions_.find(guid);
		if (target != file_conditions_.end())
		{
			return { false, fmt::format("cannot make file manager due to same guid: {}", guid) };
		}

		file_conditions_.insert({ guid, { count, {}, {} } });

		return { true, std::nullopt };
	}

	auto FileManager::failure(const std::string& guid, const std::string& message) -> std::tuple<bool, std::optional<std::string>>
	{
		std::unique_lock<std::mutex> lock(mutex_);

		auto target = file_conditions_.find(guid);
		if (target == file_conditions_.end())
		{
			return { false, fmt::format("cannot find same guid on file manager: {}", guid) };
		}

		target->second.failures.data.push_back(message);
		lock.unlock();

		return check_condition(guid);
	}

	auto FileManager::success(const std::string& guid, const std::string& message, const std::string& temp_file_path) -> std::tuple<bool, std::optional<std::string>>
	{
		std::unique_lock<std::mutex> lock(mutex_);

		auto target = file_conditions_.find(guid);
		if (target == file_conditions_.end())
		{
			return { false, fmt::format("cannot find same guid on file manager: {}", guid) };
		}

		target->second.successes.data.push_back({ message, temp_file_path });
		lock.unlock();

		return check_condition(guid);
	}

	auto FileManager::check_condition(const std::string& guid) -> std::tuple<bool, std::optional<std::string>>
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		auto target = file_conditions_.find(guid);
		if (target == file_conditions_.end())
		{
			return { false, fmt::format("cannot find same guid on file manager: {}", guid) };
		}

		size_t source_count = target->second.count;

		size_t target_count = target->second.failures.data.size();
		target_count += target->second.successes.data.size();

		if (source_count != target_count)
		{
			return { true, std::nullopt };
		}

		return thread_pool_->push(std::make_shared<Job>(JobPriorities::Low, Converter::to_array(guid),
														std::bind(&FileManager::check_condition_callback, this, std::placeholders::_1), "check_condition_callback"));
	}

	auto FileManager::check_condition_callback(const std::vector<uint8_t>& guid) -> std::tuple<bool, std::optional<std::string>>
	{
		std::unique_lock<std::mutex> lock(mutex_);

		auto key = Converter::to_string(guid);

		auto target = file_conditions_.find(key);
		if (target == file_conditions_.end())
		{
			return { false, fmt::format("cannot find same guid on file manager: {}", key) };
		}

		if (callback_ == nullptr)
		{
			file_conditions_.erase(target);

			return { true, std::nullopt };
		}

		auto failure_array = target->second.failures.data;
		auto success_array = target->second.successes.data;

		file_conditions_.erase(target);
		lock.unlock();

		return callback_(failure_array, success_array);
	}
}