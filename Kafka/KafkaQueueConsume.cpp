#include "KafkaQueueConsume.h"
#include "Logger.h"
#include "Converter.h"

#include <format>

using namespace Utilities;
namespace Kafka
{
	// 외부 라이브러리와 이름 중복으로 인해 교체가 필요하긴함
	// KafkaQueueConsume -> KafkaQueueConsume
	// KafkaProducer -> KafkaQueueEmitter
	KafkaQueueConsume::KafkaQueueConsume(const KafkaConfig& config)
		: KafkaBase(config)
	{
		Logger::handle().write(LogTypes::Information, "KafkaQueueConsume initalized");
	}

	KafkaQueueConsume::~KafkaQueueConsume()
	{
		if (is_connected())
		{
			disconnect();
		}
		Logger::handle().write(LogTypes::Information, "KafkaConsumer disconnected");
	}

	auto KafkaQueueConsume::subscribe(const std::string& topic) -> std::expected<void, std::string>
	{
		if (!is_connected() || consumer_ == nullptr)
		{
			return std::unexpected("Consumer not connected");
		}

		try
		{
			consumer_->subscribe({topic});
			Logger::handle().write(LogTypes::Information,
				std::format("KafkaConsumer Subscribed to topic: {}", topic));

			return {};
		}
		catch (const kafka::KafkaException& e)
		{
			auto msg = std::format("KafkaConsumer subscribe error: {}", e.what());
			Logger::handle().write(LogTypes::Error, msg);

			return std::unexpected(msg);
		}
	}

	auto KafkaQueueConsume::unsubscribe() -> std::expected<void, std::string>
	{
		if (!is_connected() || consumer_ == nullptr)
		{
			return std::unexpected("Consumer not connected");
		}

		try
		{
			consumer_->unsubscribe();
			Logger::handle().write(LogTypes::Information, "KafkaConsumer Unsubscribed.");

			return {};
		}
		catch (const kafka::KafkaException& e)
		{
			auto msg = std::format("KafkaConsumer unsubscribe() error: {}", e.what());
			Logger::handle().write(LogTypes::Error, msg);

			return std::unexpected(msg);
		}
	}

	auto KafkaQueueConsume::connect() -> std::expected<void, std::string>
	{
		Logger::handle().write(LogTypes::Information, "KafkaConsumer Connecting....");

		try
		{
			if (consumer_ != nullptr)
			{
				consumer_.reset();
			}

			consumer_ = std::make_unique<kafka::clients::consumer::KafkaConsumer>(config_.get_properties());
			if (consumer_ != nullptr)
			{
				status_ = KafkaStatus::Connected;
				Logger::handle().write(LogTypes::Information, "KafkaConsumer Connected");
			}

			return {};
		}
		catch (const kafka::KafkaException& e)
		{
			auto message = std::format("[KafkaConsumer] connect() error: {}", e.what());
			Logger::handle().write(LogTypes::Error, message);

			status_ = KafkaStatus::Error;

			return std::unexpected(message);
		}
	}

	auto KafkaQueueConsume::disconnect() -> std::expected<void, std::string>
	{
		if (status_ == KafkaStatus::Disconnected)
		{
			return {};
		}

		if (consumer_ == nullptr)
		{
			return std::unexpected("Consumer is nullptr");
		}

		status_ = KafkaStatus::Disconnecting;

		try
		{
			consumer_->close();
			consumer_.reset();

			status_ = KafkaStatus::Disconnected;

			Logger::handle().write(LogTypes::Information, "KafkaConsumer Disconnected");

			return {};
		}
		catch (const kafka::KafkaException& e)
		{
			auto msg = std::format("KafkaConsumer disconnect() error: {}", e.what());
			Logger::handle().write(LogTypes::Error, msg);

			status_ = KafkaStatus::Error;
			return std::unexpected(msg);
		}
	}

	auto KafkaQueueConsume::poll(std::chrono::milliseconds timeout_ms) -> std::vector<KafkaMessage>
	{
		std::vector<KafkaMessage> messages;

		if (!is_connected() || consumer_ == nullptr)
		{
			return messages;
		}

		try
		{
			auto records = consumer_->poll(timeout_ms);
			for (auto& record : records)
			{
				if (!record.error())
				{
					KafkaMessage kafka_message(
						record.topic(),
						record.key().toString(),
						record.value().toString()
					);

					kafka_message.partition(record.partition());
					kafka_message.timestamp(record.timestamp().msSinceEpoch);

					for (auto&& header : record.headers())
					{
						auto key_buffer = header.key;
						auto value_buffer = header.value;

						kafka_message.add_header(header.key, header.value.toString());
					}

					messages.push_back(std::move(kafka_message));
				}
			}
		}
		catch (const kafka::KafkaException& e)
		{
			auto msg = std::format("[KafkaConsumer] poll() error: {}", e.what());
			Logger::handle().write(LogTypes::Error, msg);
		}

		return messages;
	}

	auto KafkaQueueConsume::commit_sync() -> void
	{
		if (!is_connected() || consumer_ == nullptr)
		{
			return;
		}
		try
		{
			consumer_->commitSync();
		}
		catch(const kafka::KafkaException& e)
		{
			auto msg = std::format("KafkaConsumer commit_sync error: {}", e.what());
			Logger::handle().write(LogTypes::Error, msg);
		}

	}

	auto KafkaQueueConsume::commit_async() -> void
	{
		if (!is_connected() || consumer_ == nullptr)
		{
			return;
		}
		try
		{
			consumer_->commitAsync();
		}
		catch(const kafka::KafkaException& e)
		{
			auto msg = std::format("KafkaConsumer commit_async error: {}", e.what());
			Logger::handle().write(LogTypes::Error, msg);
		}
	}

	auto KafkaQueueConsume::close() -> void
	{
		if (!is_connected() || consumer_ == nullptr)
		{
			return;
		}
		disconnect();
	}


}