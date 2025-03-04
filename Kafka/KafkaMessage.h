#pragma once

#include "KafkaCommon.h"

namespace Kafka {

class KafkaMessage {
public:
    KafkaMessage(const std::string& topic, const std::string& key, const std::string& value);
    ~KafkaMessage();
    
    auto topic(const std::string& topic) -> void;
	auto topic() const -> std::string;

	auto key(const std::string& key) -> void;
	auto key() const -> std::string;

    auto value(const std::string& value) -> void;
	auto value() const -> std::string;

    auto partition(int partition) -> void;
	auto partition() -> int;

    auto timestamp(int64_t timestamp) -> void;
	auto timestamp() -> int64_t;

    auto add_header(const std::string& key, const std::string& value) -> void;
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