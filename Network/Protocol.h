#pragma once
#include <string>
#include <cstdint>

namespace Network
{
	using SessionId = std::uint64_t;

	enum class SessionState : std::uint8_t
	{
		Create = 0,
		Handshaking,
		Authenticated,
		InGame,
		Closing,
		Closed
	};

	inline const char* to_string(SessionState state)
	{
		switch (state)
		{
		case SessionState::Create:
			return "Create";
		case SessionState::Handshaking:
			return "Handshaking";
		case SessionState::Authenticated:
			return "Authenticated";
		case SessionState::InGame:
			return "InGame";
		case SessionState::Closing:
			return "Closing";
		case SessionState::Closed:
			return "Closed";
		default:
			return "Unknown";
		}
	}

}