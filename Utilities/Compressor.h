#pragma once

#include <tuple>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace Utilities
{
	class Compressor
	{
	public:
		static auto compression(const std::vector<uint8_t>& original_data, const uint16_t& block_bytes = 1024)
			-> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>;
		static auto decompression(const std::vector<uint8_t>& compressed_data, const uint16_t& block_bytes = 1024)
			-> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>;
	};
}
