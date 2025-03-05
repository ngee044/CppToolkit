#include "KafkaMessage.h"

namespace Kafka
{
	KafkaMessage::KafkaMessage(const std::string& topic, const std::string& key, const std::string& value) 
		: topic_(topic)
		, key_(key)
		, value_(value)
	{

	}

	KafkaMessage::~KafkaMessage()
	{
	}

	auto KafkaMessage::topic(const std::string& topic) -> void
	{
		topic_ = topic;
	}

	auto KafkaMessage::key(const std::string& key) -> void
	{
		key_ = key;
	}

	auto KafkaMessage::value(const std::string& value) -> void
	{
		value_ = value;
	}

	auto KafkaMessage::partition(int partition) -> void
	{
		partition_ = partition;
	}

	auto KafkaMessage::timestamp(int64_t timestamp) -> void
	{
		timestamp_ = timestamp;
	}

	auto KafkaMessage::add_header(const std::string& key, const std::string& value) -> void
	{
		message_headers_.push_back({key, value});
	}

	auto KafkaMessage::get_headers() const -> const std::vector<MessageHeader>&
	{
		return message_headers_;
	}

	auto KafkaMessage::topic() const -> const kafka::Topic&
	{
		return topic_;
	}

	auto KafkaMessage::key() const -> const std::string&
	{
		return key_;
	}

	auto KafkaMessage::value() const -> const std::string&
	{
		return value_;
	}

	auto KafkaMessage::partition() const -> kafka::Partition
	{
		return partition_;
	}

	auto KafkaMessage::timestamp() const -> int64_t
	{
		return timestamp_;
	}




}