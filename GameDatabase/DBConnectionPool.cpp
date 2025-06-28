#include "DBConnectionPool.h"
#include <Logger.h>
#include <Converter.h>
#include "Job.h"
#include <algorithm>
#include <numeric>
#include <thread>

namespace GameDatabase
{
    DBConnectionPool::DBConnectionPool()
        : active_connections_(0)
        , connection_timeout_(std::chrono::seconds(30))
        , validation_query_("SELECT 1")
        , auto_validation_enabled_(false)
        , validation_interval_(std::chrono::minutes(5))
        , validation_timer_running_(false)
        , leak_detection_timeout_(std::chrono::minutes(30))
    {
        // Initialize pool stats
        pool_stats_.total_connections_created = 0;
        pool_stats_.total_connections_destroyed = 0;
        pool_stats_.total_queries_executed = 0;
        pool_stats_.total_failed_queries = 0;
        pool_stats_.current_active_connections = 0;
        pool_stats_.current_available_connections = 0;
        pool_stats_.peak_active_connections = 0;
        pool_stats_.average_wait_time = std::chrono::milliseconds(0);
        pool_stats_.max_wait_time = std::chrono::milliseconds(0);
        pool_stats_.pool_created_time = std::chrono::steady_clock::now();
        pool_stats_.leaked_connections_detected = 0;
        pool_stats_.leaked_connections_recovered = 0;
    }

    DBConnectionPool::~DBConnectionPool()
    {
        stop_validation_timer();
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
            pool_stats_.total_connections_created++;
        }

        // Start auto validation if enabled
        if (auto_validation_enabled_)
        {
            start_validation_timer();
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

    auto DBConnectionPool::pop_with_context(const std::string& context) -> std::shared_ptr<DBConnection>
    {
        auto start_time = std::chrono::steady_clock::now();
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
        
        // Record wait time
        auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        wait_times_.push_back(wait_time);
        if (wait_times_.size() > MAX_WAIT_TIME_SAMPLES)
        {
            wait_times_.pop_front();
        }
        
        // Update statistics
        pool_stats_.current_active_connections = active_connections_;
        pool_stats_.current_available_connections = available_connections_.size();
        pool_stats_.peak_active_connections = (std::max)(pool_stats_.peak_active_connections, 
                                                       static_cast<uint64_t>(active_connections_));
        
        if (wait_time > pool_stats_.max_wait_time)
        {
            pool_stats_.max_wait_time = wait_time;
        }
        
        if (!wait_times_.empty())
        {
            auto total_wait = std::accumulate(wait_times_.begin(), wait_times_.end(), 
                                              std::chrono::milliseconds(0));
            pool_stats_.average_wait_time = total_wait / wait_times_.size();
        }
        
        // Record lease for leak detection
        active_leases_[connection] = ConnectionLease{
            connection,
            std::chrono::steady_clock::now(),
            context
        };

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
            
            // Remove from active leases
            active_leases_.erase(connection);
            
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
                    pool_stats_.current_active_connections = active_connections_;
                    pool_stats_.current_available_connections = available_connections_.size();
                    return;
                }
            }

            available_connections_.push_back(connection);
            active_connections_--;
            
            // Update statistics
            pool_stats_.current_active_connections = active_connections_;
            pool_stats_.current_available_connections = available_connections_.size();
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
                auto connection = pop_with_context("async_operation");
                if (!connection)
                {
                    callback(false, "Failed to get connection from pool");
                    pool_stats_.total_failed_queries++;
                    return { false, "Failed to get connection from pool" };
                }

