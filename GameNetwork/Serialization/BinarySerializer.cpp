#include "BinarySerializer.h"
#include <algorithm>

namespace GameNetwork
{
    namespace Serialization
    {
        // Helper function to detect endianness in C++17
        static bool is_little_endian()
        {
            uint16_t test = 0x0001;
            return *reinterpret_cast<uint8_t*>(&test) == 0x01;
        }

        BinarySerializer::BinarySerializer()
            : little_endian_(is_little_endian())
        {
            buffer_.reserve(256);  // Default initial capacity
        }

        BinarySerializer::BinarySerializer(size_t initial_capacity)
            : little_endian_(is_little_endian())
        {
            buffer_.reserve(initial_capacity);
        }

        BinarySerializer::~BinarySerializer() = default;

        auto BinarySerializer::write_uint8(uint8_t value) -> void
        {
            buffer_.push_back(value);
        }

        auto BinarySerializer::write_uint16(uint16_t value) -> void
        {
            ensure_capacity(sizeof(uint16_t));
            if (little_endian_)
            {
                buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
                buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            }
            else
            {
                buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
                buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
            }
        }
        auto BinarySerializer::write_uint32(uint32_t value) -> void
        {
            ensure_capacity(sizeof(uint32_t));
            if (little_endian_)
            {
                buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
                buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
                buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
                buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
            }
            else
            {
                buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
                buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
                buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
                buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
            }
        }

        auto BinarySerializer::write_uint64(uint64_t value) -> void
        {
            ensure_capacity(sizeof(uint64_t));
            if (little_endian_)
            {
                for (int i = 0; i < 8; ++i)
                {
                    buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
                }
            }
            else
            {
                for (int i = 7; i >= 0; --i)
                {
                    buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
                }
            }
        }

        auto BinarySerializer::write_int8(int8_t value) -> void
        {
            write_uint8(static_cast<uint8_t>(value));
        }

        auto BinarySerializer::write_int16(int16_t value) -> void
        {
            write_uint16(static_cast<uint16_t>(value));
        }

        auto BinarySerializer::write_int32(int32_t value) -> void
        {
            write_uint32(static_cast<uint32_t>(value));
        }

        auto BinarySerializer::write_int64(int64_t value) -> void
        {
            write_uint64(static_cast<uint64_t>(value));
        }

        auto BinarySerializer::write_float(float value) -> void
        {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(float));
            write_uint32(bits);
        }

        auto BinarySerializer::write_double(double value) -> void
        {
            uint64_t bits;
            std::memcpy(&bits, &value, sizeof(double));
            write_uint64(bits);
        }

        auto BinarySerializer::write_bool(bool value) -> void
        {
            write_uint8(value ? 1 : 0);
        }

        auto BinarySerializer::write_string(const std::string& value) -> void
        {
            // Write length as varint for efficiency
            write_varint(value.length());
            write_bytes(reinterpret_cast<const uint8_t*>(value.data()), value.length());
        }

        auto BinarySerializer::write_bytes(const uint8_t* data, size_t length) -> void
        {
            ensure_capacity(length);
            buffer_.insert(buffer_.end(), data, data + length);
        }

        auto BinarySerializer::write_varint(uint64_t value) -> void
        {
            while (value >= 0x80)
            {
                buffer_.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
                value >>= 7;
            }
            buffer_.push_back(static_cast<uint8_t>(value & 0x7F));
        }

        auto BinarySerializer::write_signed_varint(int64_t value) -> void
        {
            // Use zigzag encoding for signed values
            uint64_t encoded = (static_cast<uint64_t>(value) << 1) ^ (value >> 63);
            write_varint(encoded);
        }

        auto BinarySerializer::get_data() const -> const std::vector<uint8_t>&
        {
            return buffer_;
        }

        auto BinarySerializer::get_size() const -> size_t
        {
            return buffer_.size();
        }

        auto BinarySerializer::extract_data() -> std::vector<uint8_t>
        {
            return std::move(buffer_);
        }

        auto BinarySerializer::clear() -> void
        {
            buffer_.clear();
        }

        auto BinarySerializer::reserve(size_t capacity) -> void
        {
            buffer_.reserve(capacity);
        }

        auto BinarySerializer::ensure_capacity(size_t additional_bytes) -> void
        {
            if (buffer_.size() + additional_bytes > buffer_.capacity())
            {
                buffer_.reserve(buffer_.capacity() * 2);
            }
        }

        // BinaryDeserializer implementation
        BinaryDeserializer::BinaryDeserializer(const std::vector<uint8_t>& data)
            : data_(data.data())
            , size_(data.size())
            , position_(0)
            , little_endian_(is_little_endian())
        {
        }

        BinaryDeserializer::BinaryDeserializer(const uint8_t* data, size_t size)
            : data_(data)
            , size_(size)
            , position_(0)
            , little_endian_(is_little_endian())
        {
        }

        BinaryDeserializer::~BinaryDeserializer() = default;

        auto BinaryDeserializer::read_uint8() -> std::tuple<uint8_t, bool>
        {
            if (!can_read(sizeof(uint8_t)))
            {
                return {0, false};
            }
            return {data_[position_++], true};
        }

        auto BinaryDeserializer::read_uint16() -> std::tuple<uint16_t, bool>
        {
            if (!can_read(sizeof(uint16_t)))
            {
                return {0, false};
            }
            
            uint16_t value;
            if (little_endian_)
            {
                value = static_cast<uint16_t>(data_[position_]) |
                        (static_cast<uint16_t>(data_[position_ + 1]) << 8);
            }
            else
            {
                value = (static_cast<uint16_t>(data_[position_]) << 8) |
                        static_cast<uint16_t>(data_[position_ + 1]);
            }
            position_ += sizeof(uint16_t);
            return {value, true};
        }

