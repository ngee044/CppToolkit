#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <tuple>
#include <optional>

namespace GameNetwork
{
    namespace Serialization
    {
        enum class SerializationType : uint8_t
        {
            Binary = 0x01,
            Json = 0x02,
            Protobuf = 0x03  // Future extension
        };

        class BinarySerializer
        {
        public:
            BinarySerializer();
            explicit BinarySerializer(size_t initial_capacity);
            ~BinarySerializer();

            // Write operations
            auto write_uint8(uint8_t value) -> void;
            auto write_uint16(uint16_t value) -> void;
            auto write_uint32(uint32_t value) -> void;
            auto write_uint64(uint64_t value) -> void;
            auto write_int8(int8_t value) -> void;
            auto write_int16(int16_t value) -> void;
            auto write_int32(int32_t value) -> void;
            auto write_int64(int64_t value) -> void;
            auto write_float(float value) -> void;
            auto write_double(double value) -> void;
            auto write_bool(bool value) -> void;
            auto write_string(const std::string& value) -> void;
            auto write_bytes(const uint8_t* data, size_t length) -> void;
            
            // Varint encoding for efficient integer storage
            auto write_varint(uint64_t value) -> void;
            auto write_signed_varint(int64_t value) -> void;
            
            // Get serialized data
            auto get_data() const -> const std::vector<uint8_t>&;
            auto get_size() const -> size_t;
            auto extract_data() -> std::vector<uint8_t>;
            
            // Reset the serializer
            auto clear() -> void;
            
            // Reserve capacity
            auto reserve(size_t capacity) -> void;

        private:
            auto ensure_capacity(size_t additional_bytes) -> void;
            
        private:
            std::vector<uint8_t> buffer_;
            bool little_endian_;
        };

        class BinaryDeserializer
        {
        public:
            explicit BinaryDeserializer(const std::vector<uint8_t>& data);
            BinaryDeserializer(const uint8_t* data, size_t size);
            ~BinaryDeserializer();

            // Read operations
            auto read_uint8() -> std::tuple<uint8_t, bool>;
            auto read_uint16() -> std::tuple<uint16_t, bool>;
            auto read_uint32() -> std::tuple<uint32_t, bool>;
            auto read_uint64() -> std::tuple<uint64_t, bool>;
            auto read_int8() -> std::tuple<int8_t, bool>;
            auto read_int16() -> std::tuple<int16_t, bool>;
            auto read_int32() -> std::tuple<int32_t, bool>;
            auto read_int64() -> std::tuple<int64_t, bool>;
            auto read_float() -> std::tuple<float, bool>;
            auto read_double() -> std::tuple<double, bool>;
            auto read_bool() -> std::tuple<bool, bool>;
            auto read_string() -> std::tuple<std::string, bool>;
            auto read_bytes(size_t length) -> std::tuple<std::vector<uint8_t>, bool>;
            
            // Varint decoding
            auto read_varint() -> std::tuple<uint64_t, bool>;
            auto read_signed_varint() -> std::tuple<int64_t, bool>;
            
            // Position management
            auto get_position() const -> size_t;
            auto set_position(size_t pos) -> bool;
            auto skip(size_t bytes) -> bool;
            auto remaining_bytes() const -> size_t;
            auto is_end() const -> bool;
            
            // Reset
            auto reset() -> void;

        private:
            auto can_read(size_t bytes) const -> bool;
            
        private:
            const uint8_t* data_;
            size_t size_;
            size_t position_;
            bool little_endian_;
        };
    }
}