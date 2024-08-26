#pragma once

#include <string>

namespace Utilities
{
	class Generator
	{
	public:
		static auto guid(void) -> std::string;
	};
}
