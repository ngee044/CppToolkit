#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Utilities
{
	class SystemInformation
	{
	public:
		static uint64_t memory_usage(void);
		static auto get_ip_addresses(void) -> std::vector<std::string>;
	};
}
