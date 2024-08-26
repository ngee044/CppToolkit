#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

namespace Utilities
{
	class Combiner
	{
	public:
		static auto append(std::vector<uint8_t>& result, const std::vector<uint8_t>& source) -> void;
		static auto divide(const std::vector<uint8_t>& source, size_t& index) -> std::vector<uint8_t>;
	};
}
