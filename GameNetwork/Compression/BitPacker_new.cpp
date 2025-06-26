#include "BitPacker.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace GameNetwork
{
    namespace Compression
    {
        BitPacker::BitPacker() : bit_position_(0)
        {
            buffer_.reserve(1024);
        }

        BitPacker::BitPacker(size_t initial_capacity) : bit_position_(0)
        {
            buffer_.reserve(initial_capacity);
        }

        auto BitPacker::write_bits(uint32_t value, uint8_t num_bits) -> void
        {
            if (num_bits > 32) {
                throw std::invalid_argument("Cannot write more than 32 bits at once");
            }

            for (uint8_t i = 0; i < num_bits; ++i)
            {
                if ((bit_position_ % 8) == 0)
                {
                    buffer_.push_back(0);
                }

                uint8_t bit = (value >> i) & 1;
                if (bit)
                {
                    buffer_[bit_position_ / 8] |= (1 << (bit_position_ % 8));
                }

                bit_position_++;
            }
        }

        auto BitPacker::write_bool(bool value) -> void
        {
            write_bits(value ? 1 : 0, 1);
        }

        auto BitPacker::write_uint8(uint8_t value) -> void
        {
            write_bits(value, 8);
        }

        auto BitPacker::write_uint16(uint16_t value) -> void
        {
            write_bits(value, 16);
        }

        auto BitPacker::write_uint32(uint32_t value) -> void
        {
            write_bits(value, 32);
        }

        auto BitPacker::write_uint64(uint64_t value) -> void
        {
            write_uint32(static_cast<uint32_t>(value));
            write_uint32(static_cast<uint32_t>(value >> 32));
        }

        auto BitPacker::get_data() const -> std::vector<uint8_t>
        {
            return buffer_;
        }

        auto BitPacker::get_buffer() const -> const std::vector<uint8_t>&
        {
            return buffer_;
        }

        auto BitPacker::get_bit_position() const -> size_t
        {
            return bit_position_;
        }

        auto BitPacker::get_bit_count() const -> size_t
        {
            return bit_position_;
        }

        auto BitPacker::get_byte_count() const -> size_t
        {
            return (bit_position_ + 7) / 8;
        }

        auto BitPacker::clear() -> void
        {
            buffer_.clear();
            bit_position_ = 0;
        }

        auto BitPacker::reset() -> void
        {
            buffer_.clear();
            bit_position_ = 0;
        }

        auto BitPacker::ensure_capacity(size_t bits_needed) -> void
        {
            size_t bytes_needed = (bit_position_ + bits_needed + 7) / 8;
            if (buffer_.capacity() < bytes_needed)
            {
                buffer_.reserve(bytes_needed * 2);
            }
        }

        // BitUnpacker implementation
        BitUnpacker::BitUnpacker(const std::vector<uint8_t>& data) 
            : data_(data), bit_position_(0)
        {
        }

        auto BitUnpacker::read_bits(uint8_t num_bits) -> uint32_t
        {
            if (num_bits > 32) {
                throw std::invalid_argument("Cannot read more than 32 bits at once");
            }

            uint32_t result = 0;
            for (uint8_t i = 0; i < num_bits; ++i)
            {
                if (bit_position_ >= data_.size() * 8)
                {
                    throw std::out_of_range("Not enough data to read");
                }

                uint8_t bit = (data_[bit_position_ / 8] >> (bit_position_ % 8)) & 1;
                result |= (static_cast<uint32_t>(bit) << i);
                bit_position_++;
            }

            return result;
        }

        auto BitUnpacker::read_bool() -> bool
        {
            return read_bits(1) != 0;
        }

        auto BitUnpacker::read_uint8() -> uint8_t
        {
            return static_cast<uint8_t>(read_bits(8));
        }

        auto BitUnpacker::read_uint16() -> uint16_t
        {
            return static_cast<uint16_t>(read_bits(16));
        }

        auto BitUnpacker::read_uint32() -> uint32_t
        {
            return read_bits(32);
        }

        auto BitUnpacker::read_uint64() -> uint64_t
        {
            uint32_t low = read_uint32();
            uint32_t high = read_uint32();
            return (static_cast<uint64_t>(high) << 32) | low;
        }

        auto BitUnpacker::get_bits_read() const -> size_t
        {
            return bit_position_;
        }

        auto BitUnpacker::get_bits_remaining() const -> size_t
        {
            return (data_.size() * 8) - bit_position_;
        }

        auto BitUnpacker::is_empty() const -> bool
        {
            return bit_position_ >= data_.size() * 8;
        }

        auto BitUnpacker::ensure_bits_available(size_t bits_needed) const -> bool
        {
            return (bit_position_ + bits_needed) <= (data_.size() * 8);
        }
    }
}
