#include "DBConnection.h"

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <iostream>
#include <cstdint>

namespace GameDatabase
{
	DBConnection::DBConnection()
		: connection_(SQL_NULL_HANDLE),
		statement_(SQL_NULL_HANDLE)
	{
	}

	DBConnection::~DBConnection()
	{
		// Clean up the statement and connection handles
		if (statement_ != SQL_NULL_HANDLE)
		{
			SQLFreeHandle(SQL_HANDLE_STMT, statement_);
		}
		if (connection_ != SQL_NULL_HANDLE)
		{
			SQLDisconnect(connection_);
			SQLFreeHandle(SQL_HANDLE_DBC, connection_);
		}
	}

	auto DBConnection::connect(SQLHENV henv, const std::wstring& connection_string) -> std::tuple<bool, std::optional<std::string>>
	{
		if (::SQLAllocHandle(SQL_HANDLE_DBC, henv, &connection_) != SQL_SUCCESS)
		{
			return { false, "Failed to allocate connection handle."};
		}

		SQLSetConnectAttrW(connection_, SQL_LOGIN_TIMEOUT, reinterpret_cast<SQLPOINTER>(5), 0);

		SQLWCHAR out_conn_str[1024] = {};
		SQLSMALLINT out_conn_len = 0;

		SQLRETURN ret_code = SQLDriverConnectW(
			connection_,
			nullptr,
			const_cast<SQLWCHAR*>(connection_string.c_str()),
			SQL_NTSL,
			out_conn_str,
			static_cast<SQLSMALLINT>(std::size(out_conn_str)),
			&out_conn_len,
			SQL_DRIVER_COMPLETE);

		if (!SQL_SUCCEEDED(ret_code))
		{
			// TODO
			// Handle error
			handle_error(ret_code);
			return { false, "Failed to connect to the database." };
		}

		if (::SQLAllocHandle(SQL_HANDLE_STMT, connection_, &statement_) != SQL_SUCCESS)
		{
			return { false, "Failed to allocate statement handle." };
		}

		return { true, std::nullopt };

	}	
	auto DBConnection::clear() -> void
	{
		if (connection_ != SQL_NULL_HANDLE)
		{
			SQLDisconnect(connection_);
			SQLFreeHandle(SQL_HANDLE_DBC, connection_);
			connection_ = SQL_NULL_HANDLE;
		}

		if (statement_ != SQL_NULL_HANDLE)
		{
			SQLFreeHandle(SQL_HANDLE_STMT, statement_);
			statement_ = SQL_NULL_HANDLE;
		}
	}

	auto DBConnection::execute(const std::wstring& query) -> std::tuple<bool, std::optional<std::string>>
	{
		if (statement_ == SQL_NULL_HANDLE)
		{
			return { false, "Statement handle is not initialized." };
		}

		auto ret = ::SQLExecDirectW(statement_, const_cast<SQLWCHAR*>(query.c_str()), SQL_NTSL);
		if (!SQL_SUCCEEDED(ret))
		{
			handle_error(ret);
			return { false, "Failed to execute query." };
		}

		return { true, std::nullopt };
	}

	auto DBConnection::fetch() -> std::tuple<bool, std::optional<std::string>>
	{
		SQLRETURN ret = SQLFetch(statement_);

		switch (ret)
		{
			case SQL_SUCCESS:
			case SQL_SUCCESS_WITH_INFO:
				return { true, std::nullopt };
			case SQL_NO_DATA:
				return { false, "No more data to fetch." };
			default:
				handle_error(ret);
				return { false, "Failed to fetch data." };
		}

		return { false, "Unknown error during fetch." };
	}

	auto DBConnection::row_count() -> std::int32_t
	{
		SQLLEN count = 0;
		SQLRETURN ret = SQLRowCount(statement_, &count);

		if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
		{
			handle_error(ret);
			return -1; // Indicating an error
		}

		return static_cast<std::int32_t>(count);
	}

	auto DBConnection::unbind() -> void
	{
		::SQLFreeStmt(statement_, SQL_UNBIND);
		::SQLFreeStmt(statement_, SQL_RESET_PARAMS);
		::SQLFreeStmt(statement_, SQL_CLOSE);
	}

	auto DBConnection::bind_param(std::int32_t param_index, bool* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_param(param_index, SQL_C_TINYINT, SQL_TINYINT, sizeof(bool), value, index);
	}

	auto DBConnection::bind_param(std::int32_t param_index, std::int8_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_param(param_index, SQL_C_TINYINT, SQL_TINYINT, sizeof(std::int8_t), value, index);
	}

	auto DBConnection::bind_param(std::int32_t param_index, std::int16_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_param(param_index, SQL_C_SHORT, SQL_SMALLINT, sizeof(std::int16_t), value, index);
	}

	auto DBConnection::bind_param(std::int32_t param_index, std::int32_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_param(param_index, SQL_C_LONG, SQL_INTEGER, sizeof(std::int32_t), value, index);
	}

	auto DBConnection::bind_param(std::int32_t param_index, std::int64_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_param(param_index, SQL_C_SBIGINT, SQL_BIGINT, sizeof(std::int64_t), value, index);
	}

	auto DBConnection::bind_param(std::int32_t param_index, float* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_param(param_index, SQL_C_FLOAT, SQL_REAL, 0, value, index);	
	}
	
