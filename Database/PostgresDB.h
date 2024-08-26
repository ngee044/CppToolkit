#pragma once

#include "Database.h"
#include "libpq-fe.h"

namespace Database
{
	class PostgresDB : public DatabaseInterface
	{
	public:
		PostgresDB(const std::string& conn_str);
		virtual ~PostgresDB();

		auto execute_query(const std::string& sql_query) -> std::tuple<bool, std::optional<std::string>> override;
		auto execute_query_and_get_result(const std::string& sql_query)
			-> std::tuple<std::optional<std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>>>, std::optional<std::string>> override;

	protected:
		auto parse_postgres_array(const std::string& array_string) const -> std::vector<std::string>;

	private:
		PGconn* connection_;
	};
}