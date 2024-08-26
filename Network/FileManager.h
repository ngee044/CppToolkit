#pragma once

#include "ThreadPool.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include <functional>

using namespace Thread;

namespace Network
{
	struct Failures
	{
		std::vector<std::string> data;
	};

	struct Successes
	{
		std::vector<std::pair<std::string, std::string>> data;
	};

	struct ReceivedConditions
	{
		size_t count;
		Failures failures;
		Successes successes;
	};

	class FileManager
	{
	public:
		FileManager(void);
		virtual ~FileManager(void);

		auto thread_pool(std::shared_ptr<ThreadPool> thread_pool) -> void;
		auto received_files_callback(const std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<std::string>&,
																									  const std::vector<std::pair<std::string, std::string>>&)>& callback)
			-> void;

		auto start(const std::string& guid, const size_t& count) -> std::tuple<bool, std::optional<std::string>>;
		auto failure(const std::string& guid, const std::string& message) -> std::tuple<bool, std::optional<std::string>>;
		auto success(const std::string& guid, const std::string& message, const std::string& temp_file_path) -> std::tuple<bool, std::optional<std::string>>;

	protected:
		auto check_condition(const std::string& guid) -> std::tuple<bool, std::optional<std::string>>;
		auto check_condition_callback(const std::vector<uint8_t>& guid) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::mutex mutex_;
		std::shared_ptr<ThreadPool> thread_pool_;
		std::map<std::string, ReceivedConditions> file_conditions_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)> callback_;
	};
}