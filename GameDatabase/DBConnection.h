#pragma once
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#include <string_view>
#include <string>
#include <memory>
#include <tuple>
#include <optional>
#include <vector>

#include <mutex>

namespace GameDataBase
{
	class DBConnection
	{
	public:
		DBConnection();
		~DBConnection();

		auto connect(SQLHENV henv, const std::wstring& connection_string) -> std::tuple<bool, std::optional<std::string>>;
		auto clear() -> void;

		auto execute(const std::wstring& query) -> std::tuple<bool, std::optional<std::string>>;
		auto fetch() -> std::tuple<bool, std::optional<std::string>>;
		auto row_count() -> std::int32_t;
		auto unbind() -> void;

		auto bind_param(SQLUSMALLINT param_index, SQLSMALLINT c_type, SQLSMALLINT sql_types, SQLULEN length, SQLPOINTER ptr, SQLLEN* index)
			-> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(SQLUSMALLINT column_index, SQLSMALLINT c_type, SQLULEN length, SQLPOINTER value, SQLLEN* index)
			-> std::tuple<bool, std::optional<std::string>>;
		auto handle_error(SQLRETURN ret_code) -> void;

	private:
		SQLHDBC connection_;
		SQLHSTMT statement_;
	};
}