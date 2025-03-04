#pragma once

#include <string>
#include <map>
#include <kafka/Properties.h>

namespace Kafka 
{
	class KafkaConfig
	{
	public:
		KafkaConfig();
		~KafkaConfig();

		// basic config
		auto set_bootstrap_servers(const std::string& servers) -> void;
		std::string get_bootstrap_servers() const;

		auto set_topic(const std::string& topic) -> void;
		std::string get_topic() const;

		auto set_group_id(const std::string& group_id) -> void;
		std::string get_group_id() const;

		// TLS
		auto set_tls_enabled(bool enabled) -> void;
		auto get_tls_enabled() const -> bool;

		auto set_ca_location(const std::string& ca_path) -> void;
		std::string get_ca_location() const;

		auto set_certificate_location(const std::string& cert_path) -> void;
		std::string get_certificate_location() const;

		auto set_key_location(const std::string& key_path) -> void;
		std::string get_key_location() const;

		// Message timeout
		auto set_message_timeout_ms(int timeout_ms) -> void;
		int get_message_timeout_ms() const;

		// Properties
		auto set_property(const std::string& key, const std::string& value) -> void;
		std::string get_property(const std::string& key) const;

		kafka::Properties to_kafka_properties() const;

	private:
		std::string bootstrap_servers_;
		std::string topic_;
		std::string group_id_;

		bool tls_enabled_;
		std::string ca_location_;
		std::string certificate_location_;
		std::string key_location_;

		int message_timeout_ms_;

		std::map<std::string, std::string> properties_;
	};

}