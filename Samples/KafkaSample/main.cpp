#include <iostream>
#include <chrono>
#include <thread>
#include <expected>
#include "KafkaQueueEmitter.h"
#include "KafkaQueueConsume.h"
#include "KafkaConfig.h"
#include "KafkaMessage.h"

using namespace Kafka;

int main()
{
	Kafka::KafkaConfig producer_config("127.0.0.1:9092");
	producer_config.set_topic("test_topic");
	producer_config.add_config("acks", "all");
	// enable.idempotence is implicitly true with acks=all on Kafka 3.x+               

	Kafka::KafkaQueueEmitter producer(producer_config);

	{
		auto start_result = producer.start();
		if (!start_result)
		{
			std::cerr << "Producer start failed: "
					<< start_result.error() << std::endl;
			return -1;
		}
	}

	{
		Kafka::KafkaMessage message("test_topic", "myKey", "Hello Kafka from sample code!");

		DeliveryResult delivery_result(DeliveryResult::Status::Failed, "not attempted", {});
		for (int attempt = 1; attempt <= 5; ++attempt)
		{
			delivery_result = producer.send(message);
			if (delivery_result.get_status() == DeliveryResult::Status::Success)
			{
				break;
			}
			std::cerr << "Send attempt " << attempt << " failed, retrying in 3s..." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
		if (delivery_result.get_status() != DeliveryResult::Status::Success)
		{
			std::cerr << "Send failed: " << delivery_result.get_message() << std::endl;
			if (delivery_result.get_error().has_value())
			{
				std::cerr << "Error: " << delivery_result.get_error().value() << std::endl;
			}
		}
		else
		{
			std::cout << delivery_result.get_message() << std::endl;
		}
	}

	// 4) Consumer
	Kafka::KafkaConfig consumer_config("127.0.0.1:9092");
	consumer_config.set_topic("test_topic");
	consumer_config.set_group_id("test_consumer_group");
	// auto.offset.reset, enable.auto.commit 등 원하는 설정을 추가
	consumer_config.add_config("auto.offset.reset", "earliest"); // 예시

	Kafka::KafkaQueueConsume consumer(consumer_config);

	// 5) Consumer start + connect + subscribe
	{
		auto start_result = consumer.start();
		if (!start_result)
		{
			std::cerr << "Consumer start failed: "
					<< start_result.error() << std::endl;
			return -1;
		}

		auto sub_result = consumer.subscribe("test_topic");
		if (!sub_result)
		{
			std::cerr << "Consumer subscribe failed: "
					<< sub_result.error() << std::endl;
			return -1;
		}
	}

	// 6) Message polling
	{
		std::cout << "Polling messages..." << std::endl;
		auto messages = consumer.poll(std::chrono::milliseconds(2000));
		for (auto& msg : messages)
		{
			std::cout << "[Consumer] Received: "
					<< "topic=" << msg.topic()
					<< ", key=" << msg.key()
					<< ", value=" << msg.value()
					<< ", partition=" << msg.partition()
					<< ", timestamp=" << msg.timestamp()
					<< std::endl;

			auto headers = msg.get_headers();
			for (auto&& [h_key, h_val] : headers)
			{
				std::cout << "   header: " << h_key << " = " << h_val << std::endl;
			}
		}
	}

	// 7) stop & disconnect
	{
		// 필요 시, 전송 대기 중인 메시지 flush
		producer.flush();

		// stop() + wait_stop() 등
		producer.stop();
		//producer.wait_stop();  // 필요하면 호출
	}

	// Consumer 정리
	{
		consumer.stop();
		//consumer.wait_stop();  // 필요하면 호출
	}

	std::cout << "Sample main completed." << std::endl;
	return 0;
}
