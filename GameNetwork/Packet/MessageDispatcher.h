#pragma once

#include "../Packet/GamePacket.h"
#include <ThreadPool.h>

#include <memory>
#include <functional>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <optional>
#include <tuple>
#include <future>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace GameNetwork
{
	class GameSession;
    
	using PacketHandler = std::function<std::tuple<bool, std::optional<std::string>>(
		std::shared_ptr<GameSession>, const GamePacket&)>;
    
	class MessageDispatcher : public std::enable_shared_from_this<MessageDispatcher>
	{
	public:
		MessageDispatcher();
		virtual ~MessageDispatcher();
        
		// Handler registration
		auto register_handler(PacketType type, PacketHandler handler) -> void;
		auto unregister_handler(PacketType type) -> void;
		auto has_handler(PacketType type) const -> bool;
        
		// Message dispatching
		auto dispatch(std::shared_ptr<GameSession> session, 
					 const GamePacket& packet) -> void;
        
		// Queue management
		auto queue_message(std::shared_ptr<GameSession> session,
						  std::unique_ptr<GamePacket> packet) -> void;
        
		// Thread pool
		auto set_thread_pool(std::shared_ptr<Thread::ThreadPool> thread_pool) -> void;
        
		// Lifecycle
		auto start() -> void;
		auto stop() -> void;
        
		// Priority queue management
		auto set_priority(PacketType type, PacketPriority priority) -> void;
		auto get_priority(PacketType type) const -> PacketPriority;
        
		// Rate limiting
		auto enable_rate_limiting(bool enable) -> void;
		auto is_rate_limiting_enabled() const -> bool;
		auto set_rate_limit(PacketType type, uint32_t max_per_second) -> void;
		auto check_rate_limit(const std::string& session_id, PacketType type) -> bool;
        
		// Default handlers
		auto register_default_handlers() -> void;
        
		// Statistics
		struct DispatcherStats
		{
			uint64_t total_dispatched;
			uint64_t failed_dispatches;
			uint64_t rate_limited;
			std::unordered_map<PacketType, uint64_t> dispatch_count_by_type;
			std::unordered_map<PacketType, uint64_t> average_processing_time_us;
		};
        
		auto get_stats() const -> DispatcherStats;
		auto reset_stats() -> void;
        
	private:
		struct QueuedMessage
		{
			std::shared_ptr<GameSession> session;
			std::unique_ptr<GamePacket> packet;
			PacketPriority priority;
			std::chrono::steady_clock::time_point queued_time;
            
			// Default constructor
			QueuedMessage() = default;
            
			// Move constructor
			QueuedMessage(QueuedMessage&& other) noexcept
				: session(std::move(other.session))
				, packet(std::move(other.packet))
				, priority(other.priority)
				, queued_time(other.queued_time)
			{}
            
			// Move assignment operator
			QueuedMessage& operator=(QueuedMessage&& other) noexcept
			{
				if (this != &other)
				{
					session = std::move(other.session);
					packet = std::move(other.packet);
					priority = other.priority;
					queued_time = other.queued_time;
				}
				return *this;
			}
            
			// Delete copy constructor and copy assignment
			QueuedMessage(const QueuedMessage&) = delete;
			QueuedMessage& operator=(const QueuedMessage&) = delete;
            
			auto operator<(const QueuedMessage& other) const -> bool
			{
				return priority < other.priority;
			}
		};
        
		struct RateLimitInfo
		{
			std::queue<std::chrono::steady_clock::time_point> timestamps;
		};
        
		auto worker_thread() -> void;
        
	private:
		mutable std::mutex handlers_mutex_;
		mutable std::mutex queue_mutex_;
		mutable std::mutex rate_limit_mutex_;
		std::condition_variable queue_cv_;
        
		// Handler storage
		std::unordered_map<PacketType, PacketHandler> handlers_;
		std::unordered_map<PacketType, PacketPriority> priorities_;
        
		// Message queue
		std::priority_queue<QueuedMessage> message_queue_;
        
		// Rate limiting
		std::atomic<bool> rate_limiting_enabled_;
		std::unordered_map<PacketType, uint32_t> rate_limits_;
		std::unordered_map<std::string, RateLimitInfo> rate_limit_info_;
        
		// Statistics
		DispatcherStats stats_;
        
		// Thread pool
		std::shared_ptr<Thread::ThreadPool> thread_pool_;
        
		// Worker threads
		std::vector<std::thread> workers_;
		std::atomic<bool> is_running_;
	};
}
