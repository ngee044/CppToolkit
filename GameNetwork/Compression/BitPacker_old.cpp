#include "BitPacker.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namesp        auto get_buffer() const -> const std::vector<uint8_t>&
        {
            return buffer_;
        }

        auto get_bit_position() const -> size_t
        {
            return bit_position_;
        }

        auto reset() -> void
        {
            buffer_.clear();
            bit_position_ = 0;
        }
{
    namespace Compression
    {
        BitPacker::BitPacker() : bit_position_(0)
        {
            buffer_.reserve(256); // Default initial capacity
        }

        BitPacker::BitPacker(size_t initial_capacity) : bit_position_(0)
        {
            buffer_.reserve((initial_capacity + 7) / 8); // Convert bits to bytes
        }

        auto BitPacker::write_bits(uint32_t value, uint8_t num_bits) -> void
        {
            if (num_bits > 32 || num_bits == 0)
            {
                throw std::invalid_argument("Invalid number of bits");
            }

            ensure_capacity(num_bits);

            // Mask to ensure we only use the specified number of bits
            uint32_t mask = (num_bits == 32) ? 0xFFFFFFFF : ((1u << num_bits) - 1);
            value &= mask;

            // Write bits
            while (num_bits > 0)
            {
                size_t byte_index = bit_position_ / 8;
                size_t bit_offset = bit_position_ % 8;
                size_t bits_available_in_byte = 8 - bit_offset;
                size_t bits_to_write = std::min(static_cast<size_t>(num_bits), bits_available_in_byte);

                // Extract the bits to write
                uint8_t bits_value = static_cast<uint8_t>(value & ((1u << bits_to_write) - 1));
                
                // Clear and set the bits in the target byte
                uint8_t mask_byte = static_cast<uint8_t>((1u << bits_to_write) - 1);
                buffer_[byte_index] &= ~(mask_byte << bit_offset);
                buffer_[byte_index] |= (bits_value << bit_offset);
                
                value >>= bits_to_write;
                num_bits -= static_cast<uint8_t>(bits_to_write);
                bit_position_ += bits_to_write;
            }
        }

        auto BitPacker::read_bits(uint8_t num_bits) -> uint32_t
        {
            if (num_bits > 32 || num_bits == 0)
            {
                throw std::invalid_argument("Invalid number of bits");
            }

            if (bit_position_ + num_bits > buffer_.size() * 8)
            {
                throw std::out_of_range("Not enough bits to read");
            }

            uint32_t result = 0;
            uint8_t bits_read = 0;

            while (bits_read < num_bits)
            {
                size_t byte_index = bit_position_ / 8;
                size_t bit_offset = bit_position_ % 8;
                size_t bits_available_in_byte = 8 - bit_offset;
                size_t bits_to_read = std::min(static_cast<size_t>(num_bits - bits_read), bits_available_in_byte);

                // Extract bits from the byte
                uint8_t mask = static_cast<uint8_t>((1u << bits_to_read) - 1);
                uint8_t bits_value = (buffer_[byte_index] >> bit_offset) & mask;
                
                result |= (static_cast<uint32_t>(bits_value) << bits_read);
                
                bits_read += static_cast<uint8_t>(bits_to_read);
                bit_position_ += bits_to_read;
            }

            return result;
        }

        auto BitPacker::get_buffer() const -> const std::vector<uint8_t>&
        {
            return buffer_;
        }

        auto BitPacker::get_bit_position() const -> size_t
        {
            return bit_position_;
        }

        auto BitPacker::reset() -> void
        {
            buffer_.clear();
            bit_position_ = 0;
        }

        auto BitPacker::ensure_capacity(size_t additional_bits) -> void
        {
            size_t required_bytes = (bit_position_ + additional_bits + 7) / 8;
            if (buffer_.size() < required_bytes)
            {
                buffer_.resize(required_bytes, 0);
            }
        }
    }
}
