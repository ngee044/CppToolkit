#pragma once

#pragma once

#include "KafkaConfig.h"

#include <kafka/KafkaConsumer.h>

#include <memory>
#include <atomic>
#include <string>
#include <tuple>
#include <optional>

namespace Kafka
{
	class KafkaConsumer
	{
	public:
		KafkaConsumer(const KafkaConfig& config);
		~KafkaConsumer();

	private:
		bool initialized_;
		KafkaConfig config_;

		std::unique_ptr<kafka::clients::consumer::KafkaConsumer> consumer_;
		std::atomic<bool> running_;
	};
} 