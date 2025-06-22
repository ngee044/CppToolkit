#include "DBConnectionPool.h"

#include <fmt/format.h>
#include <fmt/xchar.h>
#include "Logger.h"

using namespace Utilities;

namespace GameDatabase
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
		
		Logger::handle().write(LogTypes::Information, "Clearing database connection pool");

		if (environment_ != SQL_NULL_HANDLE)
		{
			::SQLFreeHandle(SQL_HANDLE_ENV, environment_);
			environment_ = SQL_NULL_HANDLE;
			Logger::handle().write(LogTypes::Information, "Released environment handle");
		}

		for (auto& connection : connections_)
		{
			connection.reset();
		}		connections_.clear();
		
		Logger::handle().write(LogTypes::Information, 
			fmt::format("Connection pool cleared. Released {} connections", connections_.size()));
	}
	auto DBConnectionPool::pop() -> std::shared_ptr<DBConnection>
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (connections_.empty())
		{
			Logger::handle().write(LogTypes::Error, "Connection pool is empty");
			return nullptr;
		}

		auto connection = connections_.back();
		connections_.pop_back();
		
		Logger::handle().write(LogTypes::Information, 
			fmt::format("Connection retrieved from pool. Remaining connections: {}", connections_.size()));

		return connection;
	}
	auto DBConnectionPool::push(std::shared_ptr<DBConnection> connection) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (connection)
		{
			connections_.push_back(connection);
			Logger::handle().write(LogTypes::Information, 
				fmt::format("Connection returned to pool. Total connections: {}", connections_.size()));
		}
		else
		{
			Logger::handle().write(LogTypes::Error, "Attempted to push null connection to pool");
			throw std::invalid_argument("Cannot push a null connection.");
		}
	}
	auto DBConnectionPool::connect(std::int32_t connection_count, const std::wstring& connection_string) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		
		Logger::handle().write(LogTypes::Information, 
			fmt::format("Initializing connection pool with {} connections", connection_count));

		if (::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment_) != SQL_SUCCESS)
		{
			Logger::handle().write(LogTypes::Error, "Failed to allocate environment handle");
			return { false, "Failed to allocate environment handle." };
		}

		if (::SQLSetEnvAttr(environment_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0) != SQL_SUCCESS)
		{
			Logger::handle().write(LogTypes::Error, "Failed to set ODBC version");
			return { false, "Failed to set ODBC version." };
		}

		for (auto i = 0; i < connection_count; ++i)
		{
			auto connection = std::make_shared<DBConnection>();
			auto [success, error] = connection->connect(environment_, connection_string);
			if (!success)
			{
				Logger::handle().write(LogTypes::Error, 
					fmt::format("Failed to connect to database (connection {}): {}", i + 1, error.value()));
				return { false, fmt::format("Failed to connect: {}", error.value()) };
			}
			connections_.push_back(connection);
		}
		
		Logger::handle().write(LogTypes::Information, 
			fmt::format("Connection pool initialized successfully with {} connections", connection_count));
		return { true, std::nullopt };
	}
}