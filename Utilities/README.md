# Class Diagram

## Logger

``` mermaid
classDiagram

class LogTypes {
	<<enumeration>>
	Exception
	Error
	Information
	Debug
}

class DateTimeFormats {
	<<enumeration>>
	None
	DateTime
	Date
	Time
}

class Log {
	+ Log(const LogTypes&, const DateTimeFormats&, const string&, 
		const optional~time_point~high_resolution_clock~~&)
	+ ~Log()*

	+ to_string()* string

	# message_with_timestamp(const string&) string
	# message_without_timestamp(const string&) string

	# time_point time_point_
	# optional~time_point~high_resolution_clock~~ start_time_flag_
	# time_point~high_resolution_clock~~ end_time_flag_
	# map message_types_
	# map datetime_types_
}

class StringLog {
	+ StringLog(const LogTypes&, const DateTimeFormats&, const string&, 
		const optional~time_point~high_resolution_clock~~&)
	+ ~StringLog(void)*

	+ to_string()* string

	- string log_message_
}

class WStringLog {
	+ WStringLog(const LogTypes&, const DateTimeFormats&, const wstring&, 
		const optional~time_point~high_resolution_clock~~&)
	+ ~WStringLog(void)*

	+ to_string()* string

	- wstring log_message_
}

class U16StringLog {
	+ U16StringLog(const LogTypes&, const DateTimeFormats&, const u16string&, 
		const optional~time_point~high_resolution_clock~~&)
	+ ~U16StringLog(void)*

	+ to_string()* string

	- u16string log_message_
}

class U32StringLog {
	+ U32StringLog(const LogTypes&, const DateTimeFormats&, const u32string&, 
		const optional~time_point~high_resolution_clock~~&)
	+ ~U32StringLog(void)*

	+ to_string()* string

	- u32string log_message_
}

class Logger {
	- Logger(void)

	+ ~Logger(void)

	+ target_type(const LogTypes& type) void
	+ target_type(void) LogTypes

	+ life_cycle(const uint16_t&) void
	+ life_cycle(void) uint16_t

	+ max_file_size(const size_t&) void
	+ max_file_size(void) size_t

	+ locale_mode(const locale&) void
	+ locale_mode(void) locale

	+ log_root(const string&) void
	+ log_root(const wstring&) void
	+ log_root(const u16string&) void
	+ log_root(const u32string&) void

	+ log_root(void) string

	+ file_mode(const bool&) void
	+ file_mode(void) bool

	+ console_mode(const bool&) void
	+ console_mode(void) bool

	+ datetime_pattern(const DateTimeFormats&) void
	+ datetime_pattern(void) const DateTimeFormats

	+ start(const string&) void
	+ start(const wstring&) void
	+ start(const u16string&) void
	+ start(const u32string&) void

	+ chrono_start(void) time_point~high_resolution_clock~

	+ write(const LogTypes&, const string&, 
		const optional~time_point~high_resolution_clock~~&) void
	+ write(const LogTypes&, const wstring&, 
		const optional~time_point~high_resolution_clock~~&) void
	+ write(const LogTypes&, const u16string&, 
		const optional~time_point~high_resolution_clock~~&) void
	+ write(const LogTypes&, const u32string&, 
		const optional~time_point~high_resolution_clock~~&) void

	+ stop(void) void

	- run(void) void
	- write_file(const string&, const vector~shared_ptr~Log~~&) void
	- backup_file(const string&, const string&) void
	- check_life_cycle(const string&) string
 
	- atomic~bool~ thread_stop_
	- mutex mutex_
	- shared_ptr~thread~ thread_
	- condition_variable condition_

	- bool file_mode_
	- bool console_mode_
	- LogTypes log_types_
	- locale locale_
	- string log_name_
	- string log_root_
	- DateTimeFormats datetime_pattern_
	- atomic~size_t~ max_file_size_
	- atomic~uint16_t~ life_cycle_period_
	- vector~shared_ptr~Log~~ messages_

	+ handle(void)$ Logger&
	+ destroy(void)$ void

	- unique_ptr~Logger~$ handle_
	- once_flag$ once_
}

StringLog --|> Log
WStringLog --|> Log
U16StringLog --|> Log
U32StringLog --|> Log

Log "1" --> "1" LogTypes
Log "1" --> "1" DateTimeFormats

Logger "1" --> "1" DateTimeFormats
Logger "1" o--> "0..n" Log
Logger "1" --> "1" LogTypes
```


- Log에서 사용된 두가지 map은 다음과 같은 자료형으로 구성됩니다.
```
std::map<LogTypes, std::string> message_types_
std::map<DateTimeFormats, std::string> datetime_types_
```

## Logger 사용법
``` C++
#include <iostream>

#include "Logger.h"
#include "ArgumentParser.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

using namespace Utilities;

auto parse_arguments(ArgumentParser& arguments) -> void;

bool write_file_ = false;
bool write_console_ = true;
LogTypes type_ = LogTypes::Information;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().target_type(type_);
	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());
	
	Logger::handle().start("LogSample");

	for (int32_t i = 0; i < 10000; ++i)
	{
		Logger::handle().write(LogTypes::Information, fmt::format("Log test {}", i));
	}

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