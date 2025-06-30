#include "AsioZeroCopyBuffer.h"
#include <algorithm>

namespace GameNetwork::Optimization
{
	AsioZeroCopyBuffer::AsioZeroCopyBuffer(size_t buffer_size)
		: buffer_(buffer_size)
		, wrap_buffer_(std::max(size_t(1), buffer_size / 4)) // Reserve at least 1 byte
	{
	}
    
	AsioZeroCopyBuffer::~AsioZeroCopyBuffer() = default;
    
	auto AsioZeroCopyBuffer::get_read_buffer() -> boost::asio::mutable_buffer
	{
		// Reserve space for reading
		auto space_opt = buffer_.reserve_write_space(4096); // Default read size
        
		if (space_opt)
		{
			auto [space_ptr, space_size] = *space_opt;
			pending_read_size_ = space_size;
			return boost::asio::buffer(space_ptr, space_size);
		}
        
		// Buffer full, ensure wrap_buffer_ has at least 1 element
		pending_read_size_ = 0;
		if (wrap_buffer_.empty())
		{
			wrap_buffer_.resize(1);
		}
		// Ensure data() is not null by checking again
		uint8_t* buffer_ptr = wrap_buffer_.data();
		if (!buffer_ptr)
		{
			// This should never happen if resize worked
			static uint8_t fallback_buffer[1] = {0};
			buffer_ptr = fallback_buffer;
		}
		return boost::asio::buffer(buffer_ptr, 0);
	}
    
	auto AsioZeroCopyBuffer::commit_read(size_t bytes_transferred) -> void
	{
		if (pending_read_size_ > 0 && bytes_transferred > 0)
		{
			buffer_.commit_write(bytes_transferred);
			pending_read_size_ = 0;
		}
	}
    
	auto AsioZeroCopyBuffer::get_write_buffer() -> boost::asio::const_buffer
	{
		// Peek at next packet without consuming
		auto packet_view = buffer_.peek_packet();
        
		if (packet_view)
		{
			pending_write_size_ = packet_view->size;
			return boost::asio::buffer(packet_view->data, packet_view->size);
		}
        
		pending_write_size_ = 0;
		static const uint8_t empty_buffer[1] = {0};
		return boost::asio::buffer(empty_buffer, 0);
	}
    
	auto AsioZeroCopyBuffer::consume_write(size_t bytes_transferred) -> void
	{
		if (pending_write_size_ > 0 && bytes_transferred > 0)
		{
			// Skip the packet we just sent
			buffer_.skip_bytes(bytes_transferred + sizeof(uint32_t)); // Include size header
			pending_write_size_ = 0;
		}
	}
    
	auto AsioZeroCopyBuffer::write_packet(const uint8_t* data, size_t size) -> bool
	{
		auto [success, error] = buffer_.write_packet(data, size);
		return success;
	}
    
	auto AsioZeroCopyBuffer::read_packet() -> std::optional<PacketView>
	{
		auto [packet_view, error] = buffer_.read_packet();
		return packet_view;
	}
    
	auto AsioZeroCopyBuffer::has_data_to_write() const -> bool
	{
		return !buffer_.is_empty();
	}
    
} // namespace GameNetwork::Optimization
