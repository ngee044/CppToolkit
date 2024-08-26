// FolderCompressionSample.cpp : This file contains the 'main' function. Program execution
// begins and ends there.
//

#include <iostream>

#include "File.h"
#include "Folder.h"
#include "Logger.h"
#include "Converter.h"
#include "ArgumentParser.h"

#include "fmt/format.h"
#include "fmt/xchar.h"
#include "fmt/ranges.h"

#include "boost/json.hpp"
#include "boost/json/parse.hpp"

#include <tuple>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

using namespace Utilities;

auto compress_folder() -> bool;
auto decompress_file() -> bool;
auto parse_arguments(ArgumentParser& arguments) -> void;
auto print_help_message() -> void;

#ifdef _DEBUG
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Sequence;
#else
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;
#endif

bool compress_mode = false;
bool decompress_mode = false;
std::string source_folder;
std::string destination_folder;
std::string compressed_file;
std::vector<std::string> file_extensions_ = {};

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	if (!compress_mode && !decompress_mode)
	{
		print_help_message();
		return 0;
	}

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("FolderCompression");

	bool success = false;
	if (compress_mode)
	{
		success = compress_folder();
	}
	else if (decompress_mode)
	{
		success = decompress_file();
	}

	Logger::handle().stop();
	Logger::destroy();

	if (!success)
	{
		print_help_message();
	}

	return 0;
}

auto compress_folder() -> bool
{
	if (source_folder.empty() || compressed_file.empty())
	{
		Logger::handle().write(LogTypes::Error, "Source folder and compressed file path are required for compression.");
		return false;
	}

	if (!std::filesystem::exists(source_folder))
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Source folder does not exist: {}", source_folder));
		return false;
	}

	auto [compress_condition, compress_message] = Folder::compression(compressed_file, source_folder, true, file_extensions_);
	if (!compress_condition)
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Cannot compress folder: {}", compress_message.value()));
		return false;
	}

	Logger::handle().write(LogTypes::Information, fmt::format("Successfully compressed folder to: {}", compressed_file));
	return true;
}

auto decompress_file() -> bool
{
	if (compressed_file.empty() || destination_folder.empty())
	{
		Logger::handle().write(LogTypes::Error, "Compressed file path and destination folder are required for decompression.");
		return false;
	}

	if (!std::filesystem::exists(compressed_file))
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Compressed file does not exist: {}", compressed_file));
		return false;
	}

	auto [decompress_condition, decompress_message] = Folder::decompression(destination_folder, compressed_file);
	if (!decompress_condition)
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Cannot decompress file: {}", decompress_message.value()));
		return false;
	}

	Logger::handle().write(LogTypes::Information, fmt::format("Successfully decompressed file to: {}", destination_folder));
	return true;
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

	auto array_target = arguments.to_array("--file_extensions");
	if (array_target != std::nullopt)
	{
		file_extensions_ = (std::vector<std::string>)array_target.value();
	}

	auto string_target = arguments.to_string("--source");
	if (string_target != std::nullopt)
	{
		source_folder = string_target.value();
	}

	string_target = arguments.to_string("--destination");
	if (string_target != std::nullopt)
	{
		destination_folder = string_target.value();
	}

	string_target = arguments.to_string("--compressed_file");
	if (string_target != std::nullopt)
	{
		compressed_file = string_target.value();
	}

	auto bool_target = arguments.to_bool("--compress");
	if (bool_target != std::nullopt)
	{
		compress_mode = bool_target.value();
	}

	bool_target = arguments.to_bool("--decompress");
	if (bool_target != std::nullopt)
	{
		decompress_mode = bool_target.value();
	}
}

auto print_help_message() -> void
{
	std::cout << "FolderCompression Usage:" << std::endl;
	std::cout << "  FolderCompression [options]" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  --compress                  Enable compression mode" << std::endl;
	std::cout << "  --decompress                Enable decompression mode" << std::endl;
	std::cout << "  --source <folder>           Source folder to compress (required for compression)" << std::endl;
	std::cout << "  --destination <folder>      Destination folder for decompression (required for decompression)" << std::endl;
	std::cout << "  --compressed_file <file>    Compressed file path (required for both modes)" << std::endl;
	std::cout << "  --write_console_log <level> Console log level (default: 5 in Debug, 3 in Release)" << std::endl;
	std::cout << "  --write_file_log <level>    File log level (default: 0 - None)" << std::endl;
	std::cout << "  --file_extensions <ext1,ext2,...> File extensions to include in compression (optional)" << std::endl;
	std::cout << std::endl;
	std::cout << "Examples:" << std::endl;
	std::cout << "  Compress a folder:" << std::endl;
	std::cout << "    FolderCompression --compress --source ./source_folder --compressed_file ./compressed.file" << std::endl;
	std::cout << std::endl;
	std::cout << "  Compress only specific file types:" << std::endl;
	std::cout << "    FolderCompression --compress --source ./source_folder --compressed_file ./compressed.file --file_extensions txt,json,xml" << std::endl;
	std::cout << std::endl;
	std::cout << "  Decompress a file:" << std::endl;
	std::cout << "    FolderCompression --decompress --compressed_file ./compressed.file --destination ./destination_folder" << std::endl;
	std::cout << std::endl;
	std::cout << "  Set console log level to Debug:" << std::endl;
	std::cout << "    FolderCompression --compress --source ./source_folder --compressed_file ./compressed.file --write_console_log 4" << std::endl;
	std::cout << std::endl;
	std::cout << "Note:" << std::endl;
	std::cout << "  - You must specify either --compress or --decompress mode." << std::endl;
	std::cout << "  - Make sure you have write permissions for the compressed file and destination folder." << std::endl;
}