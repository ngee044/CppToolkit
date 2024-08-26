# Class Diagram

## ThreadPool

``` mermaid
classDiagram

class JobPriorities {
	<<enumeration>>
	Top
	High
	Normal
	Low
}

class Job {
	+ Job(const JobPriorities&, const string&)
	+ Job(const JobPriorities&, const vector~uint8_t~&, const string&)
	+ Job(const JobPriorities&, const function~bool(void)~&, const string&)
	+ Job(const JobPriorities&, const bool&, const function~bool(const bool&)~&, const string&)
	+ Job(const JobPriorities&, const vector~uint8_t~&, const function~bool(const vector~uint8_t~&)~&, const string&)
	~Job(void)*

	+ get_ptr(void) shared_ptr~Job~

	+ job_pool(shared_ptr~JobPool~ pool) void
	+ priority(void) const JobPriorities

	+ title(const string& new_title) void
	+ title(void) const string
	
	+ work(void) bool
	
	+ destroy(void) void
	
	+ to_json(void) string

	# save(const string& folder_name) void
	# load(void) void

	# get_data(void) vector~uint8_t~&
	# job_pool(void) shared_ptr~JobPool~

	# working(void)* bool

	- string title_
	- vector~uint8_t~ data_
	- string temporary_file_
	- JobPriorities priority_
	- weak_ptr~JobPool~ job_pool_
	- function~bool(void)~ callback1_
	- function~bool(const bool&)~ callback2_
	- function~bool(const vector~uint8_t~&)~ callback3_
}

class JobPool {
	+ JobPool(const string&);
	+ ~JobPool(void)*

	+ get_ptr(void) shared_ptr~JobPool~

	+ clear(void) void
	+ notify_empty(const JobPriorities&) void
	+ uncompleted_jobs(const string& ) vector~vector~uint8_t~~

	+ push(shared_ptr~Job~) bool
	+ pop(const JobPriorities&) shared_ptr~Job~

	+ notify_callback(const function~void(const JobPriorities&)~&) void

	+ priority_count(optional~JobPriorities~) size_t

	+ void lock(const bool& condition)
	+ bool lock(void)

	- mutex mutex_
	- bool lock_condition_
	- string thread_pool_title_
	- function~void(const JobPriorities&)~ notify_callback_
	- map backup_extensions_
	- map job_queues_
}

class ThreadWorker {
	+ ThreadWorker(const JobPriorities&, const vector~JobPriorities~&)
	+ ~ThreadWorker(void)*

	+ get_ptr(void) shared_ptr~ThreadWorker~

	+ start(void) void
	+ notify_one(const JobPriorities&) void
	+ stop(void) void

	+ job_pool(shared_ptr~JobPool~) void
	
	+ worker_title(const string&) void
	+ worker_title(void) string

	+ priority(void) const JobPriorities

	- run(void) void
	- do_run(shared_ptr<Job>) void
	- check_condition(void) bool

	- has_job(void) bool
	- current_job(shared_ptr~JobPool~, const JobPriorities&) shared_ptr~Job~

	- mutex mutex_
	- JobPriorities priority_
	- atomic~bool~ thread_stop_
	- weak_ptr~JobPool~ job_pool_
	- string thread_worker_title_
	- condition_variable condition_
	- shared_ptr~thread~ thread_
	- vector~JobPriorities~ sub_priorities_
}

class ThreadPool {
	+ ThreadPool(const string&)
	+ ~ThreadPool(void)*

	+ get_ptr(void) shared_ptr~ThreadPool~

	+ uncompleted_jobs(const string&) vector~vector~uint8_t~~
	+ push(shared_ptr~Job~) bool
	+ push(shared_ptr~ThreadWorker~) void
	
	+ lock(const bool&) void
	+ lock(void) bool

	+ start(void) void
	+ stop(const bool&) void

	+ job_pool(void) shared_ptr~JobPool~

	# notify_callback(const JobPriorities&) void

	- bool working_
	- mutex mutex_
	- string thread_pool_title_
	- shared_ptr~JobPool~ job_pool_
	- vector~shared_ptr~ThreadWorker~~ thread_workers_
}

Job "1" --> "1" JobPriorities
JobPool "1" o--> "0..n" Job
ThreadWorker "1" --> "1" JobPriorities
ThreadWorker "1..n" --> "1" JobPool
ThreadPool "1" o--> "0..n" ThreadWorker
ThreadPool "1" --> "1" JobPool
``` 


