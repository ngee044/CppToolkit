#pragma once

#ifdef _WIN32
	#include <windows.h>
	#include <sql.h>
	#include <sqlext.h>
	#include <sqltypes.h>
#elif __APPLE__
	#include <sql.h>
	#include <sqlext.h>
	#include <sqltypes.h>
#else
	#include <sql.h>
	#include <sqlext.h>
	#include <sqltypes.h>
#endif

#include <string_view>
#include <string>
#include <memory>
#include <tuple>
#include <optional>
#include <vector>
#include <type_traits>
#include <cstdint>

#include <mutex>

// 플랫폼별 타입 정의
#ifndef _WIN32
	typedef wchar_t WCHAR;
	typedef unsigned char BYTE;
#endif

namespace GameDatabase
{
	enum
	{
		WVARCHAR_MAX = 4000,
		BINARY_MAX = 8000
	};

	class DBConnection
	{
	public:
		DBConnection();
		~DBConnection();
		auto connect(SQLHENV henv, const std::wstring& connection_string) -> std::tuple<bool, std::optional<std::string>>;
		auto clear() -> void;
		auto prepare(const std::wstring& query) -> std::tuple<bool, std::optional<std::string>>;
		auto execute(const std::wstring& query) -> std::tuple<bool, std::optional<std::string>>;
		auto fetch() -> std::tuple<bool, std::optional<std::string>>;
		auto row_count() -> std::int32_t;
		auto unbind() -> void;
		
		// Data retrieval method
		auto get_data(std::int32_t column_index, std::int32_t c_type, void* buffer, std::int32_t buffer_size, SQLLEN* indicator) 
			-> std::tuple<bool, std::optional<std::string>>;
		
		// Additional methods for compatibility
		auto is_valid() const -> bool { return connection_ != SQL_NULL_HANDLE && statement_ != SQL_NULL_HANDLE; }
		auto is_null() const -> bool { return !is_valid(); }
		auto get_statement_handle() const -> SQLHSTMT { return statement_; }
		auto get_connection_handle() const -> SQLHDBC { return connection_; }
		auto get_affected_rows() -> std::int32_t { return row_count(); }
		
		// Template method for getting typed data from result set
		template<typename T>
		auto get_column_data(std::int32_t column_index) -> std::tuple<T, std::optional<std::string>>
		{
			static_assert(std::is_arithmetic_v<T> || std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring>, 
				"Type must be arithmetic, string, or wstring");
			
			T result{};
			SQLLEN indicator = 0;
			SQLRETURN ret = SQL_ERROR;
			
			if constexpr (std::is_same_v<T, std::int32_t>)
			{
				ret = SQLGetData(statement_, column_index, SQL_C_SLONG, &result, sizeof(result), &indicator);
			}
			else if constexpr (std::is_same_v<T, std::int64_t>)
			{
				ret = SQLGetData(statement_, column_index, SQL_C_SBIGINT, &result, sizeof(result), &indicator);
			}
			else if constexpr (std::is_same_v<T, double>)
			{
				ret = SQLGetData(statement_, column_index, SQL_C_DOUBLE, &result, sizeof(result), &indicator);
			}
			else if constexpr (std::is_same_v<T, std::wstring>)
			{
				WCHAR buffer[4096] = {};
				ret = SQLGetData(statement_, column_index, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
				if (SQL_SUCCEEDED(ret))
				{
					result = std::wstring(buffer);
				}
			}
			
			if (!SQL_SUCCEEDED(ret))
			{
				return { T{}, "Failed to get column data" };
			}
			
			return { result, std::nullopt };
		}

		auto bind_param(std::int32_t param_index, bool* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, std::int8_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, std::int16_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, std::int32_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, std::int64_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, float* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, double* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, TIMESTAMP_STRUCT* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, WCHAR* str, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_param(std::int32_t param_index, BYTE* bin, std::int32_t size, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;

		auto bind_column(std::int32_t column_index, bool* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, std::int8_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, std::int16_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, std::int32_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, std::int64_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, float* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, double* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, TIMESTAMP_STRUCT* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, WCHAR* str, std::int32_t size, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(std::int32_t column_index, BYTE* bin, std::int32_t size, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;

		// get_data 메소드 추가
		template<typename T>
		auto get_data(std::int32_t column_index) -> std::tuple<bool, std::optional<T>, std::optional<std::string>>;

	protected:
		auto bind_param(SQLUSMALLINT param_index, SQLSMALLINT c_type, SQLSMALLINT sql_types, SQLULEN length, SQLPOINTER ptr, SQLLEN* index)
			-> std::tuple<bool, std::optional<std::string>>;
		auto bind_column(SQLUSMALLINT column_index, SQLSMALLINT c_type, SQLULEN length, SQLPOINTER value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>;
		auto handle_error(SQLRETURN ret_code) -> void;

	private:
		SQLHDBC connection_;
		SQLHSTMT statement_;
	};
}