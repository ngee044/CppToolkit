// NetworkServerSample.cpp : This file contains the 'main' function. Program
// execution begins and ends there.
//

#include <iostream>

#include "ArgumentParser.h"
#include "Converter.h"
#include "Logger.h"
#include "NetworkServer.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include <tuple>
#include <memory>
#include <signal.h>
#include <string>
#include <optional>

using namespace Network;
using namespace Utilities;

auto register_signal(void) -> void;
auto signal_callback(int32_t signum) -> void;

auto parse_arguments(ArgumentParser& arguments) -> void;

std::shared_ptr<NetworkServer> server_ = nullptr;

#ifdef _DEBUG
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Packet;
#else
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;
#endif
uint16_t server_port_ = 9090;
size_t buffer_size_ = 1024;
uint16_t high_priority_count_ = 3;
uint16_t normal_priority_count_ = 3;
uint16_t low_priority_count_ = 3;
uint16_t write_interval_ = 1000;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().write_interval(write_interval_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("NetworkServerSample");

	register_signal();

	server_ = std::make_shared<NetworkServer>("SampleServer", high_priority_count_, normal_priority_count_, low_priority_count_);
	server_->register_key("test_key");

	server_->received_connection_callback(
		[](const std::string& id, const std::string& sub_id, const bool& condition) -> std::tuple<bool, std::optional<std::string>>
		{
			if (server_ == nullptr)
			{
				return { false, "server has no handle" };
			}

			Logger::handle().write(LogTypes::Information, fmt::format("received condition message from "
																	  "NetworkClientSample : [{}:{}] => {}",
																	  id, sub_id, condition));

			return { true, std::nullopt };
		});
	server_->received_message_callback(
		[](const std::string& id, const std::string& sub_id, const std::string& message) -> std::tuple<bool, std::optional<std::string>>
		{
			if (server_ == nullptr)
			{
				return { false, "server has no handle" };
			}

			Logger::handle().write(LogTypes::Information, fmt::format("received_message: {}", message));

			return server_->send_binary(Converter::to_array("send_binary"), message, id, sub_id);
		});
	server_->received_binary_callback(
		[](const std::string& id, const std::string& sub_id, const std::string& message, const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
		{
			if (server_ == nullptr)
			{
				return { false, "server has no handle" };
			}

			Logger::handle().write(LogTypes::Information, fmt::format("received_binary: {}", message));

			return server_->send_message(message, id, sub_id);
		});
	server_->received_file_callback(
		[](const std::string& id, const std::string& sub_id, const std::string& message,
		   const std::vector<uint8_t>& file_path) -> std::tuple<bool, std::optional<std::string>>
		{
			if (server_ == nullptr)
			{
				return { false, "server has no handle" };
			}

			Logger::handle().write(LogTypes::Information, fmt::format("received_file: {}", message));

			return server_->send_message(message, id, sub_id);
		});
	server_->received_files_callback(
		[](const std::string& id, const std::string& sub_id, const std::vector<std::string>& failures,
		   const std::vector<std::pair<std::string, std::string>>& successes) -> std::tuple<bool, std::optional<std::string>>
		{
			if (server_ == nullptr)
			{
				return std::make_tuple(false, "server has no handle");
			}

			Logger::handle().write(LogTypes::Information, fmt::format("received_files: successes[{}], failures[{}]", successes.size(), failures.size()));

			return { true, std::nullopt };
		});

#ifdef USE_ENCRYPT_MODULE
	server_->encrypt_mode(true);
#endif

	server_->start(server_port_, buffer_size_);
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

	if (server_ == nullptr)
	{
		return;
	}

	server_->stop();
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