	auto DBConnection::bind_param(std::int32_t param_index, double* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_param(param_index, SQL_C_DOUBLE, SQL_DOUBLE, 0, value, index);
	}

	auto DBConnection::bind_param(std::int32_t param_index, TIMESTAMP_STRUCT* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_param(param_index, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, sizeof(TIMESTAMP_STRUCT), value, index);
	}

	auto DBConnection::bind_param(std::int32_t param_index, WCHAR* str, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		SQLULEN size = static_cast<SQLULEN>((::wcslen(str) + 1) * 2);
		*index = SQL_NTSL;

		if (size > WVARCHAR_MAX)
		{
			return bind_param(param_index, SQL_C_WCHAR, SQL_WLONGVARCHAR, size, (SQLPOINTER)str, index);
		}
		else
		{
			return bind_param(param_index, SQL_C_WCHAR, SQL_WVARCHAR, size, (SQLPOINTER)str, index);
		}
	}

	auto DBConnection::bind_param(std::int32_t param_index, BYTE* bin, std::int32_t size, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		if (bin == nullptr)
		{
			*index = SQL_NULL_DATA;
			size = 1;
		}
		else
		{
			*index = static_cast<SQLLEN>(size);
		}

		if (size > WVARCHAR_MAX)
		{
			return bind_param(param_index, SQL_C_BINARY, SQL_LONGVARBINARY, size, (BYTE*)bin, index);
		}
		else
		{
			return bind_param(param_index, SQL_C_BINARY, SQL_BINARY, size, (BYTE*)bin, index);
		}
	}

	auto DBConnection::bind_column(std::int32_t column_index, bool* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_TINYINT, sizeof(bool), value, index);
	}

	auto DBConnection::bind_column(std::int32_t column_index, std::int8_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_TINYINT, sizeof(std::int8_t), value, index);
	}

	auto DBConnection::bind_column(std::int32_t column_index, std::int16_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_SHORT, sizeof(std::int16_t), value, index);
	}

	auto DBConnection::bind_column(std::int32_t column_index, std::int32_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_LONG, sizeof(std::int32_t), value, index);
	}

	auto DBConnection::bind_column(std::int32_t column_index, std::int64_t* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_SBIGINT, sizeof(std::int64_t), value, index);
	}

	auto DBConnection::bind_column(std::int32_t column_index, float* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_FLOAT, sizeof(float), value, index);
	}

	auto DBConnection::bind_column(std::int32_t column_index, double* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_DOUBLE, sizeof(double), value, index);
	}
	
	auto DBConnection::bind_column(std::int32_t column_index, TIMESTAMP_STRUCT* value, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_TYPE_TIMESTAMP, sizeof(TIMESTAMP_STRUCT), value, index);
	}

	auto DBConnection::bind_column(std::int32_t column_index, WCHAR* str, std::int32_t size, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{
		return bind_column(column_index, SQL_C_WCHAR, size, str, index);
	}

	auto DBConnection::bind_column(std::int32_t column_index, BYTE* bin, std::int32_t size, SQLLEN* index) -> std::tuple<bool, std::optional<std::string>>
	{

		return bind_column(column_index, SQL_BINARY, size, bin, index);
	}

	/*protected*/
	auto DBConnection::bind_param(SQLUSMALLINT param_index, SQLSMALLINT c_type, SQLSMALLINT sql_types, SQLULEN length, SQLPOINTER ptr, SQLLEN* index)
		-> std::tuple<bool, std::optional<std::string>>
	{
		auto ret = ::SQLBindParameter(
			statement_,
			param_index,
			SQL_PARAM_INPUT,
			c_type,
			sql_types,
			length,
			0, // Parameter scale
			ptr,
			0, // Buffer length
			index);

		if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
		{
			handle_error(ret);
			return { false, "Failed to bind parameter." };
		}

		return { true, std::nullopt };
	}

	auto DBConnection::bind_column(SQLUSMALLINT column_index, SQLSMALLINT c_type, SQLULEN length, SQLPOINTER value, SQLLEN* index)
		-> std::tuple<bool, std::optional<std::string>>
	{
		auto ret = ::SQLBindCol(
			statement_,
			column_index,
			c_type,
			value,
			length,
			index);

		if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
		{
			handle_error(ret);
			return { false, "Failed to bind parameter." };
		}

		return { true, std::nullopt };
	}

	auto DBConnection::handle_error(SQLRETURN ret_code) -> void
	{
		if (ret_code == SQL_SUCCESS)
		{
			return;
		}

		SQLSMALLINT index = 1;
		SQLWCHAR sql_state[SQL_MAX_MESSAGE_LENGTH] = {};
		SQLINTEGER native_error = 0;
		SQLWCHAR message[SQL_MAX_MESSAGE_LENGTH] = {};
		SQLSMALLINT message_length = 0;
		SQLRETURN error_ret = 0;

		while (true)
		{
			auto error_ret = ::SQLGetDiagRecW(
					SQL_HANDLE_STMT,
					statement_,
					index,
					sql_state,
					&native_error,
					message,
					SQL_MAX_MESSAGE_LENGTH,
					&message_length);
			
			if (error_ret == SQL_NO_DATA)
			{
				break; // No more error records
			}
			else if (!SQL_SUCCEEDED(error_ret))
			{
				break; // Error retrieving error record
			}

			// TODO
			// Log the error message
			std::wcout.imbue(std::locale("kor"));
			std::wcout << message << std::endl;

			index++;
		}
	}
} 