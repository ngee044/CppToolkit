#pragma once

#pragma once

#include "KafkaBase.h"

#include <kafka/KafkaConsumer.h>

#include <memory>
#include <atomic>
#include <string>
#include <tuple>
#include <optional>

namespace Kafka
{
	using MessageCallback = std::function<void(const KafkaMessage&)>;
	using ErrorCallback = std::function<void(KafkaError, const std::string&)>;

	class KafkaConsumer : public KafkaBase
	{
	public:
		KafkaConsumer(const KafkaConfig& config);
		~KafkaConsumer();

		auto subscribe(const std::string& topic) -> std::tuple<bool, std::optional<std::string>>;
		auto unsubscribe() -> std::tuple<bool, std::optional<std::string>>;

		std::vector<KafkaMessage> poll(std::chrono::milliseconds timeout_ms);

		auto commit_sync() -> void;
		auto commit_async() -> void;

		auto is_connected() -> bool;
		auto close();
	};
} 