# Class Diagram

## Logger

``` mermaid
classDiagram

class LogTypes {
	<<enumeration>>
	None
	Exception
	Error
	Warning
	Information
	Debug
	Sequence
	Parameter
	Packet
}

class Log {
	+ Log(const LogTypes&, const optional~time_point~high_resolution_clock~~&)
	+ ~Log(void)*

	+ log_type(void) LogTypes
	+ to_json(void)* string
	+ to_string(void)* string

	# time_stamp(void) string
	# create_json(const string&) string
	# create_message(const string&) string
	# start_time_flag(void) optional~time_point~high_resolution_clock~~

	- LogTypes log_type_
	- string datetime_format_
	- map message_types_
	- time_point time_point_
	- time_point~high_resolution_clock~ end_time_flag_
	- optional~time_point~high_resolution_clock~~ start_time_flag_
}

class StringLog {
	+ StringLog(const LogTypes&, const string&,
		const optional~time_point~high_resolution_clock~~&)
	+ ~StringLog(void)*

	+ to_json(void)* string
	+ to_string(void)* string

	- string log_message_
}

class WStringLog {
	+ WStringLog(const LogTypes&, const wstring&,
		const optional~time_point~high_resolution_clock~~&)
	+ ~WStringLog(void)*

	+ to_json(void)* string
	+ to_string(void)* string

	- wstring log_message_
}

class U16StringLog {
	+ U16StringLog(const LogTypes&, const u16string&,
		const optional~time_point~high_resolution_clock~~&)
	+ ~U16StringLog(void)*

	+ to_json(void)* string
	+ to_string(void)* string

	- u16string log_message_
}

class U32StringLog {
	+ U32StringLog(const LogTypes&, const u32string&,
		const optional~time_point~high_resolution_clock~~&)
	+ ~U32StringLog(void)*

	+ to_json(void)* string
	+ to_string(void)* string

	- u32string log_message_
}

class Logger {
	- Logger(void)

	+ ~Logger(void)

	+ life_cycle(uint16_t) void
	+ life_cycle(void) uint16_t

	+ max_file_size(size_t) void
	+ max_file_size(void) size_t

	+ max_lines(size_t) void
	+ max_lines(void) size_t

	+ locale_mode(const locale&) void
	+ locale_mode(void) locale

	+ log_root(const string&) void
	+ log_root(void) string

	+ file_mode(const LogTypes&) void
	+ file_mode(void) LogTypes

	+ console_mode(const LogTypes&) void
	+ console_mode(void) LogTypes

	+ database_mode(bool) void
	+ database_mode(void) bool

	+ write_interval(uint16_t) void
	+ write_interval(void) uint16_t

	+ set_notification_for_database(const function~bool(const string&, const vector~string~&)~&) void

	+ start(const string&) void

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
	- write_log(const vector~shared_ptr~Log~~&) void
	- convert_log(const vector~shared_ptr~Log~~&) tuple~vector~string~, vector~string~, vector~string~~
	- write_console(const vector~string~&) void
	- write_database(const vector~string~&) void
	- write_file(const string&, const vector~string~&) deque~string~
	- backup_file(const string&, const string&) void
	- backup_file(const deque~string~&, const string&) void
	- check_life_cycle(const string&) string

	- atomic~bool~ thread_stop_
	- mutex mutex_
	- unique_ptr~thread~ thread_
	- condition_variable condition_

	- bool database_mode_
	- bool file_backup_mode_
	- uint16_t write_interval_
	- uint16_t max_lines_

	- LogTypes file_mode_
	- LogTypes console_mode_
	- LogTypes log_types_

	- locale locale_
	- string log_name_
	- string log_root_
	- atomic~size_t~ max_file_size_
	- atomic~uint16_t~ life_cycle_period_
	- vector~shared_ptr~Log~~ messages_

	- function~bool(const string&, const vector~string~&)~ notification_

	+ handle(void)$ Logger&
	+ destroy(void)$ void

	- unique_ptr~Logger~$ handle_
	- once_flag$ once_
}

class File {
	+ File(void)
	+ File(const string&, const openmode&)
	+ File(const string&, const openmode&, const locale&)
	+ ~File(void)

	+ open(const string&, const openmode&) expected~void, string~
	+ open(const string&, const openmode&, const locale&) expected~void, string~

	+ write_bytes(const uint8_t*, size_t) expected~void, string~
	+ write_bytes(const deque~uint8_t~&) expected~void, string~
	+ write_bytes(const vector~uint8_t~&) expected~void, string~
	+ write_lines(const deque~string~&, bool) expected~void, string~
	+ write_lines(const vector~string~&, bool) expected~void, string~
	+ read_bytes(void) tuple~optional~vector~uint8_t~~, optional~string~~
	+ read_bytes(size_t, size_t) tuple~optional~vector~uint8_t~~, optional~string~~
	+ read_lines(bool) tuple~optional~deque~string~~, optional~string~~
	+ close(void) void

	+ compression(const string&, uint16_t)$ expected~void, string~
	+ decompression(const string&, uint16_t)$ expected~void, string~

	- fstream stream_
	- string file_path_
	- openmode openmode_
}

class Folder {
	+ Folder(void)
	+ ~Folder(void)

	+ create_folder(const string&) expected~void, string~
	+ delete_folder(const string&) expected~void, string~
	+ get_folders(const string&, bool) tuple~optional~vector~string~~, optional~string~~
	+ get_files(const string&, bool, const vector~string~&) tuple~optional~vector~string~~, optional~string~~

	+ compression(const string&, const string&, bool, const vector~string~&, uint16_t)$ expected~void, string~
	+ decompression(const string&, const string&, uint16_t)$ expected~void, string~
}

class Compressor {
	+ compression(const vector~uint8_t~&, uint16_t)$ tuple~optional~vector~uint8_t~~, optional~string~~
	+ decompression(const vector~uint8_t~&, uint16_t)$ tuple~optional~vector~uint8_t~~, optional~string~~
}

StringLog --|> Log
WStringLog --|> Log
U16StringLog --|> Log
U32StringLog --|> Log

Log "1" --> "1" LogTypes

Logger "1" o--> "0..n" Log
Logger "1" --> "1" LogTypes
```


- Log에서 사용된 map은 다음과 같은 자료형으로 구성됩니다.
```
std::map<LogTypes, std::string> message_types_
```

## Logger 사용법
``` C++
#include <iostream>

#include "Logger.h"
#include "ArgumentParser.h"

#include <format>

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
		Logger::handle().write(LogTypes::Information, std::format("Log test {}", i));
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