#pragma once

#pragma once

#include "KafkaBase.h"

#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include <expected>
#include <chrono>

namespace Kafka
{
	using MessageCallback = std::function<void(const KafkaMessage&)>;
	using ErrorCallback = std::function<void(KafkaError, const std::string&)>;

	class KafkaQueueConsume : public KafkaBase
	{
	public:
		KafkaQueueConsume(const KafkaConfig& config);
		virtual ~KafkaQueueConsume();

		auto subscribe(const std::string& topic) -> std::expected<void, std::string>;
		auto unsubscribe() -> std::expected<void, std::string>;

		auto poll(std::chrono::milliseconds timeout_ms) -> std::vector<KafkaMessage>;

		auto commit_sync() -> void;
		auto commit_async() -> void;

		auto close() -> void;

	protected:
		auto connect() -> std::expected<void, std::string> override;
		auto disconnect() -> std::expected<void, std::string> override;

	};
}