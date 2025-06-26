#pragma once

#include "DBConnection.h"
#include <ThreadPool.h>
#include <Job.h>
#include <functional>
#include <future>
#include <chrono>
#include <condition_variable>

namespace GameDatabase
{
    // Async query callback types
    template<typename T>
    using QueryCallback = std::function<void(T result, std::optional<std::string> error)>;
    
    using ExecuteCallback = std::function<void(bool success, std::optional<std::string> error)>;

    class DBConnectionPool
    {
    public:
        DBConnectionPool();
        ~DBConnectionPool();

        // Connection management
        auto connect(std::int32_t connection_count, const std::wstring& connection_string) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto clear() -> void;

        // Synchronous operations
        auto pop() -> std::shared_ptr<DBConnection>;
        auto push(std::shared_ptr<DBConnection> connection) -> void;

        // Asynchronous operations with ThreadPool
        auto set_thread_pool(std::shared_ptr<Thread::ThreadPool> thread_pool) -> void;
        
        template<typename ResultType>
        auto execute_async(std::function<ResultType(std::shared_ptr<DBConnection>)> operation,
                          QueryCallback<ResultType> callback) -> void;
        
        auto execute_async(std::function<void(std::shared_ptr<DBConnection>)> operation,
                          ExecuteCallback callback) -> void;

        // Pool statistics
        auto get_available_connections() const -> size_t;
        auto get_total_connections() const -> size_t;
        auto get_active_connections() const -> size_t;
        auto is_healthy() const -> bool;
        
        // Connection health check
        auto check_connections_health() -> std::tuple<size_t, size_t>; // returns (healthy, unhealthy)
        
        // Timeout configuration
        auto set_connection_timeout(std::chrono::seconds timeout) -> void;
        auto get_connection_timeout() const -> std::chrono::seconds;

    private:
        auto wait_for_connection(std::chrono::milliseconds timeout) 
            -> std::tuple<std::shared_ptr<DBConnection>, std::optional<std::string>>;
        auto recreate_connection(std::shared_ptr<DBConnection> old_connection) 
            -> std::tuple<std::shared_ptr<DBConnection>, std::optional<std::string>>;

    private:
        mutable std::mutex mutex_;
        std::condition_variable connection_available_;

        SQLHENV environment_;
        std::wstring connection_string_;
        std::vector<std::shared_ptr<DBConnection>> connections_;
        std::vector<std::shared_ptr<DBConnection>> available_connections_;
        size_t active_connections_;
        
        std::shared_ptr<Thread::ThreadPool> thread_pool_;
        std::chrono::seconds connection_timeout_;
        
        // Statistics
        std::atomic<uint64_t> total_queries_;
        std::atomic<uint64_t> failed_queries_;
    };

    // Template implementation
    template<typename ResultType>
    auto DBConnectionPool::execute_async(std::function<ResultType(std::shared_ptr<DBConnection>)> operation,
                                        QueryCallback<ResultType> callback) -> void
    {
        if (!thread_pool_)
        {
            callback(ResultType{}, "Thread pool not set");
            return;
        }

        auto job = std::make_shared<Thread::Job>(
            [this, operation, callback]() -> void
            {
                auto connection = pop();
                if (!connection)
                {
                    callback(ResultType{}, "Failed to get connection from pool");
                    failed_queries_++;
                    return;
                }

                try
                {
                    auto result = operation(connection);
                    push(connection);
                    callback(result, std::nullopt);
                    total_queries_++;
                }
                catch (const std::exception& e)
                {
                    push(connection);
                    callback(ResultType{}, std::string("Database operation failed: ") + e.what());
                    failed_queries_++;
                }
            }
        );

        thread_pool_->add_job(Thread::ThreadPriority::Normal, job);
    }
}
