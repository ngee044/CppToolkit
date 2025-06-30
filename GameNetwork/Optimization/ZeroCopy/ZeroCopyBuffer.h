#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <tuple>
#include <string>
#include <vector>

namespace GameNetwork::Optimization
{
	// Forward declaration
	struct PacketView;
    
	/**
	 * Lock-free ring buffer for zero-copy packet handling
	 * Optimized for cache performance with proper alignment
	 */
	class ZeroCopyBuffer
	{
	public:
		static constexpr size_t CACHE_LINE_SIZE = 64;
		static constexpr size_t DEFAULT_BUFFER_SIZE = 1 << 20; // 1MB
        
		explicit ZeroCopyBuffer(size_t buffer_size = DEFAULT_BUFFER_SIZE);
		~ZeroCopyBuffer();
        
		// Disable copy operations
		ZeroCopyBuffer(const ZeroCopyBuffer&) = delete;
		ZeroCopyBuffer& operator=(const ZeroCopyBuffer&) = delete;
        
		// Enable move operations
		ZeroCopyBuffer(ZeroCopyBuffer&& other) noexcept;
		ZeroCopyBuffer& operator=(ZeroCopyBuffer&& other) noexcept;
        
		/**
		 * Write packet data without copying
		 * Returns tuple of (success, optional error message)
		 */
		auto write_packet(const uint8_t* data, size_t size) 
			-> std::tuple<bool, std::optional<std::string>>;
        
		/**
		 * Reserve space for writing
		 * Returns pointer and size if successful
		 */
		auto reserve_write_space(size_t size) 
			-> std::optional<std::pair<uint8_t*, size_t>>;
        
		/**
		 * Commit written data
		 */
		auto commit_write(size_t size) -> void;
        
		/**
		 * Read packet without copying (returns view)
		 * Returns tuple of (packet view, optional error message)
		 */
		auto read_packet() 
			-> std::tuple<std::optional<PacketView>, std::optional<std::string>>;
        
		/**
		 * Peek at next packet without consuming
		 */
		auto peek_packet() const 
			-> std::optional<PacketView>;
        
		/**
		 * Skip/consume bytes from read position
		 */
		auto skip_bytes(size_t size) -> bool;
        
		// Buffer statistics
		auto available_write_space() const -> size_t;
		auto available_read_space() const -> size_t;
		auto is_empty() const -> bool;
		auto is_full() const -> bool;
		auto capacity() const -> size_t { return capacity_; }
        
		// Reset buffer (not thread-safe)
		auto reset() -> void;
        
	private:
		// Helper functions
		auto wrap_position(size_t pos) const -> size_t
		{
			return pos & (capacity_ - 1); // Fast modulo for power of 2
		}
        
		auto distance(size_t from, size_t to) const -> size_t
		{
			return (to - from) & (capacity_ - 1);
		}
        
	private:
		alignas(CACHE_LINE_SIZE) std::unique_ptr<uint8_t[]> buffer_;
		const size_t capacity_; // Must be power of 2
        
		// Separate cache lines to avoid false sharing
		alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_pos_{0};
		alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_pos_{0};
		alignas(CACHE_LINE_SIZE) std::atomic<size_t> reserved_write_pos_{0};
	};
    
	/**
	 * Lightweight view into packet data without ownership
	 */
	struct PacketView
	{
		const uint8_t* data;
		size_t size;
		size_t buffer_offset; // Position in ring buffer
        
		PacketView() : data(nullptr), size(0), buffer_offset(0) {}
        
		PacketView(const uint8_t* d, size_t s, size_t offset)
			: data(d), size(s), buffer_offset(offset) {}
        
		auto as_vector() const -> std::vector<uint8_t>
		{
			return std::vector<uint8_t>(data, data + size);
		}
        
		auto is_valid() const -> bool
		{
			return data != nullptr && size > 0;
		}
	};
    
} // namespace GameNetwork::Optimization
