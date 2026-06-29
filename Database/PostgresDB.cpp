#include "PostgresDB.h"

#include "Logger.h"

#include <format>

#include <regex>
#include <sstream>
#include <charconv>
#include <memory>

using namespace Utilities;

namespace Database
{
	PostgresDB::PostgresDB(const std::string& conn_str) : connection_(nullptr)
	{
		connection_ = PQconnectdb(conn_str.c_str());
		if (PQstatus(connection_) != CONNECTION_OK)
		{
			Logger::handle().write(LogTypes::Error, std::format("cannot create PGconn: {}", PQerrorMessage(connection_)));
		}
	}

	PostgresDB::~PostgresDB() { PQfinish(connection_); }

	PostgresDB::PostgresDB(PostgresDB&& other) noexcept : connection_(other.connection_) { other.connection_ = nullptr; }

	auto PostgresDB::operator=(PostgresDB&& other) noexcept -> PostgresDB&
	{
		if (this != &other)
		{
			PQfinish(connection_);
			connection_ = other.connection_;
			other.connection_ = nullptr;
		}

		return *this;
	}

	auto PostgresDB::execute_query(const std::string& sql_query) -> std::expected<void, std::string>
	{
		if (!is_connected())
		{
			return std::unexpected(std::format("there is no created PGconn: {}", PQerrorMessage(connection_)));
		}

		PGresult* result = PQexec(connection_, sql_query.c_str());
		if (PQresultStatus(result) != PGRES_COMMAND_OK)
		{
			std::string error = PQerrorMessage(connection_);
			PQclear(result);

			return std::unexpected(error);
		}
		PQclear(result);

		return {};
	}

	auto PostgresDB::execute_query_and_get_result(const std::string& sql_query)
		-> std::expected<std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>>, std::string>
	{
		if (!is_connected())
		{
			return std::unexpected(std::format("there is no created PGconn: {}", PQerrorMessage(connection_)));
		}

		std::unique_ptr<PGresult, decltype(&PQclear)> result(PQexec(connection_, sql_query.c_str()), &PQclear);
		if (PQresultStatus(result.get()) != PGRES_TUPLES_OK)
		{
			return std::unexpected(std::string(PQerrorMessage(connection_)));
		}

		int field_count = PQnfields(result.get());
		int row_count = PQntuples(result.get());
		std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>> result_data;
		for (int row_index = 0; row_index < row_count; ++row_index)
		{
			std::vector<std::variant<int, double, std::string, std::vector<std::string>>> current_row;
			for (int field_index = 0; field_index < field_count; ++field_index)
			{
				if (PQgetisnull(result.get(), row_index, field_index))
				{
					current_row.emplace_back("");
					continue;
				}

				Oid fieldType = PQftype(result.get(), field_index);
				char* fieldValue = PQgetvalue(result.get(), row_index, field_index);

				switch (fieldType)
				{
				case 23:
				{
					int int_value = 0;
					const char* int_end = fieldValue + std::char_traits<char>::length(fieldValue);
					auto [ptr, ec] = std::from_chars(fieldValue, int_end, int_value);
					if (ec != std::errc{} || ptr != int_end)
					{
						return std::unexpected(std::format("failed to parse int4 value: {}", fieldValue));
					}
					current_row.emplace_back(int_value);
					break;
				}
				case 700:
				{
					try
					{
						current_row.emplace_back(std::stof(fieldValue));
					}
					catch (const std::exception&)
					{
						return std::unexpected(std::format("failed to parse float4 value: {}", fieldValue));
					}
					break;
				}
				case 1043:
					current_row.emplace_back(std::string(fieldValue));
					break;
				case 1009:
					current_row.emplace_back(parse_postgres_array(fieldValue));
					break;
				default:
					current_row.emplace_back(std::string(fieldValue));
				}
			}
			result_data.push_back(current_row);
		}

		return result_data;
	}

	auto PostgresDB::execute_command(const std::string& sql) -> std::expected<void, std::string>
	{
        if (!is_connected())
		{
			return std::unexpected(std::format("there is no created PGconn: {}", PQerrorMessage(connection_)));
		}

        PGresult* postgre_result = PQexec(connection_, sql.c_str());
        if (PQresultStatus(postgre_result) != PGRES_COMMAND_OK)
        {
            auto error_message = PQerrorMessage(connection_);
			std::string error = std::format("Error executing command: {}", error_message);
			Logger::handle().write(LogTypes::Error, error);
            PQclear(postgre_result);
            return std::unexpected(error);
        }

        PQclear(postgre_result);
        return {};
	}

	auto PostgresDB::escape_string(const std::string input) -> std::string
	{
		if (!connection_)
		{
			return input;
		}
        std::vector<char> buffer(input.size() * 2 + 1);
        int error = 0;
        size_t len = PQescapeStringConn(connection_, buffer.data(), input.c_str(), input.size(), &error);

        std::string escaped(buffer.data(), len);

        if (error != 0)
        {
			Logger::handle().write(LogTypes::Error, std::format("Error escaping string: {}", PQerrorMessage(connection_)) );
        }

        return escaped;
	}

	auto PostgresDB::parse_postgres_array(const std::string& array_string) const -> std::vector<std::string>
	{
		std::vector<std::string> elements;
		std::regex array_element_regex(R"(\{([^}]*)\})");
		std::smatch matches;

		if (std::regex_search(array_string, matches, array_element_regex) && matches.size() > 1)
		{
			std::string elements_string = matches[1].str();
			std::istringstream istringstream(elements_string);
			std::string element;

			while (std::getline(istringstream, element, ','))
			{
				element.erase(0, element.find_first_not_of(' '));
				element.erase(element.find_last_not_of(' ') + 1);
				elements.push_back(element);
			}
		}

		return elements;
	}
}