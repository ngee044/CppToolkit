#include "KafkaQueueEmitter.h"

#include "Logger.h"

#include <format>

using namespace Utilities;

namespace Kafka
{
	KafkaQueueEmitter::KafkaQueueEmitter(const KafkaConfig& config)
		: KafkaBase(config)
	{
		Logger::handle().write(LogTypes::Information, "KafkaQueueEmitter initialized");
	}

	KafkaQueueEmitter::~KafkaQueueEmitter()
	{
		Logger::handle().write(LogTypes::Information, "KafkaQueueEmitter disconnect");
		disconnect();
	}

	auto KafkaQueueEmitter::send(const KafkaMessage& message) -> DeliveryResult
	{
		if (!is_connected() || producer_ == nullptr)
		{
			Logger::handle().write(LogTypes::Error, "Failed to send message");
			return DeliveryResult(
				DeliveryResult::Status::Failed,
				"Failed to send message",
				"Producer is not connected"
			);
		}

		try
		{
			auto record = create_producer_record(message);

			auto delivery_promise = std::make_shared<std::promise<DeliveryResult>>();
			auto delivery_future = delivery_promise->get_future();

			producer_->send(record,
				[delivery_promise](const kafka::clients::producer::RecordMetadata& metadata, const kafka::Error& error)
				{
					if (!error)
					{
						delivery_promise->set_value(DeliveryResult(
							DeliveryResult::Status::Success,
							std::format("Message delivered: {}", metadata.toString()),
							{}
						));
					}
					else
					{
						delivery_promise->set_value(DeliveryResult(
							DeliveryResult::Status::Failed,
							"Message delivery failed",
							error.message()
						));
					}
				},
				kafka::clients::producer::KafkaProducer::SendOption::ToCopyRecordValue);

			producer_->flush();

			if (delivery_future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout)
			{
				return DeliveryResult(
					DeliveryResult::Status::Failed,
					"Message delivery timed out",
					"flush completed but delivery callback not received within 10 seconds"
				);
			}

			return delivery_future.get();
		}
		catch (const kafka::KafkaException& e)
		{
			Logger::handle().write(LogTypes::Error, e.what());
			return DeliveryResult(
				DeliveryResult::Status::Failed,
				"KafkaException on send",
				e.what()
			);
		}
	}

	auto KafkaQueueEmitter::send_batch(const std::vector<KafkaMessage>& messages) -> std::vector<DeliveryResult>
	{
		if (!is_connected() || producer_ == nullptr)
		{
			std::vector<DeliveryResult> results;
			for (size_t i = 0; i < messages.size(); ++i)
			{
				results.emplace_back(DeliveryResult::Status::Failed, "Producer is not connected", std::string{});
			}
			return results;
		}

		std::vector<std::shared_ptr<std::promise<DeliveryResult>>> promises;
		promises.reserve(messages.size());

		for (const auto& message : messages)
		{
			try
			{
				auto record = create_producer_record(message);
				auto delivery_promise = std::make_shared<std::promise<DeliveryResult>>();
				promises.push_back(delivery_promise);

				producer_->send(record,
					[delivery_promise](const kafka::clients::producer::RecordMetadata& metadata, const kafka::Error& error)
					{
						if (!error)
						{
							delivery_promise->set_value(DeliveryResult(
								DeliveryResult::Status::Success,
								std::format("Message delivered: {}", metadata.toString()),
								{}
							));
						}
						else
						{
							delivery_promise->set_value(DeliveryResult(
								DeliveryResult::Status::Failed,
								"Message delivery failed",
								error.message()
							));
						}
					},
					kafka::clients::producer::KafkaProducer::SendOption::ToCopyRecordValue);
			}
			catch (const kafka::KafkaException& e)
			{
				auto error_promise = std::make_shared<std::promise<DeliveryResult>>();
				error_promise->set_value(DeliveryResult(DeliveryResult::Status::Failed, "KafkaException on send", e.what()));
				promises.push_back(error_promise);
			}
		}

		producer_->flush();

		std::vector<DeliveryResult> delivery_results;
		delivery_results.reserve(promises.size());
		for (auto& promise : promises)
		{
			auto future = promise->get_future();
			if (future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout)
			{
				delivery_results.emplace_back(DeliveryResult::Status::Failed, "Message delivery timed out", std::string{});
			}
			else
			{
				delivery_results.push_back(future.get());
			}
		}

		return delivery_results;
	}

	auto KafkaQueueEmitter::flush() -> void
	{
		if (producer_== nullptr)
		{
			Logger::handle().write(LogTypes::Error, "KafkaQueueEmitter is nullptr");
			return;
		}

		producer_->flush();
	}

	auto KafkaQueueEmitter::close() -> void
	{
		if (!is_connected() || producer_ == nullptr)
		{
			return;
		}

		disconnect();
	}

	auto KafkaQueueEmitter::connect() -> std::expected<void, std::string>
	{
		Logger::handle().write(LogTypes::Information, "Connecting Kafka Producer");

		if (producer_ != nullptr)
		{
			return {};
		}

		try
		{
			auto properties = config_.get_properties();
			properties.put("topic.metadata.refresh.interval.ms", "500");

			producer_ = std::make_unique<kafka::clients::producer::KafkaProducer>(properties);
			if (producer_ != nullptr)
			{
				status_ = KafkaStatus::Connected;

				// Warm up: trigger metadata fetch for the configured topic
				producer_->flush();
				std::this_thread::sleep_for(std::chrono::seconds(2));

				Logger::handle().write(LogTypes::Information, "Kafka Producer Connected");
			}

			return {};
		}
		catch (const kafka::KafkaException& e)
		{
			std::string error_message = std::format("Producer Connect Error = {}", e.what());
			Logger::handle().write(LogTypes::Error, error_message);
			return std::unexpected(error_message);
		}
	}

	auto KafkaQueueEmitter::disconnect() -> std::expected<void, std::string>
	{
		if (status_ == KafkaStatus::Disconnected)
		{
			return {};
		}

		if (producer_ == nullptr)
		{
			return std::unexpected("producer is nullptr");
		}

		status_ = KafkaStatus::Disconnecting;

		try
		{
			producer_->flush();
			producer_->close();
			producer_.reset();

			status_ = KafkaStatus::Disconnected;

			Logger::handle().write(LogTypes::Information, "Kafka Producer Disconnected");

			return {};
		}
		catch(const kafka::KafkaException& e)
		{
			std::string error_message = std::format("Error Disconnecting producer: {}", e.what());
			Logger::handle().write(LogTypes::Error, error_message);
			return std::unexpected(error_message);
		}

	}

	auto KafkaQueueEmitter::create_producer_record(const KafkaMessage& message) -> kafka::clients::producer::ProducerRecord
	{
		kafka::clients::producer::ProducerRecord record(message.topic(),
														message.key().empty() ? kafka::NullKey
																			  : kafka::Key(message.key().c_str(), message.key().size()),
														kafka::Value(message.value().data(), message.value().size()));

		if (message.partition() > 0)
		{
			record.setPartition(message.partition());
		}

		for (auto&& header : message.get_headers())
		{
			auto& [h_key, h_val] = header;
			record.headers().push_back(
				kafka::Header(
				kafka::Header::Key(h_key.c_str(), h_key.size()),
				kafka::Header::Value(h_val.c_str(), h_val.size())
				)
			);
		}

		return record;
	}


}