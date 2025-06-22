#pragma once

#include <GameNetworkConstants.h>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace GameNetwork
{
    namespace Compression
    {
        class BitPacker
        {
        public:
            BitPacker();
            explicit BitPacker(size_t initial_capacity);
            virtual ~BitPacker() = default;

            // Write operations
            auto write_bits(uint32_t value, uint8_t num_bits) -> void;
            auto write_bool(bool value) -> void;
            auto write_uint8(uint8_t value) -> void;
            auto write_uint16(uint16_t value) -> void;
            auto write_uint32(uint32_t value) -> void;
            auto write_uint64(uint64_t value) -> void;
            
            // Optimized float packing
            auto pack_float(float value, uint8_t precision_bits) -> void;
            auto pack_float_range(float value, float min, float max, uint8_t bits) -> void;
            
            // Vector packing
            auto pack_vector2(const glm::vec2& vec, uint8_t precision_bits) -> void;
            auto pack_vector3(const glm::vec3& vec, uint8_t precision_bits) -> void;
            
            // Quaternion packing (only 3 components needed)
            auto pack_quaternion(const glm::quat& rotation) -> void;
            // Angle packing
            auto pack_angle(float angle_radians, uint8_t bits) -> void;
            
            // Get packed data
            auto get_data() const -> std::vector<uint8_t>;
            auto get_bit_count() const -> size_t;
            auto get_byte_count() const -> size_t;
            
            // Reset
            auto clear() -> void;

        private:
            std::vector<uint8_t> buffer_;
            size_t bit_position_;
            
            auto ensure_capacity(size_t bits_needed) -> void;
        };

        class BitUnpacker
        {
        public:
            explicit BitUnpacker(const std::vector<uint8_t>& data);
            virtual ~BitUnpacker() = default;

            // Read operations
            auto read_bits(uint8_t num_bits) -> uint32_t;
            auto read_bool() -> bool;
            auto read_uint8() -> uint8_t;
            auto read_uint16() -> uint16_t;
            auto read_uint32() -> uint32_t;
            auto read_uint64() -> uint64_t;
            
            // Float unpacking
            auto unpack_float(uint8_t precision_bits) -> float;
            auto unpack_float_range(float min, float max, uint8_t bits) -> float;
            // Vector unpacking
            auto unpack_vector2(uint8_t precision_bits) -> glm::vec2;
            auto unpack_vector3(uint8_t precision_bits) -> glm::vec3;
            
            // Quaternion unpacking
            auto unpack_quaternion() -> glm::quat;
            
            // Angle unpacking
            auto unpack_angle(uint8_t bits) -> float;
            
            // State
            auto get_bits_read() const -> size_t;
            auto get_bits_remaining() const -> size_t;
            auto is_empty() const -> bool;

        private:
            const std::vector<uint8_t>& data_;
            size_t bit_position_;
            
            auto ensure_bits_available(size_t bits_needed) const -> bool;
        };

        // Helper functions
        namespace BitPackingHelpers
        {
            auto smallest_three_quaternion(const glm::quat& q) -> std::tuple<uint8_t, float, float, float>;
            auto reconstruct_quaternion(uint8_t largest_index, float a, float b, float c) -> glm::quat;
            
            auto quantize_float(float value, float min, float max, uint32_t levels) -> uint32_t;
            auto dequantize_float(uint32_t quantized, float min, float max, uint32_t levels) -> float;
        }
    }
}