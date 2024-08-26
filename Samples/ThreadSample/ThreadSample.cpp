// ThreadSample.cpp : This file contains the 'main' function. Program execution
// begins and ends there.
//

#include <iostream>

#include "ArgumentParser.h"
#include "Converter.h"
#include "Job.h"
#include "Logger.h"
#include "ThreadPool.h"
#include "ThreadWorker.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include "boost/json.hpp"
#include "boost/json/parse.hpp"

#include <tuple>
#include <string>
#include <vector>
#include <optional>

using namespace Utilities;
using namespace Thread;

auto parse_arguments(ArgumentParser& arguments) -> void;

uint32_t max_count_ = 10;
uint16_t write_interval_ = 1000;
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().write_interval(write_interval_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("ThreadSample");

	for (int i = 0; i < max_count_; ++i)
	{
		std::unique_ptr<ThreadPool> pool = std::make_unique<ThreadPool>();
		pool->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::High }));
		pool->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal }));
		auto [started, start_error] = pool->start();
		if (!started)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("Failed to start a thread pool: {}", start_error.value()));
			pool.reset();

			continue;
		}

		auto [removed_count, remove_error] = pool->remove_workers(JobPriorities::Normal);
		if (removed_count == 0)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("Failed to remove a worker: {}", remove_error.value()));
		}

		auto [pushed_job, push_job_error] = pool->push(std::make_shared<Job>(JobPriorities::High,
																			 [i]() -> std::tuple<bool, std::optional<std::string>>
																			 {
																				 Logger::handle().write(LogTypes::Information, fmt::format("High: {}", i));
																				 return { true, std::nullopt };
																			 }));
		if (!pushed_job)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("Failed to push a job: {}", push_job_error.value()));
		}

		auto [stopped, stop_error] = pool->stop();
		if (!stopped)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("Failed to stop a thread pool: {}", stop_error.value()));
		}

		pool.reset();
	}

	Logger::handle().stop();
	Logger::destroy();

	return 0;
}

auto parse_arguments(ArgumentParser& arguments) -> void
{
	auto int_target = arguments.to_int("--write_console_log");
	if (int_target != std::nullopt)
	{
		write_console_ = (LogTypes)int_target.value();
	}

	int_target = arguments.to_int("--write_file_log");
	if (int_target != std::nullopt)
	{
		write_file_ = (LogTypes)int_target.value();
	}

	auto uint_target = arguments.to_uint("--max_count");
	if (uint_target != std::nullopt)
	{
		max_count_ = uint_target.value();
	}
}