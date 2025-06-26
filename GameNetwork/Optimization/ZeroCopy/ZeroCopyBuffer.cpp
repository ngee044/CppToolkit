#include "ZeroCopyBuffer.h"
#include <algorithm>
#include <cstring>
#include <new>
#include <thread>

namespace GameNetwork::Optimization
{
    namespace // anonymous namespace for internal helpers
    {
        size_t round_up_power_of_2(size_t n)
        {
            if (n <= 1) return 1;
            n--;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            if (sizeof(size_t) > 4)
            {
                n |= n >> 32;
            }
            n++;
            return n;
        }
    }
    
    ZeroCopyBuffer::ZeroCopyBuffer(size_t buffer_size)
        : capacity_(round_up_power_of_2(buffer_size)) // Round up to power of 2
    {
        // Allocate aligned memory for optimal cache performance
        void* raw_memory = ::operator new[](capacity_, std::align_val_t{CACHE_LINE_SIZE});
        buffer_ = std::unique_ptr<uint8_t[]>(static_cast<uint8_t*>(raw_memory));
        
        // Initialize memory to avoid page faults later
        std::memset(buffer_.get(), 0, capacity_);
    }
    
    ZeroCopyBuffer::~ZeroCopyBuffer()
    {
        // Unique_ptr will handle deallocation
    }
    
    ZeroCopyBuffer::ZeroCopyBuffer(ZeroCopyBuffer&& other) noexcept
        : buffer_(std::move(other.buffer_))
        , capacity_(other.capacity_)
        , write_pos_(other.write_pos_.load())
        , read_pos_(other.read_pos_.load())
        , reserved_write_pos_(other.reserved_write_pos_.load())
    {
    }
    
    ZeroCopyBuffer& ZeroCopyBuffer::operator=(ZeroCopyBuffer&& other) noexcept
    {
        if (this != &other)
        {
            buffer_ = std::move(other.buffer_);
            write_pos_.store(other.write_pos_.load());
            read_pos_.store(other.read_pos_.load());
            reserved_write_pos_.store(other.reserved_write_pos_.load());
        }
        return *this;
    }
    
