#pragma once

#include "LockFreePool.h"
#include "DBConnection.h"

#include <ThreadPool.h>

#include <chrono>
#include <functional>
#include <future>

using namespace Thread;

namespace GameDatabase::Optimization
{
	/**
	 * High-performance database connection pool using lock-free data structures
	 */
	class OptimizedDBConnectionPool
	{
	public:
		OptimizedDBConnectionPool();
		~OptimizedDBConnectionPool();
        
		// Connection management
		auto connect(std::int32_t connection_count, const std::wstring& connection_string) 
			-> std::tuple<bool, std::optional<std::string>>;
		auto clear() -> void;
        
		// Zero-allocation connection acquisition
		auto acquire() -> std::shared_ptr<DBConnection>;
		auto release(std::shared_ptr<DBConnection> connection) -> void;
        
		// Async operations with minimal overhead
		auto set_thread_pool(std::shared_ptr<ThreadPool> thread_pool) -> void
		{
			thread_pool_ = thread_pool;
		}
        
		template<typename ResultType>
		auto execute_async(std::function<ResultType(std::shared_ptr<DBConnection>)> operation)
			-> std::future<std::tuple<ResultType, std::optional<std::string>>>
		{
			auto promise = std::make_shared<std::promise<std::tuple<ResultType, std::optional<std::string>>>>();
			auto future = promise->get_future();
            
			thread_pool_->push(JobPriorities::Normal, 
				[this, operation, promise]()
			{
				auto connection = acquire();
				if (!connection)
				{
					promise->set_value({ResultType{}, "Failed to acquire connection"});
					return;
				}
                
				try
				{
					auto result = operation(connection);
					release(connection);
					promise->set_value({result, std::nullopt});
				}
				catch (const std::exception& e)
				{
					release(connection);
					promise->set_value({ResultType{}, e.what()});
				}
			});
            
			return future;
		}
        
		// Pool statistics
		auto get_available_connections() const -> size_t
		{
			return available_connections_.size();
		}
        
		auto get_total_connections() const -> size_t
		{
			return total_connections_;
		}
        
		auto get_active_connections() const -> size_t
		{
			return active_connections_.load(std::memory_order_relaxed);
		}
        
		auto is_healthy() const -> bool
		{
			return get_available_connections() > 0 || get_active_connections() < get_total_connections();
		}
        
	private:
		auto create_connection() -> std::shared_ptr<DBConnection>;
        
	private:
		SQLHENV environment_;
		std::wstring connection_string_;
        
		// Lock-free pool for available connections
		LockFreePool<std::shared_ptr<DBConnection>> available_connections_;
        
		// Statistics
		std::atomic<size_t> total_connections_{0};
		std::atomic<size_t> active_connections_{0};
		std::atomic<uint64_t> total_queries_{0};
		std::atomic<uint64_t> failed_queries_{0};
        
		std::shared_ptr<ThreadPool> thread_pool_;
	};
    
} // namespace GameDatabase::Optimization
