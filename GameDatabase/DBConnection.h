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
				// Additional methods for compatibility
		auto is_valid() const -> bool { return connection_ != SQL_NULL_HDBC && statement_ != SQL_NULL_HSTMT; }
		auto is_null() const -> bool { return !is_valid(); }
		auto get_data() -> void* { return static_cast<void*>(statement_); }
		auto get_affected_rows() -> std::int32_t { return row_count(); }
		
		// Template method for getting typed data
		template<typename T>
		auto get_data(int column_index) -> T
		{
			// This is a placeholder implementation
			// In a real implementation, you would extract data from the result set
			static_assert(std::is_default_constructible_v<T>, "Type must be default constructible");
			return T{};
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