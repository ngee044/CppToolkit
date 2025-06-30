#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>
#include <optional>

namespace GameNetwork
{
	class BinaryBuffer
	{
	public:
		BinaryBuffer();
		explicit BinaryBuffer(size_t initial_capacity);
		~BinaryBuffer();

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
        
		// Read operations
		auto read_uint8() -> std::tuple<bool, uint8_t>;
		auto read_uint16() -> std::tuple<bool, uint16_t>;
		auto read_uint32() -> std::tuple<bool, uint32_t>;
		auto read_uint64() -> std::tuple<bool, uint64_t>;
		auto read_int8() -> std::tuple<bool, int8_t>;
		auto read_int16() -> std::tuple<bool, int16_t>;
		auto read_int32() -> std::tuple<bool, int32_t>;
		auto read_int64() -> std::tuple<bool, int64_t>;
		auto read_float() -> std::tuple<bool, float>;
		auto read_double() -> std::tuple<bool, double>;
		auto read_bool() -> std::tuple<bool, bool>;
		auto read_string() -> std::tuple<bool, std::string>;
		auto read_bytes(size_t length) -> std::tuple<bool, std::vector<uint8_t>>;
        
		// Buffer management
		auto get_data() const -> const std::vector<uint8_t>&;
		auto get_size() const -> size_t;
		auto get_capacity() const -> size_t;
		auto clear() -> void;
		auto reset_read_position() -> void;
		auto get_read_position() const -> size_t;
		auto set_read_position(size_t pos) -> bool;
        
	private:
		std::vector<uint8_t> buffer_;
		size_t read_position_;
        
		auto ensure_capacity(size_t additional_bytes) -> void;
		auto can_read(size_t bytes) const -> bool;
	};
}