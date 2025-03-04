#include "KafkaConfig.h"

namespace Kafka
{
	KafkaConfig::KafkaConfig(const std::string& brokers)
		: brokers_(brokers)
		, partition_(3)
		, timeout_ms_(1000)
		, auto_commit_(true)
	{
		properties_.put("bootstrap.servers", brokers);
	}

	void KafkaConfig::add_config(const std::string& key, const std::string& value)
	{
		properties_.put(key, value);
	}

	void KafkaConfig::add_broker(const std::string& broker)
	{
		if (!brokers_.empty())
		{
			brokers_ += ",";
		}
		brokers_ += broker;
		properties_.put("bootstrap.servers", brokers_);
	}

	const kafka::Properties& KafkaConfig::get_properties() const
	{
		return properties_;
	}

	std::string KafkaConfig::get_brokers() const
	{
		return brokers_;
	}


}