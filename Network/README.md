# Class Diagram

## Network

``` mermaid
classDiagram

class ConnectConditions{
	<<enumeration>>
	None
	Waiting
	Expired
	Confirmed
}

class DataModes{
	<<enumeration>>
	Binary
	File
	Message
	Connection
}

class FileModes{
	<<enumeration>>
	Start
	Success
	Failure
}

class SessionState{
	<<enumeration>>
	Create
	Handshaking
	Authenticated
	InGame
	Closing
	Closed
}

class DataHandler {
	+ DataHandler(uint16_t, uint16_t, uint16_t)
	+ ~DataHandler(void)*

	+ id(const string& new_id) void

	+ id(void) const string
	+ sub_id(void) const string

	+ start_code(char, char, char, char) void
	+ end_code(char, char, char, char) void

	+ condition(ConnectConditions, bool) void
	+ condition(void) const ConnectConditions

	+ send_binary(const vector~uint8_t~&, const string&) expected~void, string~
	+ send_message(const string&) expected~void, string~
	+ send_files(const vector~pair~string, string~~&) expected~void, string~

	# sub_id(const string&) void

	# set_self_guard(const weak_ptr~void~&) void
	# alive(void) bool
	# strand(void) shared_ptr~strand~

	# create_thread_pool(const string&) void
	# thread_pool(void) shared_ptr~Thread::ThreadPool~
	# destroy_thread_pool(void) void

	# buffer_size(size_t) void
	# buffer_size(void) const size_t

	# socket(shared_ptr~socket~) void
	# socket(void) shared_ptr~socket~
	# destroy_socket(void) void

	# send(DataModes, const vector~uint8_t~&) expected~void, string~

	# read_start_code(uint8_t) void
	# read_length_code(void) void
	# read_data(size_t) void
	# read_end_code(uint8_t) void

	# disconnected(bool)* void
	# received_data(DataModes, const vector~uint8_t~&)* expected~void, string~

	- create_receiving_buffers(size_t) void
	- destroy_receiving_buffers(void) void

	- compress_message(const vector~uint8_t~&) expected~void, string~
	- decompress_message(const vector~uint8_t~&) expected~void, string~

	# mutex mutex_

	- string id_
	- string sub_id_
	- size_t buffer_size_
	- ConnectConditions condition_
	- uint16_t high_priority_count_
	- uint16_t normal_priority_count_
	- uint16_t low_priority_count_

	- shared_ptr~Thread::ThreadPool~ thread_pool_
	- shared_ptr~socket~ socket_
	- shared_ptr~strand~ strand_
	- weak_ptr~void~ self_guard_

	- uint8_t* receiving_buffers_
	- vector~uint8_t~ start_code_tag_
	- vector~uint8_t~ end_code_tag_
	- vector~uint8_t~ received_data_
}

class NetworkClient {
	+ NetworkClient(const string&, uint16_t, uint16_t, uint16_t);
	+ ~NetworkClient(void)*

	+ get_ptr(void) shared_ptr~NetworkClient~

	+ auto_pong(bool) void

	+ start(const string&, uint16_t, size_t) bool
	+ wait_stop(uint32_t) void
	+ stop(void) void

	+ register_key(const string&) void

	+ received_connection_callback(const function~expected~void, string~(bool, bool)~&) void
	+ received_binary_callback(const function~expected~void, string~(const string&, const vector~uint8_t~&)~&) void
	+ received_message_callback(const function~expected~void, string~(const string&)~&) void
	+ received_file_callback(const function~expected~void, string~(const string&, const vector~uint8_t~&)~&) void
	+ received_files_callback(const function~expected~void, string~(const vector~string~&, const vector~pair~string, string~~&)~&) void

	# disconnected(bool) void
	# received_data(DataModes, const vector~uint8_t~&) expected~void, string~
	# request_connection(void) void

	- create_io_context(void) void
	- destroy_io_context(void) void

	- create_socket(const string&, uint16_t) bool
	- run(void) expected~void, string~

	- received_connection(const vector~uint8_t~&) expected~void, string~
	- received_binary(const vector~uint8_t~&) expected~void, string~
	- received_message(const vector~uint8_t~&) expected~void, string~
	- received_file(const vector~uint8_t~&) expected~void, string~
	- received_files(const vector~string~&, const vector~pair~string, string~~&) expected~void, string~

	- string server_id_
	- string registered_key_

	- unique_ptr~FileManager~ file_manager_

	- shared_ptr~io_context~ io_context_
	- map~DataModes, const function~expected~void, string~(const vector~uint8_t~&)~~ message_handlers_

	- future~bool~ future_status_
	- unique_ptr~promise~bool~~ promise_status_

	- function~expected~void, string~(bool, bool)~ received_connection_callback_
	- function~expected~void, string~(const string&)~ received_message_callback_
	- function~expected~void, string~(const string&, const vector~uint8_t~&)~ received_file_callback_
	- function~expected~void, string~(const string&, const vector~uint8_t~&)~ received_binary_callback_
	- function~expected~void, string~(const vector~string~&, const vector~pair~string, string~~&)~ received_files_callback_

	- bool auto_pong_enabled_
}

class NetworkSession {
	+ NetworkSession(const string&, uint16_t, uint16_t, uint16_t, bool, uint32_t);
	+ ~NetworkSession(void)*

	+ session_id(void) SessionId
	+ session_id(SessionId) void
	+ state(void) SessionState
	+ get_ptr(void) shared_ptr~NetworkSession~

	+ heartbeat(bool, uint32_t) void
	+ set_max_missed_heartbeats(uint32_t) void

	+ start(shared_ptr~socket~, size_t) void
	+ stop(void) void

	+ register_key(const string&) void

	+ received_connection_callback(const function~expected~void, string~(const vector~uint8_t~&)~&) void
	+ received_binary_callback(const function~expected~void, string~(const string&, const string&, const string&, const vector~uint8_t~&)~&) void
	+ received_message_callback(const function~expected~void, string~(const string&, const string&, const string&)~&) void
	+ received_file_callback(const function~expected~void, string~(const string&, const string&, const string&, const vector~uint8_t~&)~&) void
	+ received_files_callback(const function~expected~void, string~(const string&, const string&, const vector~string~&, const vector~pair~string, string~~&)~&) void

	# disconnected(bool) void
	# received_data(DataModes, const vector~uint8_t~&) expected~void, string~

	- received_connection(const vector~uint8_t~&) expected~void, string~
	- received_binary(const vector~uint8_t~&) expected~void, string~
	- received_message(const vector~uint8_t~&) expected~void, string~
	- received_file(const vector~uint8_t~&) expected~void, string~
	- received_files(const vector~string~&, const vector~pair~string, string~~&) expected~void, string~

	- response_connection(bool) expected~void, string~
	- start_heartbeat(void) void
	- stop_heartbeat(void) void

	- SessionId session_id_
	- SessionState state_
	- string server_id_
	- string registered_key_
	- map~DataModes, const function~expected~void, string~(const vector~uint8_t~&)~~ message_handlers_

	- unique_ptr~FileManager~ file_manager_

	- shared_ptr~steady_timer~ handshake_timer_
	- atomic~bool~ stopped_

	- bool heartbeat_enabled_
	- uint32_t heartbeat_interval_sec_
	- shared_ptr~steady_timer~ heartbeat_timer_

	- time_point last_pong_at_
	- uint32_t missed_heartbeats_
	- uint32_t max_missed_heartbeats_

	- function~expected~void, string~(const vector~uint8_t~&)~ received_connection_callback_
	- function~expected~void, string~(const string&, const string&, const string&)~ received_message_callback_
	- function~expected~void, string~(const string&, const string&, const string&, const vector~uint8_t~&)~ received_file_callback_
	- function~expected~void, string~(const string&, const string&, const string&, const vector~uint8_t~&)~ received_binary_callback_
	- function~expected~void, string~(const string&, const string&, const vector~string~&, const vector~pair~string, string~~&)~ received_files_callback_
}

class NetworkServer {
	+ NetworkServer(const string&, uint16_t, uint16_t, uint16_t);
	+ ~NetworkServer(void)*

	+ get_ptr(void) shared_ptr~NetworkServer~

	+ id(const string&) void

	+ id(void) const string

	+ register_key(const string&) void

	+ heartbeat_mode(bool, uint32_t) void
	+ heartbeat_tolerance(uint32_t) void
	+ maintenance_interval(uint32_t) void

	+ start(uint16_t, size_t) expected~void, string~
	+ send_binary(const vector~uint8_t~&, const string&, const string&, const string&) expected~void, string~
	+ send_message(const string&, const string&, const string&) expected~void, string~
	+ send_files(const vector~pair~string, string~~&, const string&, const string&) expected~void, string~
	+ wait_stop(uint32_t) expected~void, string~
	+ stop(void) expected~void, string~

	+ received_connection_callback(const function~expected~void, string~(const string&, const string&, bool)~&) void
	+ received_binary_callback(const function~expected~void, string~(const string&, const string&, const string&, const vector~uint8_t~&)~&) void
	+ received_message_callback(const function~expected~void, string~(const string&, const string&, const string&)~&) void
	+ received_file_callback(const function~expected~void, string~(const string&, const string&, const string&, const vector~uint8_t~&)~&) void
	+ received_files_callback(const function~expected~void, string~(const string&, const string&, const vector~string~&, const vector~pair~string, string~~&)~&) void

	+ drop_session(const string&, const string&) void
	+ drop_sessions(const string&) void

	# received_connection(const vector~uint8_t~&) expected~void, string~
	# received_binary(const string&, const string&, const string&, const vector~uint8_t~&) expected~void, string~
	# received_message(const string&, const string&, const string&) expected~void, string~
	# received_file(const string&, const string&, const string&, const vector~uint8_t~&) expected~void, string~
	# received_files(const string&, const string&, const vector~string~&, const vector~pair~string, string~~&) expected~void, string~

	- create_io_context(uint16_t) bool
	- destroy_io_context(void) void

	- create_thread_pool(void) void
	- destroy_thread_pool(void) void

	- drop_sessions(void) void

	- start_main_job(void) void

	- wait_connection(void) void
	- received_connection_handler(const vector~uint8_t~&) expected~void, string~
	- run(void) expected~void, string~

	- start_maintenance_job(void) void
	- stop_maintenance_job(void) void

	- string id_
	- string registered_key_

	- size_t buffer_size_

	- uint16_t high_priority_count_
	- uint16_t normal_priority_count_
	- uint16_t low_priority_count_

	- bool heartbeat_enabled_
	- uint32_t heartbeat_interval_sec_

	- mutex mutex_
	- vector~shared_ptr~NetworkSession~~ sessions_

	- future~bool~ future_status_
	- unique_ptr~promise~bool~~ promise_status_

	- shared_ptr~Thread::ThreadPool~ thread_pool_
	- shared_ptr~io_context~ io_context_
	- shared_ptr~acceptor~ acceptor_
	- shared_ptr~steady_timer~ maintenance_timer_

	- uint32_t maintenance_interval_sec_
	- uint32_t heartbeat_missed_tolerance_

	- function~expected~void, string~(const string&, const string&, bool)~ received_connection_callback_
	- function~expected~void, string~(const string&, const string&, const string&)~ received_message_callback_
	- function~expected~void, string~(const string&, const string&, const string&, const vector~uint8_t~&)~ received_file_callback_
	- function~expected~void, string~(const string&, const string&, const string&, const vector~uint8_t~&)~ received_binary_callback_
	- function~expected~void, string~(const string&, const string&, const vector~string~&, const vector~pair~string, string~~&)~ received_files_callback_
}

class SendingJob{
	+ SendingJob(shared_ptr~socket~, const vector~uint8_t~&, const vector~uint8_t~&, const vector~uint8_t~&, size_t)
	+ ~SendingJob(void)*

	- working(void) expected~void, string~

	- send_start(void) expected~void, string~
	- send_length(const uint64_t&) expected~void, string~
	- send_data(const vector~uint8_t~&) expected~void, string~
	- send_end(void) expected~void, string~

	- vector~uint8_t~ start_code_
	- vector~uint8_t~ end_code_
	- size_t buffer_size_
	- shared_ptr~socket~ socket_
}

class ReceivingJob{
	+ ReceivingJob(const vector~uint8_t~&, const function~expected~void, string~(DataModes, const vector~uint8_t~&)~&)
	+ ~ReceivingJob(void)*

	- working(void) expected~void, string~

	- function~expected~void, string~(DataModes, const vector~uint8_t~&)~ receiving_callback_
}

class ConnectionJob{
	+ ConnectionJob(bool, bool, const function~expected~void, string~(bool, bool)~&)
	+ ~ConnectionJob(void)*

	- working(void) expected~void, string~

	- bool condition_
	- bool by_itself_
	- function~expected~void, string~(bool, bool)~ connection_callback_
}

class FileSendingJob{
	+ FileSendingJob(const vector~uint8_t~&, const function~expected~void, string~(DataModes, const vector~uint8_t~&)~&)
	+ ~FileSendingJob(void)*

	- working(void) expected~void, string~

	- function~expected~void, string~(DataModes, const vector~uint8_t~&)~ sending_callback_
}

DataHandler <|-- NetworkClient
DataHandler <|-- NetworkSession

DataModes o-- DataHandler
SendingJob o-- DataHandler
ReceivingJob o-- DataHandler
ConnectionJob o-- DataHandler
FileSendingJob o-- DataHandler

ConnectConditions o-- DataHandler
ConnectConditions o-- NetworkServer

NetworkServer --o NetworkSession
```

