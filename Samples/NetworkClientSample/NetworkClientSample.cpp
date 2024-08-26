// NetworkClientSample.cpp : This file contains the 'main' function. Program
// execution begins and ends there.
//

#include <iostream>

#include "ArgumentParser.h"
#include "Converter.h"
#include "Logger.h"
#include "NetworkClient.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include <memory>
#include <signal.h>
#include <string>

using namespace Network;
using namespace Utilities;

auto register_signal(void) -> void;
auto signal_callback(int32_t signum) -> void;
auto parse_arguments(ArgumentParser& arguments) -> void;

std::shared_ptr<NetworkClient> client_ = nullptr;

#ifdef _DEBUG
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Packet;
#else
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;
#endif
std::string server_ip_ = "127.0.0.1";
uint16_t server_port_ = 9090;
size_t buffer_size_ = 1024;
uint16_t high_priority_count_ = 3;
uint16_t normal_priority_count_ = 3;
uint16_t low_priority_count_ = 3;
uint16_t write_interval_ = 1000;

bool kill_signal = false;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().write_interval(write_interval_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("NetworkClientSample");

	register_signal();

	while (true)
	{
		client_ = std::make_shared<NetworkClient>("SampleClient", high_priority_count_, normal_priority_count_, low_priority_count_);

		client_->register_key("test_key");
		client_->received_connection_callback(
			[](const bool& condition, const bool& by_itself) -> std::tuple<bool, std::optional<std::string>>
			{
				if (client_ == nullptr)
				{
					return { false, "client has no handle" };
				}

				Logger::handle().write(LogTypes::Information, fmt::format("received condition of connection: {}, by itself: {}", condition, by_itself));

				if (!condition)
				{
					kill_signal = by_itself;

					if (!by_itself)
					{
						client_->stop();
					}

					return { false, "client has no connection" };
				}

				return client_->send_message("echo");
			});
		client_->received_message_callback(
			[](const std::string& message) -> std::tuple<bool, std::optional<std::string>>
			{
				if (client_ == nullptr)
				{
					return { false, "client has no handle" };
				}

				return client_->send_binary(Converter::to_array("send_binary"), message);
			});
		client_->received_binary_callback(
			[](const std::string& message, const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
			{
				if (client_ == nullptr)
				{
					return { false, "client has no handle" };
				}

				Logger::handle().write(LogTypes::Information, fmt::format("received_binary: {}", message));

				return client_->send_message(message);
			});
		client_->received_file_callback(
			[](const std::string& message, const std::vector<uint8_t>& file_path) -> std::tuple<bool, std::optional<std::string>>
			{
				if (client_ == nullptr)
				{
					return { false, "client has no handle" };
				}

				Logger::handle().write(LogTypes::Information, fmt::format("received_binary: {}", message));

				return client_->send_message(message);
			});
		client_->received_files_callback(
			[](const std::vector<std::string>& failures,
			   const std::vector<std::pair<std::string, std::string>>& successes) -> std::tuple<bool, std::optional<std::string>>
			{
				if (client_ == nullptr)
				{
					return { false, "client has no handle" };
				}

				return { true, std::nullopt };
			});

		if (client_->start(server_ip_, server_port_, buffer_size_))
		{
			client_->wait_stop();

			client_->received_connection_callback(nullptr);
			client_->received_message_callback(nullptr);
			client_->received_binary_callback(nullptr);
			client_->received_file_callback(nullptr);
			client_->received_files_callback(nullptr);
			client_.reset();

			if (kill_signal)
			{
				break;
			}
		}

		client_.reset();

		Logger::handle().write(LogTypes::Error, fmt::format("cannot connect to server : {}:{}", server_ip_, server_port_));

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

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

auto deregister_signal(void) -> void
{
	signal(SIGINT, nullptr);
	signal(SIGILL, nullptr);
	signal(SIGABRT, nullptr);
	signal(SIGFPE, nullptr);
	signal(SIGSEGV, nullptr);
	signal(SIGTERM, nullptr);
}

auto signal_callback(int32_t signum) -> void
{
	deregister_signal();

	kill_signal = true;

	if (client_ == nullptr)
	{
		return;
	}

	client_->stop();
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

	ushort_target = arguments.to_ushort("--write_interval");
	if (ushort_target != std::nullopt)
	{
		write_interval_ = ushort_target.value();
	}

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
}