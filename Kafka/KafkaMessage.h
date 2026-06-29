#pragma once

#include "KafkaCommon.h"

#include <kafka/KafkaConsumer.h>
#include <kafka/KafkaProducer.h>

namespace Kafka {

class KafkaMessage {
public:
    KafkaMessage(const std::string& topic, const std::string& key, const std::string& value);
    ~KafkaMessage();
    
    auto topic(const std::string& topic) -> void;
	auto topic() const -> const kafka::Topic&;

	auto key(const std::string& key) -> void;
	auto key() const -> const std::string&;

    auto value(const std::string& value) -> void;
	auto value() const -> const std::string&;

    auto partition(int partition) -> void;
	auto partition() const -> kafka::Partition;

    auto timestamp(int64_t timestamp) -> void;
	auto timestamp() const -> int64_t;

    auto add_header(const std::string& key, const std::string& value) -> void;
	auto get_headers() const -> const std::vector<MessageHeader>&;
	
private:
    kafka::Topic topic_;
    std::string key_;
    std::string value_;
    kafka::Partition partition_{RD_KAFKA_PARTITION_UA};
    int64_t timestamp_{0};
    std::vector<MessageHeader> message_headers_;
};

} 