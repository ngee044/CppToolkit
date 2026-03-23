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
	LongTerm
}

class Job {
	+ Job(JobPriorities, const string&, bool)
	+ Job(JobPriorities, const vector~uint8_t~&, const string&, bool)
	+ Job(JobPriorities, const function~expected~void, string~(void)~&, const string&, bool)
	+ Job(JobPriorities, bool, const function~expected~void, string~(bool)~&, const string&, bool)
	+ Job(JobPriorities, int32_t, const function~expected~void, string~(const int&)~&, const string&, bool)
	+ Job(JobPriorities, const vector~uint8_t~&, const function~expected~void, string~(const vector~uint8_t~&)~&, const string&, bool)
	~Job(void)*

	+ get_ptr(void) shared_ptr~Job~

	+ job_pool(shared_ptr~JobPool~ pool) void
	+ priority(void) const JobPriorities

	+ title(const string& new_title) void
	+ title(void) const string

	+ data(const vector~uint8_t~&) void

	+ work(void) expected~void, string~

	+ destroy(void) void

	+ to_json(void) const string

	# save(const string& folder_name) void
	# load(void) void

	# get_data(void) vector~uint8_t~&
	# get_data(void) const vector~uint8_t~&
	# get_job_pool(void) shared_ptr~JobPool~

	# working(void)* expected~void, string~

	- callback_safe_caller(const function~expected~void, string~()~&) expected~void, string~

	- string title_
	- bool use_time_stamp_
	- vector~uint8_t~ data_
	- string temporary_file_
	- JobPriorities priority_
	- weak_ptr~JobPool~ job_pool_
	- function~expected~void, string~(void)~ callback1_
	- function~expected~void, string~(bool)~ callback2_
	- function~expected~void, string~(const int&)~ callback3_
	- function~expected~void, string~(const vector~uint8_t~&)~ callback4_
}

class JobPool {
	+ JobPool(const string&);
	+ ~JobPool(void)*

	+ get_ptr(void) shared_ptr~JobPool~

	+ clear(void) void
	+ clear(JobPriorities) void
	+ uncompleted_jobs(const string&) vector~vector~uint8_t~~

	+ push(shared_ptr~Job~) expected~void, string~
	+ pop(const vector~JobPriorities~&) shared_ptr~Job~

	+ notify_callback(const function~void(JobPriorities)~&) void

	+ job_pool_title(const string&) void
	+ job_pool_title(void) const string

	+ job_count(vector~JobPriorities~&) const size_t

	+ lock(bool) void
	+ lock(void) const bool

	- mutex mutex_
	- string job_pool_title_
	- atomic_bool lock_condition_
	- function~void(JobPriorities)~ notify_callback_
	- map backup_extensions_
	- map job_queues_
}

class ThreadWorker {
	+ ThreadWorker(const vector~JobPriorities~&, const string&)
	+ ~ThreadWorker(void)*

	+ get_ptr(void) shared_ptr~ThreadWorker~

	+ start(void) expected~void, string~
	+ pause(bool) void
	+ notify_one(JobPriorities) void
	+ stop(void) expected~void, string~

	+ job_pool(shared_ptr~JobPool~) void

	+ worker_title(const string&) void
	+ worker_title(void) string

	+ priorities(void) const vector~JobPriorities~&
	+ priorities(const vector~JobPriorities~&) void

	- run(void) void
	- do_run(shared_ptr~Job~) bool
	- check_condition(void) bool

	- has_job(void) bool

	- mutex mutex_
	- atomic_bool pause_
	- atomic_bool thread_stop_
	- unique_ptr~promise~bool~~ promise_
	- weak_ptr~JobPool~ job_pool_
	- string thread_worker_title_
	- condition_variable condition_
	- unique_ptr~thread~ thread_
	- vector~JobPriorities~ priorities_
}

class ThreadPool {
	+ ThreadPool(const string&)
	+ ~ThreadPool(void)*

	+ get_ptr(void) shared_ptr~ThreadPool~

	+ uncompleted_jobs(const string&) vector~vector~uint8_t~~
	+ push(shared_ptr~Job~) expected~void, string~
	+ push(shared_ptr~ThreadWorker~) void
	+ remove_workers(JobPriorities) tuple~size_t, optional~string~~

	+ lock(bool) void
	+ lock(void) bool

	+ thread_title(const string&) void
	+ thread_title(void) const string

	+ start(void) expected~void, string~
	+ pause(bool) void
	+ stop(bool) expected~void, string~

	+ job_pool(void) shared_ptr~JobPool~

	# notify_callback(JobPriorities) void

	- atomic_bool pause_
	- atomic_bool working_
	- mutex mutex_
	- string thread_title_
	- shared_ptr~JobPool~ job_pool_
	- vector~shared_ptr~ThreadWorker~~ thread_workers_
}

Job "1" --> "1" JobPriorities
JobPool "1" o--> "0..n" Job
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

#include <format>

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

		pool.push(std::make_shared<Job>(JobPriorities::High, Converter::to_array(std::format("high_{}", i)), &write_data));
		pool.push(std::make_shared<Job>(JobPriorities::Normal, Converter::to_array(std::format("normal_{}", i)), &write_data));
		pool.push(std::make_shared<Job>(JobPriorities::Low, Converter::to_array(std::format("low_{}", i)), &write_data));

		pool.push(std::make_shared<WriteJob>(JobPriorities::High, Converter::to_array(std::format("write_job_high_{}", i))));
		pool.push(std::make_shared<WriteJob>(JobPriorities::Normal, Converter::to_array(std::format("write_job_normal_{}", i))));
		pool.push(std::make_shared<WriteJob>(JobPriorities::Low, Converter::to_array(std::format("write_job_low_{}", i))));
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