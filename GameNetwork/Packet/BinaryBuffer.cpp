#include "BinaryBuffer.h"
#include "../../Utilities/Logger.h"

using namespace Utilities;

namespace GameNetwork
{
    BinaryBuffer::BinaryBuffer()
        : read_position_(0)
    {
        buffer_.reserve(256);
    }

    BinaryBuffer::BinaryBuffer(size_t initial_capacity)
        : read_position_(0)
    {
        buffer_.reserve(initial_capacity);
    }

    BinaryBuffer::~BinaryBuffer() = default;

    auto BinaryBuffer::write_uint8(uint8_t value) -> void
    {
        buffer_.push_back(value);
    }

    auto BinaryBuffer::write_uint16(uint16_t value) -> void
    {
        ensure_capacity(sizeof(uint16_t));
        buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    auto BinaryBuffer::write_uint32(uint32_t value) -> void
    {
        ensure_capacity(sizeof(uint32_t));
        buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    auto BinaryBuffer::write_uint64(uint64_t value) -> void
    {
        ensure_capacity(sizeof(uint64_t));
        for (int i = 0; i < 8; ++i)
        {
            buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    auto BinaryBuffer::write_int8(int8_t value) -> void
    {
        write_uint8(static_cast<uint8_t>(value));
    }

    auto BinaryBuffer::write_int16(int16_t value) -> void
    {
        write_uint16(static_cast<uint16_t>(value));
    }

    auto BinaryBuffer::write_int32(int32_t value) -> void
    {
        write_uint32(static_cast<uint32_t>(value));
    }

    auto BinaryBuffer::write_int64(int64_t value) -> void
    {
        write_uint64(static_cast<uint64_t>(value));
    }
    auto BinaryBuffer::write_float(float value) -> void
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        write_uint32(bits);
    }

    auto BinaryBuffer::write_double(double value) -> void
    {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        write_uint64(bits);
    }

    auto BinaryBuffer::write_bool(bool value) -> void
    {
        write_uint8(value ? 1 : 0);
    }

    auto BinaryBuffer::write_string(const std::string& value) -> void
    {
        uint32_t length = static_cast<uint32_t>(value.length());
        write_uint32(length);
        write_bytes(reinterpret_cast<const uint8_t*>(value.data()), value.length());
    }

    auto BinaryBuffer::write_bytes(const uint8_t* data, size_t length) -> void
    {
        ensure_capacity(length);
        buffer_.insert(buffer_.end(), data, data + length);
    }

    auto BinaryBuffer::read_uint8() -> std::tuple<bool, uint8_t>
    {
        if (!can_read(sizeof(uint8_t)))
        {
            return {false, 0};
        }
        
        uint8_t value = buffer_[read_position_++];
        return {true, value};
    }

    auto BinaryBuffer::read_uint16() -> std::tuple<bool, uint16_t>
    {
        if (!can_read(sizeof(uint16_t)))
        {
            return {false, 0};
        }
        
        uint16_t value = static_cast<uint16_t>(buffer_[read_position_]) |
                        (static_cast<uint16_t>(buffer_[read_position_ + 1]) << 8);
        read_position_ += sizeof(uint16_t);
        return {true, value};
    }

    auto BinaryBuffer::read_uint32() -> std::tuple<bool, uint32_t>
    {
        if (!can_read(sizeof(uint32_t)))
        {
            return {false, 0};
        }
        
        uint32_t value = static_cast<uint32_t>(buffer_[read_position_]) |
                        (static_cast<uint32_t>(buffer_[read_position_ + 1]) << 8) |
                        (static_cast<uint32_t>(buffer_[read_position_ + 2]) << 16) |
                        (static_cast<uint32_t>(buffer_[read_position_ + 3]) << 24);
        read_position_ += sizeof(uint32_t);
        return {true, value};
    }

    auto BinaryBuffer::read_uint64() -> std::tuple<bool, uint64_t>
    {
        if (!can_read(sizeof(uint64_t)))
        {
            return {false, 0};
        }
        
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i)
        {
            value |= static_cast<uint64_t>(buffer_[read_position_ + i]) << (i * 8);
        }
        read_position_ += sizeof(uint64_t);
        return {true, value};
    }

    auto BinaryBuffer::read_int8() -> std::tuple<bool, int8_t>
    {
        auto [success, value] = read_uint8();
        return {success, static_cast<int8_t>(value)};
    }

    auto BinaryBuffer::read_int16() -> std::tuple<bool, int16_t>
    {
        auto [success, value] = read_uint16();
        return {success, static_cast<int16_t>(value)};
    }
    auto BinaryBuffer::read_int32() -> std::tuple<bool, int32_t>
    {
        auto [success, value] = read_uint32();
        return {success, static_cast<int32_t>(value)};
    }

    auto BinaryBuffer::read_int64() -> std::tuple<bool, int64_t>
    {
        auto [success, value] = read_uint64();
        return {success, static_cast<int64_t>(value)};
    }

    auto BinaryBuffer::read_float() -> std::tuple<bool, float>
    {
        auto [success, bits] = read_uint32();
        if (!success)
        {
            return {false, 0.0f};
        }
        
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        return {true, value};
    }

    auto BinaryBuffer::read_double() -> std::tuple<bool, double>
    {
        auto [success, bits] = read_uint64();
        if (!success)
        {
            return {false, 0.0};
        }
        
        double value;
        std::memcpy(&value, &bits, sizeof(double));
        return {true, value};
    }

    auto BinaryBuffer::read_bool() -> std::tuple<bool, bool>
    {
        auto [success, value] = read_uint8();
        return {success, value != 0};
    }

    auto BinaryBuffer::read_string() -> std::tuple<bool, std::string>
    {
        auto [length_success, length] = read_uint32();
        if (!length_success || !can_read(length))
        {
            return {false, ""};
        }
        
        std::string result(reinterpret_cast<const char*>(&buffer_[read_position_]), length);
        read_position_ += length;
        return {true, result};
    }

    auto BinaryBuffer::read_bytes(size_t length) -> std::tuple<bool, std::vector<uint8_t>>
    {
        if (!can_read(length))
        {
            return {false, std::vector<uint8_t>()};
        }
        
        std::vector<uint8_t> result(buffer_.begin() + read_position_, buffer_.begin() + read_position_ + length);
        read_position_ += length;
        return {true, result};
    }

    auto BinaryBuffer::get_data() const -> const std::vector<uint8_t>&
    {
        return buffer_;
    }

    auto BinaryBuffer::get_size() const -> size_t
    {
        return buffer_.size();
    }

    auto BinaryBuffer::get_capacity() const -> size_t
    {
        return buffer_.capacity();
    }

    auto BinaryBuffer::clear() -> void
    {
        buffer_.clear();
        read_position_ = 0;
    }

    auto BinaryBuffer::reset_read_position() -> void
    {
        read_position_ = 0;
    }

    auto BinaryBuffer::get_read_position() const -> size_t
    {
        return read_position_;
    }

    auto BinaryBuffer::set_read_position(size_t pos) -> bool
    {
        if (pos > buffer_.size())
        {
            return false;
        }
        read_position_ = pos;
        return true;
    }

    auto BinaryBuffer::ensure_capacity(size_t additional_bytes) -> void
    {
        size_t required_size = buffer_.size() + additional_bytes;
        if (required_size > buffer_.capacity())
        {
            buffer_.reserve(required_size * 2);
        }
    }

    auto BinaryBuffer::can_read(size_t bytes) const -> bool
    {
        return read_position_ + bytes <= buffer_.size();
    }
}