NetworkServer 사용법
``` C++
#include <iostream>

#include "Logger.h"
#include "Converter.h"
#include "NetworkServer.h"
#include "ArgumentParser.h"

#include <format>

#include <memory>
#include <string>
#include <signal.h>

using namespace Network;
using namespace Utilities;

auto register_signal(void) -> void;
auto signal_callback(int32_t signum) -> void;
bool received_connection(const std::string& id, const std::string& sub_id, const bool& condition);
bool received_message(const std::string& id, const std::string& sub_id, const std::string& message);
auto parse_arguments(ArgumentParser& arguments) -> void;

std::shared_ptr<NetworkServer> server_ = nullptr;

bool write_file_ = false;
bool write_console_ = true;
LogTypes type_ = LogTypes::Information;
uint16_t server_port_ = 9876;
uint16_t high_priority_count_ = 3;
uint16_t normal_priority_count_ = 3;
uint16_t low_priority_count_ = 3;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().target_type(type_);
	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("NetworkServerSample");

	register_signal();

	server_ = std::make_shared<NetworkServer>("SampleServer", high_priority_count_, normal_priority_count_, low_priority_count_);
	server_->received_connection_callback(&received_connection);
	server_->received_message_callback(&received_message);

	server_->start(server_port_);
	server_->wait_stop();
	server_.reset();

	Logger::handle().stop();
	Logger::destroy();

	return 0;
}

auto register_signal(void) -> void
{
	signal(SIGINT, signal_callback);
	signal(SIGILL, signal_callback);
	signal(SIGABRT, signal_callback);
	signal(SIGFPE, signal_callback);
	signal(SIGSEGV, signal_callback);
	signal(SIGTERM, signal_callback);
}

auto signal_callback(int32_t signum) -> void
{
	if (server_ == nullptr)
	{
		return;
	}

	server_->stop();
}

bool received_connection(const std::string& id, const std::string& sub_id, const bool& condition)
{
	if (server_ == nullptr)
	{
		return false;
	}

	Logger::handle().write(LogTypes::Information, std::format("received condition message from NetworkClientSample : [{}:{}] => {}", id, sub_id, condition));

	return true;
}

bool received_message(const std::string& id, const std::string& sub_id, const std::string& message)
{
	if (server_ == nullptr)
	{
		return false;
	}

	return server_->send_message(message, id, sub_id);
}

auto parse_arguments(ArgumentParser& arguments) -> void
{
	auto ushort_target = arguments.to_ushort("--server_port");
	if (ushort_target != std::nullopt)
	{
		server_port_ = ushort_target.value();
	}

	ushort_target = arguments.to_ushort("--high_priority_count");
	if (ushort_target != std::nullopt)
	{
		high_priority_count_ = ushort_target.value();
	}

	ushort_target = arguments.to_ushort("--normal_priority_count");
	if (ushort_target != std::nullopt)
	{
		normal_priority_count_ = ushort_target.value();
	}

	ushort_target = arguments.to_ushort("--low_priority_count");
	if (ushort_target != std::nullopt)
	{
		low_priority_count_ = ushort_target.value();
	}

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

NetworkClient 사용법
``` C++
#include <iostream>

