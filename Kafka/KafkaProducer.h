#pragma once
#include "KafkaBase.h"

#include "DeliveryResult.hpp"

#include <future>
#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <queue>


namespace Kafka
{
	class KafkaProducer : public KafkaBase
	{
	public:
		KafkaProducer(const KafkaConfig& config);
		virtual ~KafkaProducer();

		auto send(const KafkaMessage& message) -> DeliveryResult;
		auto send_batch(const std::vector<KafkaMessage>& messages) -> std::vector<DeliveryResult>;

		auto flush() -> void;
		auto close() -> void;

	protected:
		auto connect() -> std::tuple<bool, std::optional<std::string>> override;
		auto disconnect() -> std::tuple<bool, std::optional<std::string>> override;

		auto create_producer_record(const KafkaMessage& message) -> kafka::clients::producer::ProducerRecord;
		

	};
} 