                try
                {
                    operation(connection);
                    push(connection);
                    callback(true, std::nullopt);
                    pool_stats_.total_queries_executed++;
                    return { true, std::nullopt };
                }
                catch (const std::exception& e)
                {
                    push(connection);
                    callback(false, std::string("Database operation failed: ") + e.what());
                    pool_stats_.total_failed_queries++;
                    return { false, e.what() };
                }
            },
            "DBConnectionPool async operation"
        );
        
        thread_pool_->add_job(job);
    }

    auto DBConnectionPool::get_pool_stats() const -> ConnectionPoolStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto stats = pool_stats_;
        stats.current_active_connections = active_connections_;
        stats.current_available_connections = available_connections_.size();
        return stats;
    }

    auto DBConnectionPool::validate_all_connections() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t validated = 0;
        size_t failed = 0;
        
        for (auto& conn : connections_)
        {
            if (validate_connection(conn))
            {
                validated++;
            }
            else
            {
                failed++;
            }
        }
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Connection validation completed: " + std::to_string(validated) + 
            " validated, " + std::to_string(failed) + " failed");
    }

    auto DBConnectionPool::set_validation_query(const std::string& query) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        validation_query_ = query;
    }

    auto DBConnectionPool::get_validation_query() const -> std::string
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return validation_query_;
    }

    auto DBConnectionPool::enable_auto_validation(bool enable, std::chrono::minutes interval) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (auto_validation_enabled_ && !enable)
        {
            stop_validation_timer();
        }
        
        auto_validation_enabled_ = enable;
        validation_interval_ = interval;
        
        if (enable && !connections_.empty())
        {
            start_validation_timer();
        }
    }

    auto DBConnectionPool::set_leak_detection_timeout(std::chrono::minutes timeout) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        leak_detection_timeout_ = timeout;
    }

    auto DBConnectionPool::check_for_leaked_connections() -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        size_t leaked_count = 0;
        std::vector<std::shared_ptr<DBConnection>> leaked_connections;
        
        for (const auto& [conn, lease] : active_leases_)
        {
            auto lease_duration = std::chrono::duration_cast<std::chrono::minutes>(now - lease.lease_time);
            if (lease_duration >= leak_detection_timeout_)
            {
                leaked_count++;
                leaked_connections.push_back(conn);
                
                std::string leak_info = "Leaked connection detected - Context: " + lease.lease_context +
                                       ", Lease time: " + std::to_string(lease_duration.count()) + " minutes";
                leaked_connection_logs_.push_back(leak_info);
                
                Utilities::Logger::handle().write(Utilities::LogTypes::Warning, leak_info);
            }
        }
        
        // Force return leaked connections
        for (auto& leaked_conn : leaked_connections)
        {
            active_leases_.erase(leaked_conn);
            available_connections_.push_back(leaked_conn);
            active_connections_--;
            pool_stats_.leaked_connections_recovered++;
        }
        
        pool_stats_.leaked_connections_detected += leaked_count;
        pool_stats_.current_active_connections = active_connections_;
        pool_stats_.current_available_connections = available_connections_.size();
        
        return leaked_count;
    }

    auto DBConnectionPool::get_leaked_connection_info() const -> std::vector<std::string>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return leaked_connection_logs_;
    }

    auto DBConnectionPool::validate_connection(std::shared_ptr<DBConnection> connection) -> bool
    {
        if (!connection || !connection->is_valid())
        {
            return false;
        }
        
        try
        {
            // Execute validation query
            // This is a simplified version - in real implementation you'd execute the actual query
            return true; // Assume validation passed for now
        }
        catch (const std::exception& e)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                "Connection validation failed: " + std::string(e.what()));
            return false;
        }
    }

    auto DBConnectionPool::start_validation_timer() -> void
    {
        if (validation_timer_running_)
        {
            return;
        }
        
        validation_timer_running_ = true;
        validation_timer_thread_ = std::thread(&DBConnectionPool::validation_timer_thread, this);
    }

    auto DBConnectionPool::stop_validation_timer() -> void
    {
        validation_timer_running_ = false;
        if (validation_timer_thread_.joinable())
        {
            validation_timer_thread_.join();
        }
    }

    auto DBConnectionPool::validation_timer_thread() -> void
    {
        while (validation_timer_running_)
        {
            std::this_thread::sleep_for(validation_interval_);
            
            if (!validation_timer_running_)
            {
                break;
            }
            
            try
            {
                validate_all_connections();
                check_for_leaked_connections();
            }
            catch (const std::exception& e)
            {
                Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                    "Validation timer error: " + std::string(e.what()));
            }
        }
    }

    auto DBConnectionPool::recreate_connection(std::shared_ptr<DBConnection> old_connection)
        -> std::tuple<std::shared_ptr<DBConnection>, std::optional<std::string>>
    {
        try
        {
            // Create a new connection
            auto new_connection = std::make_shared<DBConnection>();
            
            // Connect using the stored connection string
            auto [success, error] = new_connection->connect(environment_, connection_string_);
            if (!success)
            {
                return {nullptr, error};
            }
            
            // Update statistics
            pool_stats_.total_connections_created++;
            if (old_connection)
            {
                pool_stats_.total_connections_destroyed++;
            }
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Successfully recreated database connection");
            
            return {new_connection, std::nullopt};
        }
        catch (const std::exception& e)
        {
            std::string error_msg = "Failed to recreate connection: " + std::string(e.what());
            Utilities::Logger::handle().write(Utilities::LogTypes::Error, error_msg);
            return {nullptr, error_msg};
        }
    }
}