#include "Logger.h"
#include "Converter.h"
#include "NetworkClient.h"
#include "ArgumentParser.h"

#include <format>

#include <memory>
#include <string>
#include <signal.h>

using namespace Network;
using namespace Utilities;

auto register_signal(void) -> void;
auto signal_callback(int32_t signum) -> void;
bool received_connection(const bool& condition);
bool received_message(const std::string& message);
auto parse_arguments(ArgumentParser& arguments) -> void;

std::shared_ptr<NetworkClient> client_ = nullptr;

bool write_file_ = false;
bool write_console_ = true;
LogTypes type_ = LogTypes::Information;
std::string server_ip_ = "127.0.0.1";
uint16_t server_port_ = 9876;
uint16_t high_priority_count_ = 3;
uint16_t normal_priority_count_ = 3;
uint16_t low_priority_count_ = 3;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().target_type(type_);
	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("NetworkClientSample");

	register_signal();

	client_ = std::make_shared<NetworkClient>("SampleClient", high_priority_count_, normal_priority_count_, low_priority_count_);
	client_->received_connection_callback(&received_connection);
	client_->received_message_callback(&received_message);

	client_->start(server_ip_, server_port_);
	client_->wait_stop();
	client_.reset();

	Logger::handle().stop();
	Logger::destroy();

	return 0;
}

