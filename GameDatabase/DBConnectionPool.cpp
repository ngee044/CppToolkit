#include "DBConnectionPool.h"

#include <fmt/format.h>
#include <fmt/xchar.h>

namespace GameDataBase
{
	DBConnectionPool::DBConnectionPool()
	: environment_(SQL_NULL_HANDLE)
	{
	}

	DBConnectionPool::~DBConnectionPool()
	{
		clear();
	}

	auto DBConnectionPool::clear() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (environment_ != SQL_NULL_HANDLE)
		{
			::SQLFreeHandle(SQL_HANDLE_ENV, environment_);
			environment_ = SQL_NULL_HANDLE;
		}

		for (auto& connection : connections_)
		{
			connection.reset();
		}
		connections_.clear();
	}

	auto DBConnectionPool::pop() -> std::shared_ptr<DBConnection>
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (connections_.empty())
		{
			return nullptr;
		}

		auto connection = connections_.back();
		connections_.pop_back();

		return connection;
	}

	auto DBConnectionPool::push(std::shared_ptr<DBConnection> connection) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (connection)
		{
			connections_.push_back(connection);
		}
		else
		{
			throw std::invalid_argument("Cannot push a null connection.");
		}
	}

	auto DBConnectionPool::connect(std::int32_t connection_count, const std::wstring& connection_string) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment_) != SQL_SUCCESS)
		{
			return { false, "Failed to allocate environment handle." };
		}

		if (::SQLSetEnvAttr(environment_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0) != SQL_SUCCESS)
		{
			return { false, "Failed to set ODBC version." };
		}

		for (auto i = 0; i < connection_count; ++i)
		{
			auto connection = std::make_shared<DBConnection>();
			auto [success, error] = connection->connect(environment_, connection_string);
			if (!success)
			{
				return { false, fmt::format("Failed to connect: {}", error.value()) };
			}
			connections_.push_back(connection);
		}
		
		return { true, std::nullopt };
	}
} 