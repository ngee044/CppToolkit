#pragma once

#include "../../Packet/GamePacket.h"
#include <atomic>
#include <memory>
#include <array>
#include <optional>

namespace GameNetwork::Optimization
{
	class LockFreeMessageQueue
	{
	public:
		static constexpr size_t CACHE_LINE_SIZE = 64;
		static constexpr size_t DEFAULT_QUEUE_SIZE = 65536;
        
		explicit LockFreeMessageQueue(size_t queue_size = DEFAULT_QUEUE_SIZE);
		~LockFreeMessageQueue();
        
		// Disable copy
		LockFreeMessageQueue(const LockFreeMessageQueue&) = delete;
		LockFreeMessageQueue& operator=(const LockFreeMessageQueue&) = delete;
        
		// Enable move
		LockFreeMessageQueue(LockFreeMessageQueue&& other) noexcept;
		LockFreeMessageQueue& operator=(LockFreeMessageQueue&& other) noexcept;
        
		/**
		 * Enqueue a packet (thread-safe)
		 * Returns true if successful, false if queue is full
		 */
		auto enqueue(std::unique_ptr<GamePacket> packet) -> bool;
        
		/**
		 * Dequeue a packet (thread-safe)
		 * Returns nullptr if queue is empty
		 */
		auto dequeue() -> std::unique_ptr<GamePacket>;
        
		/**
		 * Try to dequeue with timeout
		 * Returns packet or nullptr if timeout
		 */
		auto dequeue_wait(std::chrono::milliseconds timeout) 
			-> std::unique_ptr<GamePacket>;
        
		// Queue statistics
		auto size() const -> size_t;
		auto capacity() const -> size_t { return capacity_; }
		auto is_empty() const -> bool;
		auto is_full() const -> bool;
        
	private:
		struct Cell
		{
			alignas(CACHE_LINE_SIZE) std::atomic<size_t> sequence;
			std::unique_ptr<GamePacket> packet;
            
			Cell() : sequence(0), packet(nullptr) {}
		};
        
		auto wrap_position(size_t pos) const -> size_t
		{
			return pos & (capacity_ - 1);
		}
        
	private:
		const size_t capacity_;
		std::unique_ptr<Cell[]> buffer_;
        
		// Separate cache lines for producers and consumers
		alignas(CACHE_LINE_SIZE) std::atomic<size_t> enqueue_pos_{0};
		alignas(CACHE_LINE_SIZE) std::atomic<size_t> dequeue_pos_{0};
        
		// Padding to prevent false sharing
		char padding_[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];
	};
    
} // namespace GameNetwork::Optimization
