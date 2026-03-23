#include <iostream>

#include "File.h"
#include "Logger.h"
#include "Converter.h"
#include "PostgresDB.h"
#include "ArgumentParser.h"

#include <format>
#include <expected>

#include <string>
#include <vector>

using namespace Utilities;
using namespace Database;

auto parse_arguments(ArgumentParser& arguments) -> void;
auto read_query_from_file(const std::string& filename) -> std::tuple<std::optional<std::string>, std::optional<std::string>>;
auto write_result_to_file(const std::string& filename, const std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>>& rows) -> void;
auto print_help_message() -> void;

#ifdef _DEBUG
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Sequence;
#else
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;
#endif

std::string user_name_ = "";
std::string db_name_ = "";
std::string db_password_ = "";
std::string host_ = "localhost";
int port_ = 5433;
std::string query_;
std::string query_file_;
std::string output_file_;
std::string operation_ = "select";

std::shared_ptr<DatabaseInterface> database_ = nullptr;

auto join_strings(const std::vector<std::string>& values) -> std::string
{
	std::string result;
	for (const auto& value : values)
	{
		if (!result.empty())
		{
			result += ", ";
		}

		result += value;
	}

	return result;
}

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	if (user_name_.empty() || db_name_.empty() || db_password_.empty())
	{
		std::cout << "Error: Invalid database connection details." << std::endl;
		print_help_message();
		return 0;
	}

	if (host_.empty() || port_ == 0)
	{
		std::cout << "Error: Invalid host or port." << std::endl;
		print_help_message();
		return 0;
	}

	if (operation_ != "select" && operation_ != "insert" && operation_ != "update" && operation_ != "delete")
	{
		std::cout << "Error: Invalid operation type." << std::endl;
		print_help_message();
		return 0;
	}

	if (query_.empty() && query_file_.empty())
	{
		std::cout << "Error: No query specified." << std::endl;
		print_help_message();
		return 0;
	}

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("DatabaseClient");

	database_ = std::make_shared<PostgresDB>(std::format("user={} dbname={} password={} host={} port={}", user_name_, db_name_, db_password_, host_, port_));

	if (!query_file_.empty())
	{
		auto [query, error] = read_query_from_file(query_file_);
		if (query.has_value())
		{
			query_ = query.value();
		}
		else
		{
			Logger::handle().write(LogTypes::Error, error.value());
		}
	}

	if (query_.empty())
	{
		Logger::handle().write(LogTypes::Error, "No query specified. Use --query or --query_file option.");
		Logger::handle().stop();
		Logger::destroy();

		return 0;
	}

	if (operation_ == "select")
	{
		auto query_result = database_->execute_query_and_get_result(query_);
		if (!query_result)
		{
			Logger::handle().write(LogTypes::Error, std::format("Query execution failed: {}", query_result.error()));
		}
		else
		{
			for (const auto& row : query_result.value())
			{
				std::string row_string;
				for (const auto& column : row)
				{
					std::visit(
						[&](const auto& value)
						{
							if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::vector<std::string>>)
							{
								row_string += std::format("{{{}}}", join_strings(value));
							}
							else
							{
								row_string += std::format("{} ", value);
							}
						},
						column);
				}
				Logger::handle().write(LogTypes::Information, std::format("Row: {}", row_string));
			}

			if (!output_file_.empty())
			{
				write_result_to_file(output_file_, query_result.value());
			}
		}
	}
	else
	{
		auto query_result = database_->execute_query(query_);
		if (!query_result)
		{
			Logger::handle().write(LogTypes::Error, std::format("Query execution failed: {}", query_result.error()));
		}
		else
		{
			Logger::handle().write(LogTypes::Information, "Query executed successfully.");
		}
	}

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

	int_target = arguments.to_int("--port");
	if (int_target != std::nullopt)
	{
		port_ = int_target.value();
	}

	auto string_target = arguments.to_string("--user");
	if (string_target != std::nullopt)
	{
		user_name_ = string_target.value();
	}

	string_target = arguments.to_string("--dbname");
	if (string_target != std::nullopt)
	{
		db_name_ = string_target.value();
	}

	string_target = arguments.to_string("--password");
	if (string_target != std::nullopt)
	{
		db_password_ = string_target.value();
	}

	string_target = arguments.to_string("--host");
	if (string_target != std::nullopt)
	{
		host_ = string_target.value();
	}

	string_target = arguments.to_string("--query");
	if (string_target != std::nullopt)
	{
		query_ = string_target.value();
	}

	string_target = arguments.to_string("--query_file");
	if (string_target != std::nullopt)
	{
		query_file_ = string_target.value();
	}

	string_target = arguments.to_string("--output_file");
	if (string_target != std::nullopt)
	{
		output_file_ = string_target.value();
	}

	string_target = arguments.to_string("--operation");
	if (string_target != std::nullopt)
	{
		operation_ = string_target.value();
	}
}

