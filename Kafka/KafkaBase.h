#pragma once

#include "ThreadPool.h"

#include "KafkaConfig.h"
#include "KafkaMessage.h"
#include "KafkaCommon.h"

#include <kafka/KafkaConsumer.h>
#include <kafka/KafkaProducer.h>

#include <tuple>
#include <string>
#include <optional>
#include <expected>
#include <mutex>
#include <future>

namespace Kafka
{
	enum class KafkaStatus
	{
		Disconnected,
		Connecting,
		Connected,
		Disconnecting,
		Error
	};
	class KafkaBase
	{
	public:
		KafkaBase(const KafkaConfig& config);
		virtual ~KafkaBase();

		auto start() -> std::expected<void, std::string>;
		auto wait_stop() -> std::expected<void, std::string>;
		auto stop() -> std::expected<void, std::string>;

        auto get_status() const -> KafkaStatus { return status_; }
        auto is_connected() const -> bool { return status_ == KafkaStatus::Connected; }

	protected:
		virtual auto connect() -> std::expected<void, std::string> = 0;
		virtual auto disconnect() -> std::expected<void, std::string> = 0;

		auto create_thread_pool() -> std::expected<void, std::string>;
		auto destroy_thread_pool() -> std::expected<void, std::string>;

		std::shared_ptr<Thread::ThreadPool> thread_pool_;

		std::unique_ptr<kafka::clients::producer::KafkaProducer> producer_;
		std::unique_ptr<kafka::clients::consumer::KafkaConsumer> consumer_;

		std::mutex stop_mutex_;
		std::unique_ptr<std::promise<void>> stop_promise_;
		std::future<void> stop_future_;

		KafkaStatus status_;
		KafkaConfig config_;
	};
}