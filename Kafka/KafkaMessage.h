#pragma once

#include "KafkaCommon.h"

namespace Kafka {

class KafkaMessage {
public:
    KafkaMessage();
    KafkaMessage(const std::string& topic, const std::string& key, const std::string& value);
    ~KafkaMessage();
    
    auto set_topic(const std::string& topic) -> void;
    auto set_key(const std::string& key) -> void;
    auto set_value(const std::string& value) -> void;
    auto set_partition(int partition) -> void;
    auto set_timestamp(int64_t timestamp) -> void;
    
    auto add_header(const std::string& key, const std::string& value) -> void;
    
	auto get_topic() const -> std::string;
	auto get_key() const -> std::string;
	auto get_value() const -> std::string;
	auto get_partition() const -> int;
	auto get_timestamp() const -> int64_t;
	auto get_headers() const -> std::vector<MessageHeader>;

	
private:
    std::string topic_;
    std::string key_;
    std::string value_;
    int partition_;
    int64_t timestamp_;
    std::vector<MessageHeader> message_headers_;
};

} 