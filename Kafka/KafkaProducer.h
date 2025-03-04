#pragma once
#include "KafkaBase.h"

#include "DeliveryResult.hpp"

#include <future>
#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include <unordered_map>

namespace Kafka
{
	class KafkaProducer : public KafkaBase
	{
	public:
		KafkaProducer(const KafkaConfig& config);
		~KafkaProducer();

		auto send(const KafkaMessage& message);
		auto send_async(const KafkaMessage& message) -> std::future<DeliveryResult>; 
		auto send_async(const KafkaMessage& message, std::function<void(const DeliveryResult&)>);
		auto send_batch(const std::vector<KafkaMessage>& messages) -> std::vector<DeliveryResult>;

		auto is_connected() -> bool;
		auto flush() -> void;
		auto close() -> void;

	private:

	};
} 