#include "DBConnectionPool.h"
#include <Logger.h>
#include <Converter.h>
#include "Job.h"

namespace GameDatabase
{
    DBConnectionPool::DBConnectionPool()
        : active_connections_(0)
        , connection_timeout_(std::chrono::seconds(30))
        , total_queries_(0)
        , failed_queries_(0)
    {
    }

    DBConnectionPool::~DBConnectionPool()
    {
        clear();
    }

    auto DBConnectionPool::connect(std::int32_t connection_count, const std::wstring& connection_string) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);

        connection_string_ = connection_string;

        // Create environment handle
        if (SQL_SUCCESS != ::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment_))
        {
            return { false, "Failed to allocate environment handle" };
        }

        if (SQL_SUCCESS != ::SQLSetEnvAttr(environment_, SQL_ATTR_ODBC_VERSION, 
            reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0))
        {
            ::SQLFreeHandle(SQL_HANDLE_ENV, environment_);
            return { false, "Failed to set ODBC version" };
        }

        // Create connections
        for (std::int32_t i = 0; i < connection_count; i++)
        {
            auto connection = std::make_shared<DBConnection>();
            auto [connect_success, connect_error] = connection->connect(environment_, connection_string);
            if (!connect_success)
            {
                return { false, "Failed to create connection #" + std::to_string(i) + ": " + 
                         (connect_error ? *connect_error : "Unknown error") };
            }

            connections_.push_back(connection);
            available_connections_.push_back(connection);
        }

        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "DBConnectionPool initialized with " + std::to_string(connection_count) + " connections");

        return { true, std::nullopt };
    }

    auto DBConnectionPool::clear() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);

        available_connections_.clear();
        connections_.clear();

        if (environment_ != SQL_NULL_HANDLE)
        {
            ::SQLFreeHandle(SQL_HANDLE_ENV, environment_);
            environment_ = SQL_NULL_HANDLE;
        }
    }

    auto DBConnectionPool::pop() -> std::shared_ptr<DBConnection>
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait for available connection with timeout
        if (!connection_available_.wait_for(lock, connection_timeout_, 
            [this] { return !available_connections_.empty(); }))
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                "Timeout waiting for database connection");
            return nullptr;
        }

        auto connection = available_connections_.back();
        available_connections_.pop_back();
        active_connections_++;

        return connection;
    }

    auto DBConnectionPool::push(std::shared_ptr<DBConnection> connection) -> void
    {
        if (!connection)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Check if connection is still valid
            if (!connection->is_valid())
            {
                Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                    "Returning invalid connection to pool, attempting to recreate");
                
                // Try to recreate the connection
                auto [new_connection, error] = recreate_connection(connection);
                if (new_connection)
                {
                    connection = new_connection;
                }
                else
                {
                    Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                        "Failed to recreate connection: " + error.value_or("Unknown error"));
                    active_connections_--;
                    return;
                }
            }

            available_connections_.push_back(connection);
            active_connections_--;
        }

        connection_available_.notify_one();
    }

    auto DBConnectionPool::set_thread_pool(std::shared_ptr<Thread::ThreadPool> thread_pool) -> void
    {
        thread_pool_ = thread_pool;
    }

    auto DBConnectionPool::execute_async(std::function<void(std::shared_ptr<DBConnection>)> operation,
                                        ExecuteCallback callback) -> void
    {
        if (!thread_pool_)
        {
            callback(false, "Thread pool not set");
            return;
        }

        auto job = std::make_shared<Thread::Job>(
            Thread::ThreadPriority::Normal,
            [this, operation, callback]() -> std::tuple<bool, std::optional<std::string>>
            {
                auto connection = pop();
                if (!connection)
                {
                    callback(false, "Failed to get connection from pool");
                    failed_queries_++;
                    return { false, "Failed to get connection from pool" };
                }

                try
                {
                    operation(connection);
                    push(connection);
                    callback(true, std::nullopt);
                    total_queries_++;
                    return { true, std::nullopt };
                }
                catch (const std::exception& e)
                {
                    push(connection);
                    callback(false, std::string("Database operation failed: ") + e.what());
                    failed_queries_++;
                    return { false, e.what() };
                }
            },
            "DBConnectionPool async operation"
        );
        
        thread_pool_->add_job(job);
    }

    auto DBConnectionPool::get_available_connections() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_connections_.size();
    }

    auto DBConnectionPool::get_total_connections() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }

    auto DBConnectionPool::get_active_connections() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_connections_;
    }

    auto DBConnectionPool::is_healthy() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return !connections_.empty() && active_connections_ < connections_.size();
    }

    auto DBConnectionPool::check_connections_health() -> std::tuple<size_t, size_t>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t healthy = 0;
        size_t unhealthy = 0;

        for (auto& conn : connections_)
        {
            if (conn && conn->is_valid())
            {
                healthy++;
            }
            else
            {
                unhealthy++;
            }
        }

        return { healthy, unhealthy };
    }

    auto DBConnectionPool::set_connection_timeout(std::chrono::seconds timeout) -> void
    {
        connection_timeout_ = timeout;
    }

    auto DBConnectionPool::get_connection_timeout() const -> std::chrono::seconds
    {
        return connection_timeout_;
    }

    auto DBConnectionPool::recreate_connection(std::shared_ptr<DBConnection> old_connection) 
        -> std::tuple<std::shared_ptr<DBConnection>, std::optional<std::string>>
    {
        try
        {
            auto new_connection = std::make_shared<DBConnection>();
            auto [connect_success, connect_error] = new_connection->connect(environment_, connection_string_);
            if (!connect_success)
            {
                return { nullptr, "Failed to create new connection: " + 
                         (connect_error ? *connect_error : "Unknown error") };
            }

            // Replace in connections vector
            auto it = std::find(connections_.begin(), connections_.end(), old_connection);
            if (it != connections_.end())
            {
                *it = new_connection;
            }

            return { new_connection, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { nullptr, std::string("Exception creating connection: ") + e.what() };
        }
    }
}
