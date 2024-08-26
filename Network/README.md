# Class Diagram

## Network

``` mermaid
classDiagram

class ConnectCondition{
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

class DataHandler {
	+ DataHandler(const uint16_t&, const uint16_t&, const uint16_t&)
	+ ~DataHandler(void)*

	+ id(const u32string& new_id) void
	+ id(const u16string& new_id) void
	+ id(const wstring& new_id) void
	+ id(const string& new_id) void

	+ id(void) const string
	+ sub_id(void) const string

	+ start_code(const char&, const char&, const char&, const char&) void
	+ end_code(const char&, const char&, const char&, const char&) void

	+ condition(const ConnectConditions&, const bool&) void
	+ condition(void) const ConnectConditions

	+ send_binary(const vector~uint8_t~&)bool

	+ send_message(const u32string&) bool
	+ send_message(const u16string&) bool
	+ send_message(const wstring&) bool
	+ send_message(const string&) bool

	+ send_file(const u32string&) bool
	+ send_file(const u16string&) bool
	+ send_file(const wstring&) bool
	+ send_file(const string&) bool

	# encrypt_mode(const bool&) void

	# key(const u32string&) void
	# key(const u16string&) void
	# key(const wstring&) void
	# key(const string&) void
	# key(void) const string

	# iv(const u32string&) void
	# iv(const u16string&) void
	# iv(const wstring&) void
	# iv(const string&) void
	# iv(void) const string

	# sub_id(const u32string&) void
	# sub_id(const u16string&) void
	# sub_id(const wstring&) void
	# sub_id(const string&) void

	# create_thread_pool(const string&) void
	# thread_pool(void) shared_ptr~ThreadPool~
	# destroy_thread_pool(void) void

	# buffer_size(const size_t&) void
	# buffer_size(void) const size_t

	# socket(shared_ptr~socket~) void
	# socket(void) shared_ptr~socket~
	# destroy_socket(void) void

	# sub_priorities_for_high(void) vector~JobPriorities~&
	# sub_priorities_for_normal(void) vector~JobPriorities~&
	# sub_priorities_for_low(void) vector~JobPriorities~&

	# send(const DataModes&, const vector~uint8_t~&) bool

	# read_start_code(const uint8_t&) void
	# read_legnth_code(void) void
	# read_data(const size_t&) void
	# read_end_code(const uint8_t&) void
  
	# disconnected(void)* void
	# received_message(const DataModes& mode, const vector~uint8_t~& data)* bool

	- create_receiving_buffers(const size_t& size) void
	- destroy_receiving_buffers(void) void

	- compress_message(const vector~uint8_t~&) bool
	- decompress_message(const vector~uint8_t~&) bool

	- encrypt_message(const vector~uint8_t~&) bool
	- decrypt_message(const vector~uint8_t~&) bool

	# mutex mutex_

	- string id_
	- string sub_id_
	- size_t buffer_size_
	- ConnectConditions condition_
	- uint16_t high_priority_count_
	- uint16_t normal_priority_count_
	- uint16_t low_priority_count_

	- string key_
	- string iv_

	- bool encrypt_mode_

	- vector~JobPriorities~ sub_priorities_for_high_
	- vector~JobPriorities~ sub_priorities_for_normal_
	- vector~JobPriorities~ sub_priorities_for_low_

	- shared_ptr~ThreadPool~ thread_pool_
	- shared_ptr~socket~ socket_

	- uint8_t* receiving_buffers_
	- vector~uint8_t~ start_code_tag_
	- vector~uint8_t~ end_code_tag_
	- vector~uint8_t~ received_data_
}

class NetworkClient {
	+ NetworkClient(const u32string&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ NetworkClient(const u16string&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ NetworkClient(const wstring&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ NetworkClient(const string&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ ~NetworkClient(void)*

	+ get_ptr(void) shared_ptr~NetworkClient~ 
  
	+ start(const u32string&, const uint16_t&) void
	+ start(const u16string&, const uint16_t&) void
	+ start(const wstring&, const uint16_t&) void
	+ start(const string&, const uint16_t&) void
	+ wait_stop(const uint32_t&) void
	+ stop(void) void

	+ register_key(const u32string&) void
	+ register_key(const u16string&) void
	+ register_key(const wstring&) void
	+ register_key(const string&) void
  
	+ received_connection_callback(const function~bool(const bool&)~&) void
	+ received_binary_callback(const function~bool(const vector~uint8_t~&)~&) void
	+ received_message_callback(const function~bool(const string&)~&) void
	+ received_file_callback(const function~bool(const string&, const vector~uint8_t~&)~&) void

	# disconnected(void) void
	# received_message(const DataModes& mode, const vector~uint8_t~& data) bool
	# request_connection(void) void

	- create_io_context(void) void
	- destroy_io_context(void) void

	- create_socket(const string& ip, const uint16_t& port) bool
	- run(void) bool

	- binary_message(const vector~uint8_t~& data) bool
	- normal_message(const vector~uint8_t~& data) bool
	- file_message(const vector~uint8_t~& data) bool
	- connection_message(const vector~uint8_t~& data) bool

	- string server_id_
	- string registered_key_

	- shared_ptr~io_context~ io_context_
	- map~DataModes, const function~bool(const vector~uint8_t~&)~~ message_handlers_

	- future~bool~ future_status_
	- optional~promise~bool~~ promise_status_

	- function~bool(const bool&)~ received_connection_callback_
	- function~bool(const vector~uint8_t~&)~ received_binary_callback_
	- function~bool(const string&)~ received_message_callback_
	- function~bool(const string&, const vector~uint8_t~&)~ received_file_callback_
}

class NetworkSession {
	+ NetworkSession(const string&, const bool&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ ~NetworkSession(void)*

	+ get_ptr(void) shared_ptr~NetworkSession~
  
	+ start(shared_ptr~socket~ ) void
	+ stop(void) void
  
	+ register_key(const string& key) void
  
	+ received_connection_callback(const function~bool(const vector~uint8_t~&)~&) void
	+ received_binary_callback(const function~bool(const string&, const string&, const vector~uint8_t~&)~&) void
	+ received_message_callback(const function~bool(const string&, const string&, const string&)~&) void
	+ received_file_callback(const function~bool(const string&, const string&, const string&, const vector~uint8_t~&)~&) void

	# disconnected(void) void
	# received_message(const DataModes& mode, const vector~uint8_t~& data) bool

	- binary_message(const vector~uint8_t~& data) bool
	- normal_message(const vector~uint8_t~& data) bool
	- file_message(const vector~uint8_t~& data) bool
	- connection_message(const vector~uint8_t~& data) bool

	- response_connection(const bool& condition) bool

	- string server_id_
	- string registered_key_
	- map~DataModes, const function~bool(const vector~uint8_t~&)~~ message_handlers_

	- function~bool(const vector~uint8_t~&)~ received_connection_callback_
	- function~bool(const string&, const string&, const vector~uint8_t~&)~ received_binary_callback_
	- function~bool(const string&, const string&, const string&)~ received_message_callback_
	- function~bool(const string&, const string&, const string&, const vector~uint8_t~&)~ received_file_callback_
}

class NetworkServer {
	+ NetworkServer(const u32string&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ NetworkServer(const u16string&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ NetworkServer(const wstring&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ NetworkServer(const string&, const uint16_t&, const uint16_t&, const uint16_t&);
	+ ~NetworkServer(void)*
	
	+ get_ptr(void) shared_ptr~NetworkServer~

	+ id(const u32string& new_id) void
	+ id(const u16string& new_id) void
	+ id(const wstring& new_id) void
	+ id(const string& new_id) void

	+ id(void) const string

	+ encrypt_mode(const bool&) void
	+ encrypt_mode(void) const bool

	+ register_key(const u32string&) void
	+ register_key(const u16string&) void
	+ register_key(const wstring&) void
	+ register_key(const string&) void
	
	+ start(const uint16_t&) void
	+ send_binary(const vector~uint8_t~&, const string&, const string&) bool
	+ send_message(const string&, const string&, const string&) bool
	+ send_file(const string&, const string&, const string&) bool
	+ wait_stop(const uint32_t&) void
	+ stop(void) void
	
	+ received_connection_callback(const function~bool(const string&, const string&, const bool&)~&) void
	+ received_binary_callback(const function~bool(const string&, const string&, const vector~uint8_t~&)~&) void
	+ received_message_callback(const function~bool(const string&, const string&, const string&)~&) void
	+ received_file_callback(const function~bool(const string&, const string&, const string&, const vector~uint8_t~&)~&) void
	
	# received_connection(const vector~uint8_t~&) bool
	# received_binary(const string&, const string&, const vector~uint8_t~&) bool
	# received_message(const string&, const string&, const string&) bool
	# received_file(const string&, const string&, const string&, const vector~uint8_t~&) bool
	
	- create_io_context(const uint16_t& port) void
	- destroy_io_context(void) void
	
	- wait_connection(void) void
	- received_connection_handler(const vector~uint8_t~& condition) bool
	- run(void) bool
	
	- string id_
	- string registered_key_

	- uint16_t high_priority_count_
	- uint16_t normal_priority_count_
	- uint16_t low_priority_count_
	
	- bool encrypt_mode_

	- mutex mutex_
	- vector~shared_ptr~NetworkSession~~ sessions_
	
	- future~bool~ future_status_
	- optional~promise~bool~~ promise_status_
	
	- shared_ptr~ThreadPool~ thread_pool_
	- shared_ptr~io_context~ io_context_
	- shared_ptr~acceptor~ acceptor_
	
	- function~bool(const string&, const string&, const bool&)~ received_connection_callback_
	- function~bool(const string&, const string&, const vector~uint8_t~&)~ received_binary_callback_
	- function~bool(const string&, const string&, const string&)~ received_message_callback_
	- function~bool(const string&, const string&, const string&, const vector~uint8_t~&)~ received_file_callback_
}

class SendingJob{
	+ SendingJob(shared_ptr~socket~, const vector~uint8_t~&, const vector~uint8_t~&, const vector~uint8_t~&)
	+ SendingJob(void)*

	- working(void) bool

	- send_start(void) bool
	- send_length(const uint32_t&) bool
	- send_data(const vector~uint8_t~&) bool
	- send_end(void) bool

	- vector~uint8_t~ start_code_
	- vector~uint8_t~ end_code_
	- shared_ptr~socket~ socket_
}

class ReceivingJob{
	+ ReceivingJob(shared_ptr~socket~, const vector~uint8_t~&, const vector~uint8_t~&, const vector~uint8_t~&)
	+ ReceivingJob(void)*

	- working(void) bool

	- function~bool(const DataModes&, const vector~uint8_t~&)~ receiving_callback_
}

DataHandler <|-- NetworkClient
DataHandler <|-- NetworkSession

DataModes o-- DataHandler
SendingJob o-- DataHandler
ReceivingJob o-- DataHandler

ConnectCondition o-- DataHandler
ConnectCondition o-- NetworkServer

NetworkServer --o NetworkSession
``` 

NetworkServer 사용법
``` C++
#include <iostream>

#include "Logger.h"
#include "Converter.h"
#include "NetworkServer.h"
#include "ArgumentParser.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

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

	Logger::handle().write(LogTypes::Information, fmt::format("received condition message from NetworkClientSample : [{}:{}] => {}", id, sub_id, condition));

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

#include "fmt/format.h"
#include "fmt/xchar.h"

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

	Logger::handle().write(LogTypes::Information, fmt::format("received condition of connection : {}", condition));

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

	Logger::handle().write(LogTypes::Information, fmt::format("received_message: {}", message));

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