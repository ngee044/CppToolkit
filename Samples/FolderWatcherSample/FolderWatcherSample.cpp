// FolderCompressionSample.cpp : This file contains the 'main' function. Program execution
// begins and ends there.
//

#include <iostream>

#include "File.h"
#include "Folder.h"
#include "Logger.h"
#include "Converter.h"
#include "FolderWatcher.h"
#include "ArgumentParser.h"

#include <format>

#include "boost/json.hpp"
#include "boost/json/parse.hpp"

#include <tuple>
#include <string>
#include <vector>
#include <expected>
#include <optional>
#include <filesystem>

using namespace Utilities;

auto parse_arguments(ArgumentParser& arguments) -> void;

LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("FolderWatcherSample");

	std::error_code error_code;
	auto temp_path = std::filesystem::temp_directory_path(error_code);
	if (error_code)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot get temp directory path: {}, {}", error_code.message(), temp_path.string()));

		Logger::handle().stop();
		Logger::destroy();

		return 0;
	}

	std::filesystem::path new_path(temp_path);
	new_path.append("test_folder_for_watcher");

	Folder folder;
	folder.delete_folder(new_path.string());
	auto create_result = folder.create_folder(new_path.string());
	if (!create_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot create temp directory: {}, {}", create_result.error(), new_path.string()));

		Logger::handle().stop();
		Logger::destroy();

		return 0;
	}

	Logger::handle().write(LogTypes::Information, std::format("create temp directory: {}", new_path.string()));

	FolderWatcher::handle().set_callback(
		[](const std::string& dir, const std::string& filename, efsw::Action action, const std::string&)
		{
			switch (action)
			{
			case efsw::Actions::Add:
				Logger::handle().write(LogTypes::Information, std::format("{} has added on {}", filename, dir));
				break;
			case efsw::Actions::Delete:
				Logger::handle().write(LogTypes::Information, std::format("{} has deleted on {}", filename, dir));
				break;
			case efsw::Actions::Modified:
				Logger::handle().write(LogTypes::Information, std::format("{} has modified on {}", filename, dir));
				break;
			case efsw::Actions::Moved:
				Logger::handle().write(LogTypes::Information, std::format("{} has moved on {}", filename, dir));
				break;
			}
		});

	FolderWatcher::handle().start({ { new_path.string(), true } });

	std::filesystem::path new_path2(new_path);
	new_path2.append("test_folder");

	auto create_result2 = folder.create_folder(new_path2.string());
	if (!create_result2)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot create sub temp directory: {}, {}", create_result2.error(), new_path2.string()));

		Logger::handle().stop();
		Logger::destroy();

		return 0;
	}

	Logger::handle().write(LogTypes::Information, std::format("create sub temp directory: {}", new_path2.string()));

	std::filesystem::path new_file_path(new_path);
	new_file_path.append("test.json");

	File file;
	auto open_result = file.open(new_file_path.string(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!open_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot create temp file: {}, {}", open_result.error(), new_file_path.string()));

		Logger::handle().stop();
		Logger::destroy();

		return 0;
	}

	auto write_result = file.write_bytes(Converter::to_array("compressed_bytes"));
	if (!write_result)
	{
		file.close();

		Logger::handle().write(LogTypes::Error, std::format("cannot create temp file: {}", write_result.error()));

		Logger::handle().stop();
		Logger::destroy();

		return 0;
	}
	file.close();

	Logger::handle().write(LogTypes::Information, std::format("create temp file: {}", new_file_path.string()));

	std::filesystem::path new_file_path2(new_path2);
	new_file_path2.append("test.json");

	auto open_result2 = file.open(new_file_path2.string(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!open_result2)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot create temp file: {}, {}", open_result2.error(), new_file_path2.string()));

		Logger::handle().stop();
		Logger::destroy();

		return 0;
	}

	auto write_result2 = file.write_bytes(Converter::to_array("compressed_bytes"));
	if (!write_result2)
	{
		file.close();

		Logger::handle().write(LogTypes::Error, std::format("cannot create temp file: {}", write_result2.error()));

		Logger::handle().stop();
		Logger::destroy();

		return 0;
	}
	file.close();

	Logger::handle().write(LogTypes::Information, std::format("create temp file: {}", new_file_path2.string()));

	FolderWatcher::handle().stop();

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
}