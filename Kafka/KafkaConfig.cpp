#include "KafkaConfig.h"

namespace Kafka
{

	KafkaConfig::KafkaConfig()
	: tls_enabled_(false)
	, message_timeout_ms_(300000) // default 300s
	{
	}

	KafkaConfig::~KafkaConfig()
	{
	}

	#pragma region getter_setter
	void KafkaConfig::set_bootstrap_servers(const std::string& servers)
	{
		bootstrap_servers_ = servers;
	}

	std::string KafkaConfig::get_bootstrap_servers() const
	{
		return bootstrap_servers_;
	}

	auto KafkaConfig::set_topic(const std::string& topic) -> void {	topic_ = topic; }

	auto KafkaConfig::get_topic() const -> std::string { return topic_; }

	auto KafkaConfig::set_group_id(const std::string& group_id) -> void { group_id_ = group_id; }

	auto KafkaConfig::get_group_id() const -> std::string { return group_id_; }

	auto KafkaConfig::set_tls_enabled(bool enabled) -> void { tls_enabled_ = enabled; }

	auto KafkaConfig::get_tls_enabled() const -> bool { return tls_enabled_; }

	auto KafkaConfig::set_ca_location(const std::string& ca_path) -> void { ca_location_ = ca_path; }

	auto KafkaConfig::get_ca_location() const -> std::string { return ca_location_; }

	auto KafkaConfig::set_certificate_location(const std::string& cert_path) -> void { certificate_location_ = cert_path; }

	auto KafkaConfig::get_certificate_location() const -> std::string { return certificate_location_; }

	auto KafkaConfig::set_key_location(const std::string& key_path) -> void { key_location_ = key_path; }

	auto KafkaConfig::get_key_location() const -> std::string { return key_location_; }

	auto KafkaConfig::set_message_timeout_ms(int timeout_ms) -> void { message_timeout_ms_ = timeout_ms; }

	auto KafkaConfig::get_message_timeout_ms() const -> int { return message_timeout_ms_; }

	auto KafkaConfig::set_property(const std::string& key, const std::string& value) -> void { properties_[key] = value; }

	std::string KafkaConfig::get_property(const std::string& key) const
	{
		auto it = properties_.find(key);
		if (it != properties_.end())
		{
			return it->second;
		}
		return "";
	}
	#pragma endregion

	kafka::Properties KafkaConfig::to_kafka_properties() const
	{
		kafka::Properties properties;
		properties.put("bootstrap.servers", bootstrap_servers_);
		
		if (!group_id_.empty())
		{
			properties.put("group.id", group_id_);
		}

		properties.put("message.timeout.ms", std::to_string(message_timeout_ms_));

		if (tls_enabled_)
		{
			properties.put("security.protocol", "ssl");
			properties.put("ssl.ca.location", ca_location_);
			properties.put("ssl.certificate.location", certificate_location_);
			properties.put("ssl.key.location", key_location_);
		}

		for (const auto& [key, value] : properties_)
		{
			properties.put(key, value);
		}

		return properties;
	}

}