- JobPool에서 사용된 두가지 map은 다음과 같은 자료형으로 구성됩니다.
```
std::map<std::string, JobPriorities> backup_extensions_
std::map<JobPriorities, std::deque<std::shared_ptr<Job>>> job_queues_
```

ThreadPool 사용법
``` C++
#include <iostream>

#include "Job.h"
#include "Logger.h"
#include "Converter.h"
#include "ThreadPool.h"
#include "ThreadWorker.h"
#include "ArgumentParser.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

using namespace Utilities;
using namespace Thread;

auto parse_arguments(ArgumentParser& arguments) -> void;

bool write_file_ = false;
bool write_console_ = true;
LogTypes type_ = LogTypes::Information;

bool write_high_data(void)
{
	Logger::handle().write(LogTypes::Information, "High");

	return true;
}

bool write_normal_data(void)
{
	Logger::handle().write(LogTypes::Information, "Normal");

	return true;
}

bool write_low_data(void)
{
	Logger::handle().write(LogTypes::Information, "Low");

	return true;
}

bool write_data(const std::vector<uint8_t>& data)
{
	Logger::handle().write(LogTypes::Information, Converter::to_string(data));

	return true;
}

class WriteJob : public Job
{
public:
	WriteJob(const JobPriorities& priority, const std::vector<uint8_t>& data)
		: Job(priority, data)
	{
	}

protected:
	bool working(void) override
	{
		Logger::handle().write(LogTypes::Information, Converter::to_string(get_data()));

		return true;
	}
};

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().target_type(type_);
	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start(L"ThreadSample");

	ThreadPool pool;
	pool.push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::High }));
	pool.push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal }));
	pool.push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Low }));

	for (int32_t i = 0; i < 10000; ++i)
	{
		pool.push(std::make_shared<Job>(JobPriorities::High, &write_high_data));
		pool.push(std::make_shared<Job>(JobPriorities::Normal, &write_normal_data));
		pool.push(std::make_shared<Job>(JobPriorities::Low, &write_low_data));

		pool.push(std::make_shared<Job>(JobPriorities::High, Converter::to_array(fmt::format("high_{}", i)), &write_data));
		pool.push(std::make_shared<Job>(JobPriorities::Normal, Converter::to_array(fmt::format("normal_{}", i)), &write_data));
		pool.push(std::make_shared<Job>(JobPriorities::Low, Converter::to_array(fmt::format("low_{}", i)), &write_data));

		pool.push(std::make_shared<WriteJob>(JobPriorities::High, Converter::to_array(fmt::format("write_job_high_{}", i))));
		pool.push(std::make_shared<WriteJob>(JobPriorities::Normal, Converter::to_array(fmt::format("write_job_normal_{}", i))));
		pool.push(std::make_shared<WriteJob>(JobPriorities::Low, Converter::to_array(fmt::format("write_job_low_{}", i))));
	}

	pool.start();
	pool.stop();

	Logger::handle().stop();
	Logger::destroy();

	return 0;
}

auto parse_arguments(ArgumentParser& arguments) -> void
{
	auto int_target = arguments.to_int("--logging_level");
	if (int_target != std::nullopt)
	{
		type_ = (LogTypes)int_target.value();
	}
	
	auto bool_target = arguments.to_bool("--write_console_log");
	if (bool_target != std::nullopt && *bool_target)
	{
		write_console_ = bool_target.value();
	}

	bool_target = arguments.to_bool("--write_file_log");
	if (bool_target != std::nullopt && *bool_target)
	{
		write_file_ = bool_target.value();
	}
}
```