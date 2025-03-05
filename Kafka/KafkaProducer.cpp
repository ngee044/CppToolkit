#include "KafkaProducer.h"

#include "Logger.h"

#include "fmt/format.h"

using namespace Utilities;

namespace Kafka
{
	KafkaProducer::KafkaProducer(const KafkaConfig& config)
		: KafkaBase(config)
	{
		Logger::handle().write(LogTypes::Information, "KafkaProducer initialized");
	}

	KafkaProducer::~KafkaProducer()
	{
		if (is_connected())
		{
			Logger::handle().write(LogTypes::Information, "KafkaProducer disconnect");
			disconnect();
		}
	}

	auto KafkaProducer::send(const KafkaMessage& message) -> DeliveryResult
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
			auto delivery_callback = [](const kafka::clients::producer::RecordMetadata& metadata, const kafka::Error& error) 
			{
				if (!error) 
				{
					std::cout << "Message delivered: " << metadata.toString() << std::endl;
				} 
				else 
				{
					std::cerr << "Message failed to be delivered: " << error.message() << std::endl;
				}
			};

			producer_->send(record, delivery_callback);
			
			return DeliveryResult(
                DeliveryResult::Status::Success,
                "Message queued for delivery",
                {}
            );
			
		}
		catch(const kafka::KafkaException& e)
		{
            Logger::handle().write(LogTypes::Error, e.what());
            return DeliveryResult(
                DeliveryResult::Status::Failed,
                "KafkaException on send",
                e.what()
            );
		}
		
	}

	auto KafkaProducer::send_batch(const std::vector<KafkaMessage>& messages) -> std::vector<DeliveryResult>
	{
		std::vector<DeliveryResult> delivery_results;
		for (const auto& message : messages)
		{
			auto delivery_result = send(message);
			delivery_results.push_back(delivery_result);
			if (delivery_result.get_status() != DeliveryResult::Status::Success)
			{
				Logger::handle().write(LogTypes::Debug, fmt::format("failed send message (kafka) = {}", delivery_result.get_message()));
				if (delivery_result.get_error().has_value())
				{
					Logger::handle().write(LogTypes::Error, fmt::format("kafka producer send error = {}", delivery_result.get_error().value()));
				}
			}
		}

		return delivery_results;
	}

	auto KafkaProducer::flush() -> void
	{
		if (producer_== nullptr)
		{
			Logger::handle().write(LogTypes::Error, "KafkaProducer is nullptr");
			return;
		}

		producer_->flush();
	}

	auto KafkaProducer::close() -> void
	{
		disconnect();

	}

	auto KafkaProducer::connect() -> std::tuple<bool, std::optional<std::string>>
	{
		Logger::handle().write(LogTypes::Information, "Connecting Kafka Producer");

		if (producer_ != nullptr)
		{
			return { true, std::nullopt};
		}

		try
		{
			producer_ = std::make_unique<kafka::clients::producer::KafkaProducer>(config_.get_properties());
			
			return { true, std::nullopt };
		}
		catch(const kafka::KafkaException& e)
		{
			std::string error_message = fmt::format("Producer Connect Error = {}", e.what());
			Logger::handle().write(LogTypes::Error, error_message);
			return { false, error_message };
		}
	}

	auto KafkaProducer::disconnect() -> std::tuple<bool, std::optional<std::string>>
	{
		Logger::handle().write(LogTypes::Information, "DisConnecting Kafka Producer");

		if (producer_ == nullptr)
		{
			return { false, "producer is nullptr"};
		}

		try
		{
			producer_->flush();
			producer_->close();
			producer_.reset();

			return { true, std::nullopt };
		}
		catch(const kafka::KafkaException& e)
		{
			std::string error_message = fmt::format("Error Disconnecting producer: {}", e.what());
			Logger::handle().write(LogTypes::Error, error_message);
			return {false, error_message};
		}
		
	}

	auto KafkaProducer::create_producer_record(const KafkaMessage& message) -> kafka::clients::producer::ProducerRecord
	{
		auto line = message.value();

		kafka::clients::producer::ProducerRecord record(message.topic(), 
														message.key().empty() ? kafka::NullKey 
																			  : kafka::Key(message.key().c_str(), message.key().size()), 
														kafka::Value(line.c_str(), line.size()));

		if (message.partition() >= 0)
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