#pragma once

#include <stdint.h>

namespace Network
{
	enum class ConnectConditions : uint8_t
	{
		None,
		Waiting,
		Expired,
		Confirmed
	};
}