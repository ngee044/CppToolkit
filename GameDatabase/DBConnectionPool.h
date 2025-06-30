#pragma once

#include "DBConnection.h"

#include <ThreadPool.h>
#include <Job.h>

#include <functional>
#include <future>
#include <chrono>
#include <condition_variable>
#include <unordered_map>
#include <deque>

namespace GameDatabase
{
	// Async query callback types
	template<typename T>
	using QueryCallback = std::function<void(T result, std::optional<std::string> error)>;
    
	using ExecuteCallback = std::function<void(bool success, std::optional<std::string> error)>;

	// Connection pool statistics
	struct ConnectionPoolStats
	{
		uint64_t total_connections_created;
		uint64_t total_connections_destroyed;
		uint64_t total_queries_executed;
		uint64_t total_failed_queries;
		uint64_t current_active_connections;
		uint64_t current_available_connections;
		uint64_t peak_active_connections;
        
		std::chrono::milliseconds average_wait_time;
		std::chrono::milliseconds max_wait_time;
		std::chrono::steady_clock::time_point pool_created_time;
        
		// Leak detection
		uint64_t leaked_connections_detected;
		uint64_t leaked_connections_recovered;
	};

	// Connection lease tracking for leak detection
	struct ConnectionLease
	{
		std::shared_ptr<DBConnection> connection;
		std::chrono::steady_clock::time_point lease_time;
		std::string lease_context; // for debugging
	};

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
		auto pop_with_context(const std::string& context) -> std::shared_ptr<DBConnection>;
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
		auto get_pool_stats() const -> ConnectionPoolStats;
        
		// Connection health check
		auto check_connections_health() -> std::tuple<size_t, size_t>; // returns (healthy, unhealthy)
		auto validate_all_connections() -> void; // runs validation query on all connections
        
		// Timeout configuration
		auto set_connection_timeout(std::chrono::seconds timeout) -> void;
		auto get_connection_timeout() const -> std::chrono::seconds;
        
		// Validation query configuration
		auto set_validation_query(const std::string& query) -> void;
		auto get_validation_query() const -> std::string;
		auto enable_auto_validation(bool enable, std::chrono::minutes interval = std::chrono::minutes(5)) -> void;
        
		// Leak detection
		auto set_leak_detection_timeout(std::chrono::minutes timeout) -> void;
		auto check_for_leaked_connections() -> size_t; // returns number of leaked connections found
		auto get_leaked_connection_info() const -> std::vector<std::string>;

	private:
		auto wait_for_connection(std::chrono::milliseconds timeout) 
			-> std::tuple<std::shared_ptr<DBConnection>, std::optional<std::string>>;
		auto recreate_connection(std::shared_ptr<DBConnection> old_connection) 
			-> std::tuple<std::shared_ptr<DBConnection>, std::optional<std::string>>;
		auto validate_connection(std::shared_ptr<DBConnection> connection) -> bool;
		auto start_validation_timer() -> void;
		auto stop_validation_timer() -> void;
		auto validation_timer_thread() -> void;

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
        
		// Enhanced statistics
		ConnectionPoolStats pool_stats_;
		std::deque<std::chrono::milliseconds> wait_times_;
		static constexpr size_t MAX_WAIT_TIME_SAMPLES = 100;
        
		// Validation
		std::string validation_query_;
		bool auto_validation_enabled_;
		std::chrono::minutes validation_interval_;
		std::thread validation_timer_thread_;
		std::atomic<bool> validation_timer_running_;
        
		// Leak detection
		std::unordered_map<std::shared_ptr<DBConnection>, ConnectionLease> active_leases_;
		std::chrono::minutes leak_detection_timeout_;
		std::vector<std::string> leaked_connection_logs_;
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
				auto connection = pop_with_context("async_operation");
				if (!connection)
				{
					callback(ResultType{}, "Failed to get connection from pool");
					pool_stats_.total_failed_queries++;
					return;
				}

				try
				{
					auto result = operation(connection);
					push(connection);
					callback(result, std::nullopt);
					pool_stats_.total_queries_executed++;
				}
				catch (const std::exception& e)
				{
					push(connection);
					callback(ResultType{}, std::string("Database operation failed: ") + e.what());
					pool_stats_.total_failed_queries++;
				}
			}
		);

		thread_pool_->push(Thread::JobPriorities::Normal, job);
	}
}