auto register_signal(void) -> void
{
	signal(SIGINT, signal_callback);
	signal(SIGILL, signal_callback);
	signal(SIGABRT, signal_callback);
	signal(SIGFPE, signal_callback);
	signal(SIGSEGV, signal_callback);
	signal(SIGTERM, signal_callback);
}

auto signal_callback(int32_t signum) -> void
{
	if (client_ == nullptr)
	{
		return;
	}

	client_->stop();
}

bool received_connection(const bool& condition)
{
	if (client_ == nullptr)
	{
		return false;
	}

	Logger::handle().write(LogTypes::Information, std::format("received condition of connection : {}", condition));

	if (!condition)
	{
		client_->stop();

		return false;
	}

	return client_->send_message("echo");
}

bool received_message(const std::string& message)
{
	if (client_ == nullptr)
	{
		return false;
	}

	Logger::handle().write(LogTypes::Information, std::format("received_message: {}", message));

	return client_->send_message(message);
}

auto parse_arguments(ArgumentParser& arguments) -> void
{
	auto string_target = arguments.to_string("--server_ip");
	if (string_target != std::nullopt)
	{
		server_ip_ = string_target.value();
	}

	auto ushort_target = arguments.to_ushort("--server_port");
	if (ushort_target != std::nullopt)
	{
		server_port_ = ushort_target.value();
	}

	ushort_target = arguments.to_ushort("--high_priority_count");
	if (ushort_target != std::nullopt)
	{
		high_priority_count_ = ushort_target.value();
	}

	ushort_target = arguments.to_ushort("--normal_priority_count");
	if (ushort_target != std::nullopt)
	{
		normal_priority_count_ = ushort_target.value();
	}

	ushort_target = arguments.to_ushort("--low_priority_count");
	if (ushort_target != std::nullopt)
	{
		low_priority_count_ = ushort_target.value();
	}

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