#include "Generator.h"

#include <format>
#include <random>

namespace Utilities
{
	auto Generator::guid(void) -> std::string
	{
		std::random_device random;
		std::mt19937 generate(random());
		std::uniform_int_distribution<uint16_t> distrivution(0, 65535);

		uint16_t target_number = distrivution(generate);

		return std::format("{:04X}-{:04X}-{:04X}-{:04X}-{:04X}", distrivution(generate), distrivution(generate), distrivution(generate), distrivution(generate),
						   distrivution(generate));
	}
}
