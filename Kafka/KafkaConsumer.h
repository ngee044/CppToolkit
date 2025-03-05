#pragma once

#pragma once

#include "KafkaBase.h"

#include <kafka/KafkaConsumer.h>

#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include <chrono>

namespace Kafka
{
	using MessageCallback = std::function<void(const KafkaMessage&)>;
	using ErrorCallback = std::function<void(KafkaError, const std::string&)>;

	class KafkaConsumer : public KafkaBase
	{
	public:
		KafkaConsumer(const KafkaConfig& config);
		~KafkaConsumer();

		auto subscribe() -> std::tuple<bool, std::optional<std::string>>;
		auto unsubscribe() -> std::tuple<bool, std::optional<std::string>>;

		auto poll(std::chrono::milliseconds timeout_ms) -> std::tuple<bool, std::optional<std::string>>;

		auto commit_sync() -> void;
		auto commit_async() -> void;

		auto close() -> void;

	protected:
		auto connect() -> std::tuple<bool, std::optional<std::string>> override;
		auto disconnect() -> std::tuple<bool, std::optional<std::string>> override;

	};
} 