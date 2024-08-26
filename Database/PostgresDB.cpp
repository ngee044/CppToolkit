#include "PostgresDB.h"

#include "Logger.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

#include <regex>
#include <sstream>

using namespace Utilities;

namespace Database
{
	PostgresDB::PostgresDB(const std::string& conn_str) : connection_(NULL)
	{
		connection_ = PQconnectdb(conn_str.c_str());
		if (PQstatus(connection_) != CONNECTION_OK)
		{
			Logger::handle().write(LogTypes::Error, fmt::format("cannot create PGconn: {}", PQerrorMessage(connection_)));
		}
	}

	PostgresDB::~PostgresDB() { PQfinish(connection_); }

	auto PostgresDB::execute_query(const std::string& sql_query) -> std::tuple<bool, std::optional<std::string>>
	{
		if (PQstatus(connection_) != CONNECTION_OK)
		{
			return { false, fmt::format("there is no created PGconn: {}", PQerrorMessage(connection_)) };
		}

		PGresult* result = PQexec(connection_, sql_query.c_str());
		if (PQresultStatus(result) != PGRES_COMMAND_OK)
		{
			std::string error = PQerrorMessage(connection_);
			PQclear(result);

			return { false, error };
		}
		PQclear(result);

		return { true, std::nullopt };
	}

	auto PostgresDB::execute_query_and_get_result(const std::string& sql_query)
		-> std::tuple<std::optional<std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>>>, std::optional<std::string>>
	{
		if (PQstatus(connection_) != CONNECTION_OK)
		{
			return { std::nullopt, fmt::format("there is no created PGconn: {}", PQerrorMessage(connection_)) };
		}

		PGresult* result = PQexec(connection_, sql_query.c_str());
		if (PQresultStatus(result) != PGRES_TUPLES_OK)
		{
			std::string error = PQerrorMessage(connection_);
			PQclear(result);

			return { std::nullopt, error };
		}

		int field_count = PQnfields(result);
		int row_count = PQntuples(result);
		std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>> result_data;
		for (int row_index = 0; row_index < row_count; ++row_index)
		{
			std::vector<std::variant<int, double, std::string, std::vector<std::string>>> current_row;
			for (int field_index = 0; field_index < field_count; ++field_index)
			{
				if (PQgetisnull(result, row_index, field_index))
				{
					current_row.emplace_back("");
				}

				Oid fieldType = PQftype(result, field_index);
				char* fieldValue = PQgetvalue(result, row_index, field_index);

				switch (fieldType)
				{
				case 23:
					current_row.emplace_back(std::stoi(fieldValue));
					break;
				case 700:
					current_row.emplace_back(std::stof(fieldValue));
					break;
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
		PQclear(result);

		return { result_data, fmt::format("there are selected rows: {}", result_data.size()) };
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