        auto BinaryDeserializer::read_uint32() -> std::tuple<uint32_t, bool>
        {
            if (!can_read(sizeof(uint32_t)))
            {
                return {0, false};
            }
            
            uint32_t value;
            if (little_endian_)
            {
                value = static_cast<uint32_t>(data_[position_]) |
                        (static_cast<uint32_t>(data_[position_ + 1]) << 8) |
                        (static_cast<uint32_t>(data_[position_ + 2]) << 16) |
                        (static_cast<uint32_t>(data_[position_ + 3]) << 24);
            }
            else
            {
                value = (static_cast<uint32_t>(data_[position_]) << 24) |
                        (static_cast<uint32_t>(data_[position_ + 1]) << 16) |
                        (static_cast<uint32_t>(data_[position_ + 2]) << 8) |
                        static_cast<uint32_t>(data_[position_ + 3]);
            }
            position_ += sizeof(uint32_t);
            return {value, true};
        }

        auto BinaryDeserializer::read_uint64() -> std::tuple<uint64_t, bool>
        {
            if (!can_read(sizeof(uint64_t)))
            {
                return {0, false};
            }
            
            uint64_t value = 0;
            if (little_endian_)
            {
                for (int i = 0; i < 8; ++i)
                {
                    value |= static_cast<uint64_t>(data_[position_ + i]) << (i * 8);
                }
            }
            else
            {
                for (int i = 0; i < 8; ++i)
                {
                    value |= static_cast<uint64_t>(data_[position_ + i]) << ((7 - i) * 8);
                }
            }
            position_ += sizeof(uint64_t);
            return {value, true};
        }
        auto BinaryDeserializer::read_int8() -> std::tuple<int8_t, bool>
        {
            auto [value, success] = read_uint8();
            return {static_cast<int8_t>(value), success};
        }

        auto BinaryDeserializer::read_int16() -> std::tuple<int16_t, bool>
        {
            auto [value, success] = read_uint16();
            return {static_cast<int16_t>(value), success};
        }

        auto BinaryDeserializer::read_int32() -> std::tuple<int32_t, bool>
        {
            auto [value, success] = read_uint32();
            return {static_cast<int32_t>(value), success};
        }

        auto BinaryDeserializer::read_int64() -> std::tuple<int64_t, bool>
        {
            auto [value, success] = read_uint64();
            return {static_cast<int64_t>(value), success};
        }

        auto BinaryDeserializer::read_float() -> std::tuple<float, bool>
        {
            auto [bits, success] = read_uint32();
            if (!success)
            {
                return {0.0f, false};
            }
            float value;
            std::memcpy(&value, &bits, sizeof(float));
            return {value, true};
        }

        auto BinaryDeserializer::read_double() -> std::tuple<double, bool>
        {
            auto [bits, success] = read_uint64();
            if (!success)
            {
                return {0.0, false};
            }
            double value;
            std::memcpy(&value, &bits, sizeof(double));
            return {value, true};
        }

        auto BinaryDeserializer::read_bool() -> std::tuple<bool, bool>
        {
            auto [value, success] = read_uint8();
            return {value != 0, success};
        }

        auto BinaryDeserializer::read_string() -> std::tuple<std::string, bool>
        {
            // Read length as varint
            auto [length, length_success] = read_varint();
            if (!length_success || !can_read(length))
            {
                return {"", false};
            }
            
            std::string result(reinterpret_cast<const char*>(data_ + position_), length);
            position_ += length;
            return {result, true};
        }

        auto BinaryDeserializer::read_bytes(size_t length) -> std::tuple<std::vector<uint8_t>, bool>
        {
            if (!can_read(length))
            {
                return {{}, false};
            }
            
            std::vector<uint8_t> result(data_ + position_, data_ + position_ + length);
            position_ += length;
            return {result, true};
        }

        auto BinaryDeserializer::read_varint() -> std::tuple<uint64_t, bool>
        {
            uint64_t value = 0;
            int shift = 0;
            
            while (shift < 64)
            {
                if (!can_read(1))
                {
                    return {0, false};
                }
                
                uint8_t byte = data_[position_++];
                value |= static_cast<uint64_t>(byte & 0x7F) << shift;
                
                if ((byte & 0x80) == 0)
                {
                    return {value, true};
                }
                
                shift += 7;
            }
            
            return {0, false};  // Varint too long
        }

        auto BinaryDeserializer::read_signed_varint() -> std::tuple<int64_t, bool>
        {
            auto [encoded, success] = read_varint();
            if (!success)
            {
                return {0, false};
            }
            
            // Decode zigzag encoding
            int64_t value = (encoded >> 1) ^ -(encoded & 1);
            return {value, true};
        }

        auto BinaryDeserializer::get_position() const -> size_t
        {
            return position_;
        }

        auto BinaryDeserializer::set_position(size_t pos) -> bool
        {
            if (pos > size_)
            {
                return false;
            }
            position_ = pos;
            return true;
        }

        auto BinaryDeserializer::skip(size_t bytes) -> bool
        {
            if (!can_read(bytes))
            {
                return false;
            }
            position_ += bytes;
            return true;
        }

        auto BinaryDeserializer::remaining_bytes() const -> size_t
        {
            return size_ - position_;
        }

        auto BinaryDeserializer::is_end() const -> bool
        {
            return position_ >= size_;
        }

        auto BinaryDeserializer::reset() -> void
        {
            position_ = 0;
        }

        auto BinaryDeserializer::can_read(size_t bytes) const -> bool
        {
            return position_ + bytes <= size_;
        }
    }
}