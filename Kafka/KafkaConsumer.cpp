#include "KafkaConsumer.h"
#include "Logger.h"

#include "fmt/format.h"

using namespace Utilities;
namespace Kafka
{
	KafkaConsumer::KafkaConsumer(const KafkaConfig& config)
		: KafkaBase(config)
	{
		status_ = KafkaStatus::Disconnected;
		Logger::handle().write(LogTypes::Information, "KafkaConsumer initalized");
	}

	KafkaConsumer::~KafkaConsumer()
	{
		if (is_connected())
		{
			disconnect();
		}
	}

	auto KafkaConsumer::subscribe() -> std::tuple<bool, std::optional<std::string>>
	{
		return { true, std::nullopt };
	}

	auto KafkaConsumer::unsubscribe() -> std::tuple<bool, std::optional<std::string>>
	{
		return { true, std::nullopt };
	}

	auto KafkaConsumer::connect() -> std::tuple<bool, std::optional<std::string>>
	{
		return { true, std::nullopt };
	}

	auto KafkaConsumer::disconnect() -> std::tuple<bool, std::optional<std::string>>
	{
		return { true, std::nullopt };
	}

	auto KafkaConsumer::poll(std::chrono::milliseconds timeout_ms) -> std::tuple<bool, std::optional<std::string>>
	{
		return { true, std::nullopt };
	}

	auto KafkaConsumer::commit_sync() -> void
	{
	}

	auto KafkaConsumer::commit_async() -> void
	{
	}

	auto KafkaConsumer::close() -> void
	{
    }


}