    auto ZeroCopyBuffer::write_packet(const uint8_t* data, size_t size) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!data || size == 0)
        {
            return {false, "Invalid packet data"};
        }
        
        // Reserve space
        auto space_opt = reserve_write_space(size + sizeof(uint32_t)); // Size header
        if (!space_opt)
        {
            return {false, "Buffer full"};
        }
        
        auto [space_ptr, space_size] = *space_opt;
        
        // Write size header (4 bytes)
        uint32_t size_header = static_cast<uint32_t>(size);
        std::memcpy(space_ptr, &size_header, sizeof(size_header));
        
        // Write packet data
        std::memcpy(space_ptr + sizeof(size_header), data, size);
        
        // Commit the write
        commit_write(size + sizeof(size_header));
        
        return {true, std::nullopt};
    }
    
    auto ZeroCopyBuffer::reserve_write_space(size_t size) 
        -> std::optional<std::pair<uint8_t*, size_t>>
    {
        size_t current_read = read_pos_.load(std::memory_order_acquire);
        size_t current_reserved = reserved_write_pos_.load(std::memory_order_relaxed);
        
        // Check available space
        size_t available = capacity_ - distance(current_read, current_reserved) - 1;
        if (available < size)
        {
            return std::nullopt; // Not enough space
        }
        
        // Try to reserve space
        size_t new_reserved = current_reserved + size;
        while (!reserved_write_pos_.compare_exchange_weak(
            current_reserved, new_reserved,
            std::memory_order_relaxed,
            std::memory_order_relaxed))
        {
            // Recalculate if CAS failed
            current_read = read_pos_.load(std::memory_order_acquire);
            available = capacity_ - distance(current_read, current_reserved) - 1;
            
            if (available < size)
            {
                return std::nullopt;
            }
            new_reserved = current_reserved + size;
        }
        
        // Return writable pair (pointer, size)
        size_t wrapped_pos = wrap_position(current_reserved);
        return std::make_pair(buffer_.get() + wrapped_pos, size);
    }
    
    auto ZeroCopyBuffer::commit_write(size_t size) -> void
    {
        // Update write position to match reserved position
        size_t expected = write_pos_.load(std::memory_order_relaxed);
        size_t new_pos = expected + size;
        
        // Spin until we can update (ensures ordered commits)
        while (!write_pos_.compare_exchange_weak(
            expected, new_pos,
            std::memory_order_release,
            std::memory_order_relaxed))
        {
            // Spin wait - in practice this should be very rare
            std::this_thread::yield();
            new_pos = expected + size;
        }
    }
    
    auto ZeroCopyBuffer::read_packet() 
        -> std::tuple<std::optional<PacketView>, std::optional<std::string>>
    {
        size_t current_read = read_pos_.load(std::memory_order_relaxed);
        size_t current_write = write_pos_.load(std::memory_order_acquire);
        
        // Check if buffer is empty
        if (current_read == current_write)
        {
            return {std::nullopt, "Buffer empty"};
        }
        
        // Read size header
        size_t wrapped_read = wrap_position(current_read);
        uint32_t size_header;
        std::memcpy(&size_header, buffer_.get() + wrapped_read, sizeof(size_header));
        
        // Validate size
        size_t total_size = size_header + sizeof(size_header);
        if (distance(current_read, current_write) < total_size)
        {
            return {std::nullopt, "Incomplete packet"};
        }
        
        // Create packet view
        size_t data_pos = wrap_position(current_read + sizeof(size_header));
        PacketView view(buffer_.get() + data_pos, size_header, current_read);
        
        // Update read position
        read_pos_.store(current_read + total_size, std::memory_order_release);
        
        return {view, std::nullopt};
    }
    
    auto ZeroCopyBuffer::peek_packet() const 
        -> std::optional<PacketView>
    {
        size_t current_read = read_pos_.load(std::memory_order_relaxed);
        size_t current_write = write_pos_.load(std::memory_order_acquire);
        
        if (current_read == current_write)
        {
            return std::nullopt;
        }
        
        size_t wrapped_read = wrap_position(current_read);
        uint32_t size_header;
        std::memcpy(&size_header, buffer_.get() + wrapped_read, sizeof(size_header));
        
        size_t total_size = size_header + sizeof(size_header);
        if (distance(current_read, current_write) < total_size)
        {
            return std::nullopt;
        }
        
        size_t data_pos = wrap_position(current_read + sizeof(size_header));
        return PacketView(buffer_.get() + data_pos, size_header, current_read);
    }
    
    auto ZeroCopyBuffer::skip_bytes(size_t size) -> bool
    {
        size_t current_read = read_pos_.load(std::memory_order_relaxed);
        size_t current_write = write_pos_.load(std::memory_order_acquire);
        
        if (distance(current_read, current_write) < size)
        {
            return false;
        }
        
        read_pos_.store(current_read + size, std::memory_order_release);
        return true;
    }
    
    auto ZeroCopyBuffer::available_write_space() const -> size_t
    {
        size_t current_read = read_pos_.load(std::memory_order_acquire);
        size_t current_write = write_pos_.load(std::memory_order_relaxed);
        return capacity_ - distance(current_read, current_write) - 1;
    }
    
    auto ZeroCopyBuffer::available_read_space() const -> size_t
    {
        size_t current_read = read_pos_.load(std::memory_order_relaxed);
        size_t current_write = write_pos_.load(std::memory_order_acquire);
        return distance(current_read, current_write);
    }
    
    auto ZeroCopyBuffer::is_empty() const -> bool
    {
        return read_pos_.load(std::memory_order_relaxed) == 
               write_pos_.load(std::memory_order_acquire);
    }
    
    auto ZeroCopyBuffer::is_full() const -> bool
    {
        return available_write_space() == 0;
    }
    
    auto ZeroCopyBuffer::reset() -> void
    {
        write_pos_.store(0, std::memory_order_relaxed);
        read_pos_.store(0, std::memory_order_relaxed);
        reserved_write_pos_.store(0, std::memory_order_relaxed);
    }
    
} // namespace GameNetwork::Optimization
