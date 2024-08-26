#pragma once

#include <stdint.h>

namespace Network
{
	enum class DataModes : uint8_t { Binary, File, Message, Connection };

	enum class FileModes : uint8_t { Start, Success, Failure };
}