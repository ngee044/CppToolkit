#include "OptimizedDBConnectionPool.h"
#include <stdexcept>

namespace GameDatabase::Optimization
{
    OptimizedDBConnectionPool::OptimizedDBConnectionPool()
        : environment_(SQL_NULL_HENV)
    {
        // Initialize ODBC environment
        if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment_) != SQL_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate ODBC environment handle");
        }
        
        if (SQLSetEnvAttr(environment_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0) != SQL_SUCCESS)
        {
            SQLFreeHandle(SQL_HANDLE_ENV, environment_);
            throw std::runtime_error("Failed to set ODBC version");
        }
    }
    
    OptimizedDBConnectionPool::~OptimizedDBConnectionPool()
    {
        clear();
        
        if (environment_ != SQL_NULL_HENV)
        {
            SQLFreeHandle(SQL_HANDLE_ENV, environment_);
        }
    }
    
    auto OptimizedDBConnectionPool::connect(std::int32_t connection_count, const std::wstring& connection_string) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        connection_string_ = connection_string;
        
        // Create connections
        for (std::int32_t i = 0; i < connection_count; ++i)
        {
            auto connection = create_connection();
            if (!connection)
            {
                return {false, "Failed to create connection"};
            }
            
            available_connections_.push(connection);
            total_connections_.fetch_add(1, std::memory_order_relaxed);
        }
        
        return {true, std::nullopt};
    }
    
    auto OptimizedDBConnectionPool::clear() -> void
    {
        // Drain all connections
        while (auto connection = available_connections_.pop())
        {
            // Connection will be automatically closed when shared_ptr is destroyed
        }
        
        total_connections_.store(0, std::memory_order_relaxed);
        active_connections_.store(0, std::memory_order_relaxed);
    }
    
    auto OptimizedDBConnectionPool::acquire() -> std::shared_ptr<DBConnection>
    {
        // Try to get from pool first
        if (auto connection = available_connections_.pop())
        {
            // Verify connection is still valid
            if ((*connection)->is_valid())
            {
                active_connections_.fetch_add(1, std::memory_order_relaxed);
                return *connection;
            }
            
            // Connection is invalid, create a new one
            total_connections_.fetch_sub(1, std::memory_order_relaxed);
        }
        
        // Create new connection if pool was empty or connection was invalid
        auto new_connection = create_connection();
        if (new_connection)
        {
            active_connections_.fetch_add(1, std::memory_order_relaxed);
            total_connections_.fetch_add(1, std::memory_order_relaxed);
        }
        
        return new_connection;
    }
    
    auto OptimizedDBConnectionPool::release(std::shared_ptr<DBConnection> connection) -> void
    {
        if (!connection)
        {
            return;
        }
        
        active_connections_.fetch_sub(1, std::memory_order_relaxed);
        
        // Return to pool if connection is still valid
        if (connection->is_valid())
        {
            available_connections_.push(connection);
        }
        else
        {
            // Connection is invalid, don't return to pool
            total_connections_.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    
    auto OptimizedDBConnectionPool::create_connection() -> std::shared_ptr<DBConnection>
    {
        auto connection = std::make_shared<DBConnection>();
        
        auto [success, error] = connection->connect(environment_, connection_string_);
        if (!success)
        {
            return nullptr;
        }
        
        return connection;
    }
    
} // namespace GameDatabase::Optimization
