#pragma once

#include <string>
#include <functional>
#include <tuple>

namespace Kafka
{
	enum class KafkaError
	{
		Ok,
		ConfigError,
		ConnectionError,
		TimeoutError,
		UnknownError
	};

	using MessageHeader = std::tuple<std::string, std::string>;
}