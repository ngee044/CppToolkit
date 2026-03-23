#pragma once

#include <string>
#include <vector>
#include <variant>
#include <tuple>
#include <optional>
#include <expected>

namespace Database
{
	template <typename... Ts> struct column_overload : Ts...
	{
		using Ts::operator()...;
	};

	template <typename... Ts> column_overload(Ts...) -> column_overload<Ts...>;

	class DatabaseInterface
	{
	public:
		DatabaseInterface() = default;
		virtual ~DatabaseInterface() = default;

		virtual auto execute_query(const std::string& sql) -> std::expected<void, std::string> = 0;
		virtual auto execute_query_and_get_result(const std::string& sql)
			-> std::expected<std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>>, std::string>
			= 0;
	};
}