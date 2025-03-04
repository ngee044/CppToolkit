#pragma once

#include "KafkaConfig.h"

#include <kafka/KafkaProducer.h>

#include <memory>
#include <atomic>
#include <string>
#include <tuple>
#include <optional>

namespace Kafka
{
	class KafkaProducer
	{
	public:
		KafkaProducer(const KafkaConfig& config);
		~KafkaProducer();

		auto init_producer() -> std::tuple<bool, std::optional<std::string>>;
		auto send_message(const std::string& message) -> std::tuple<bool, std::optional<std::string>>;

		auto flush_messages(int timeout_ms) -> void;

	private:
		bool initialized_;
		KafkaConfig config_;

		std::unique_ptr<kafka::clients::producer::KafkaProducer> producer_;
		std::atomic<bool> running_;
	};
} 