auto read_query_from_file(const std::string& filename) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
{
	File file;
	auto open_result = file.open(filename, std::ios::binary | std::ios::in);
	if (!open_result)
	{
		return { std::nullopt, std::format("Failed to open query file: {}", filename) };
	}

	auto [bytes, read_error] = file.read_bytes();
	if (!bytes.has_value())
	{
		return { std::nullopt, std::format("Failed to read query file: {}", *read_error) };
	}

	return { Converter::to_string(bytes.value()), std::nullopt };
}

auto write_result_to_file(const std::string& filename, const std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>>& rows) -> void
{
	File file;
	auto open_result = file.open(filename, std::ios::binary | std::ios::out);
	if (!open_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("Failed to open output file: {}", filename));
		return;
	}

	for (const auto& row : rows)
	{
		std::string row_string;
		for (const auto& column : row)
		{
			std::visit(
				[&](const auto& value)
				{
					if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::vector<std::string>>)
					{
						row_string += std::format("{{{}}}", join_strings(value));
					}
					else
					{
						row_string += std::format("{} ", value);
					}
				},
				column);
		}
		file.write_lines(std::vector<std::string>{ row_string }, true);
	}
	file.close();

	Logger::handle().write(LogTypes::Information, std::format("Results written to file: {}", filename));
}

auto print_help_message() -> void
{
	std::cout << "Usage: DatabaseClient [OPTIONS]" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  --help                    Show this help message and exit" << std::endl;
	std::cout << "  --user <username>         Database user name" << std::endl;
	std::cout << "  --dbname <dbname>         Database name" << std::endl;
	std::cout << "  --password <password>     Database password" << std::endl;
	std::cout << "  --host <hostname>         Database host (default: localhost)" << std::endl;
	std::cout << "  --port <port>             Database port (default: 5433)" << std::endl;
	std::cout << "  --query <query>           SQL query to execute" << std::endl;
	std::cout << "  --query_file <filename>   File containing SQL query to execute" << std::endl;
	std::cout << "  --operation <operation>   Operation type: select, insert, update, delete (default: select)" << std::endl;
	std::cout << "  --output_file <filename>  File to write query results (for select operations)" << std::endl;
	std::cout << "  --write_console_log <level> Console log level (default: 4 in Debug, 2 in Release)" << std::endl;
	std::cout << "  --write_file_log <level>  File log level (default: 0)" << std::endl;
	std::cout << std::endl;
	std::cout << "Note: At least one of --query or --query_file must be provided." << std::endl;
	std::cout << "      Database connection details (user, dbname, password, host, port) are required." << std::endl;
	std::cout << std::endl;
	std::cout << "Examples:" << std::endl;
	std::cout << "1. Execute a SELECT query and display results:" << std::endl;
	std::cout << "   DatabaseClient --user myuser --dbname mydb --password mypass --host myhost --port 5432 \\" << std::endl;
	std::cout << "                  --query \"SELECT * FROM mytable\"" << std::endl;
	std::cout << std::endl;
	std::cout << "2. Execute an INSERT query from a file:" << std::endl;
	std::cout << "   DatabaseClient --user myuser --dbname mydb --password mypass --host myhost --port 5432 \\" << std::endl;
	std::cout << "                  --query_file insert_data.sql --operation insert" << std::endl;
	std::cout << std::endl;
	std::cout << "3. Execute a SELECT query and save results to a file:" << std::endl;
	std::cout << "   DatabaseClient --user myuser --dbname mydb --password mypass --host myhost --port 5432 \\" << std::endl;
	std::cout << "                  --query \"SELECT * FROM mytable\" --output_file results.txt" << std::endl;
	std::cout << std::endl;
	std::cout << "4. Execute an UPDATE query:" << std::endl;
	std::cout << "   DatabaseClient --user myuser --dbname mydb --password mypass --host myhost --port 5432 \\" << std::endl;
	std::cout << "                  --query \"UPDATE mytable SET column1 = 'new_value' WHERE id = 1\" --operation update" << std::endl;
	std::cout << std::endl;
	std::cout << "5. Execute a DELETE query:" << std::endl;
	std::cout << "   DatabaseClient --user myuser --dbname mydb --password mypass --host myhost --port 5432 \\" << std::endl;
	std::cout << "                  --query \"DELETE FROM mytable WHERE id = 1\" --operation delete" << std::endl;
}
