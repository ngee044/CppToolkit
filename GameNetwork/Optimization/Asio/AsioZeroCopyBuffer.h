#pragma once

#include "../ZeroCopy/ZeroCopyBuffer.h"
#include <boost/asio.hpp>
#include <memory>
#include <functional>

namespace GameNetwork::Optimization
{
	/**
	 * Boost.Asio integrated zero-copy network buffer
	 * Provides direct memory mapping for network I/O
	 */
	class AsioZeroCopyBuffer
	{
	public:
		using CompletionHandler = std::function<void(boost::system::error_code, size_t)>;
        
		explicit AsioZeroCopyBuffer(size_t buffer_size = ZeroCopyBuffer::DEFAULT_BUFFER_SIZE);
		~AsioZeroCopyBuffer();
        
		/**
		 * Get mutable buffer for async_read operations
		 * Automatically manages buffer space
		 */
		auto get_read_buffer() -> boost::asio::mutable_buffer;
        
		/**
		 * Commit bytes read from network
		 */
		auto commit_read(size_t bytes_transferred) -> void;
        
		/**
		 * Get const buffer for async_write operations
		 * Returns buffer with data ready to send
		 */
		auto get_write_buffer() -> boost::asio::const_buffer;
        
		/**
		 * Mark bytes as sent
		 */
		auto consume_write(size_t bytes_transferred) -> void;
        
		/**
		 * Direct packet write (zero-copy)
		 * Returns true if successful
		 */
		auto write_packet(const uint8_t* data, size_t size) -> bool;
        
		/**
		 * Read packet view (zero-copy)
		 * Returns packet view if available
		 */
		auto read_packet() -> std::optional<PacketView>;
        
		/**
		 * Check if data is available for writing
		 */
		auto has_data_to_write() const -> bool;
        
		/**
		 * Get underlying zero-copy buffer
		 */
		auto buffer() -> ZeroCopyBuffer& { return buffer_; }
		auto buffer() const -> const ZeroCopyBuffer& { return buffer_; }
        
	private:
		ZeroCopyBuffer buffer_;
        
		// Track pending read/write operations
		size_t pending_read_size_{0};
		size_t pending_write_size_{0};
        
		// Temporary buffer for when ring buffer wraps
		std::vector<uint8_t> wrap_buffer_;
	};
    
	/**
	 * Custom allocator for Boost.Asio handlers
	 * Reduces allocation overhead
	 */
	template<typename T>
	class PoolAllocator
	{
	private:
		static constexpr size_t POOL_SIZE = 1024;
        
		struct Block
		{
			alignas(T) uint8_t data[sizeof(T)];
			bool in_use{false};
		};
        
		static thread_local std::array<Block, POOL_SIZE> pool_;
		static thread_local size_t next_free_;
        
	public:
		using value_type = T;
        
		PoolAllocator() = default;
        
		template<typename U>
		PoolAllocator(const PoolAllocator<U>&) noexcept {}
        
		T* allocate(std::size_t n)
		{
			if (n != 1)
			{
				return static_cast<T*>(::operator new(n * sizeof(T)));
			}
            
			// Find free block
			for (size_t i = 0; i < POOL_SIZE; ++i)
			{
				size_t idx = (next_free_ + i) % POOL_SIZE;
				if (!pool_[idx].in_use)
				{
					pool_[idx].in_use = true;
					next_free_ = (idx + 1) % POOL_SIZE;
					return reinterpret_cast<T*>(&pool_[idx].data);
				}
			}
            
			// Pool exhausted, fall back to heap
			return static_cast<T*>(::operator new(sizeof(T)));
		}
        
		void deallocate(T* p, std::size_t n)
		{
			if (n != 1)
			{
				::operator delete(p);
				return;
			}
            
			// Check if pointer is from pool
			auto ptr_addr = reinterpret_cast<uintptr_t>(p);
			auto pool_start = reinterpret_cast<uintptr_t>(&pool_[0]);
			auto pool_end = reinterpret_cast<uintptr_t>(&pool_[POOL_SIZE]);
            
			if (ptr_addr >= pool_start && ptr_addr < pool_end)
			{
				// Mark as free
				size_t idx = (ptr_addr - pool_start) / sizeof(Block);
				pool_[idx].in_use = false;
			}
			else
			{
				// Not from pool, use regular delete
				::operator delete(p);
			}
		}
	};
    
	template<typename T>
	thread_local std::array<typename PoolAllocator<T>::Block, PoolAllocator<T>::POOL_SIZE> 
		PoolAllocator<T>::pool_{};
    
	template<typename T>
	thread_local size_t PoolAllocator<T>::next_free_ = 0;
    
} // namespace GameNetwork::Optimization
