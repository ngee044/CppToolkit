#pragma once

#include <stdint.h>

namespace Utilities
{
	enum class LogTypes : uint8_t { None, Exception, Error, Warning, Information, Debug, Sequence, Parameter, Packet, Unknown };
}
