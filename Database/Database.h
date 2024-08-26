#pragma once

#include <string>
#include <vector>
#include <variant>
#include <tuple>
#include <optional>

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

		virtual auto execute_query(const std::string& sql) -> std::tuple<bool, std::optional<std::string>> = 0;
		virtual auto execute_query_and_get_result(const std::string& sql)
			-> std::tuple<std::optional<std::vector<std::vector<std::variant<int, double, std::string, std::vector<std::string>>>>>, std::optional<std::string>>
			= 0;
	};
}