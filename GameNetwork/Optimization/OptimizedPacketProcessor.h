#pragma once

#include "LockFree/LockFreeMessageQueue.h"
#include "ZeroCopy/ZeroCopyBuffer.h"
#include "../Packet/GamePacket.h"
#include <ThreadPool.h>
#include <Job.h>
#include <memory>
#include <functional>
#include <unordered_map>
#include <shared_mutex>

namespace GameNetwork::Optimization
{
	/**
	 * High-performance packet processor using zero-copy and lock-free techniques
	 */
	class OptimizedPacketProcessor
	{
	public:
		using PacketHandler = std::function<void(std::unique_ptr<GamePacket>)>;
		using PacketFactory = std::function<std::unique_ptr<GamePacket>(const PacketView&)>;
        
		explicit OptimizedPacketProcessor(std::shared_ptr<Thread::ThreadPool> thread_pool);
		~OptimizedPacketProcessor();
        
		/**
		 * Register packet handler for specific packet type
		 */
		auto register_handler(PacketType type, PacketHandler handler) -> void;
        
		/**
		 * Register packet factory for deserialization
		 */
		auto register_factory(PacketType type, PacketFactory factory) -> void;
        
		/**
		 * Process raw packet data (zero-copy)
		 * Returns true if packet was queued for processing
		 */
		auto process_raw_packet(const PacketView& packet_view) -> bool;
        
		/**
		 * Process packet object directly
		 */
		auto process_packet(std::unique_ptr<GamePacket> packet) -> bool;
        
		/**
		 * Start processing threads
		 */
		auto start(size_t worker_count = 4) -> void;
        
		/**
		 * Stop processing threads
		 */
		auto stop() -> void;
        
		/**
		 * Get queue statistics
		 */
		auto get_queue_size() const -> size_t { return queue_.size(); }
		auto get_processed_count() const -> uint64_t 
		{ 
			return processed_count_.load(std::memory_order_relaxed); 
		}
        
	private:
		auto worker_thread() -> void;
        
	private:
		std::shared_ptr<Thread::ThreadPool> thread_pool_;
		LockFreeMessageQueue queue_;
        
		// Handler registry with read-write lock for minimal contention
		mutable std::shared_mutex handlers_mutex_;
		std::unordered_map<PacketType, PacketHandler> handlers_;
        
		mutable std::shared_mutex factories_mutex_;
		std::unordered_map<PacketType, PacketFactory> factories_;
        
		// Worker management
		std::atomic<bool> running_{false};
		std::vector<std::future<void>> workers_;
        
		// Statistics
		std::atomic<uint64_t> processed_count_{0};
		std::atomic<uint64_t> dropped_count_{0};
	};
    
} // namespace GameNetwork::Optimization
