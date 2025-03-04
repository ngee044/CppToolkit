#pragma once

#include "KafkaConfig.h"
#include "KafkaCommon.h"
#include "KafkaMessage.h"

#include <kafka/KafkaProducer.h>


#include <future>
#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include <unordered_map>

namespace Kafka
{
	class SendResult
	{

	};

	class KafkaProducer
	{
	public:
		KafkaProducer(const KafkaConfig& config);
		~KafkaProducer();

		auto send(const KafkaMessage& message);
		auto send_async(const KafkaMessage& message);

		auto is_connected() -> bool;
		auto flush() -> void;
		auto close() -> void;

	private:
		std::unique_ptr<kafka::clients::producer::KafkaProducer> producer_;

	};
} 