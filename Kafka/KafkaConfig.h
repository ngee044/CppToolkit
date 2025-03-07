#pragma once

#include <string>
#include <map>
#include <kafka/Properties.h>

namespace Kafka 
{
	class KafkaConfig
	{
		public:
		KafkaConfig(const std::string& brokers);
		virtual ~KafkaConfig() = default;
	
		void add_config(const std::string& key, const std::string& value);
		void add_broker(const std::string& broker);
		
		const kafka::Properties& get_properties() const;
		std::string get_brokers() const;

		auto set_topic(const std::string& topic) -> void { topic_ = topic; }
		auto get_topic() const -> std::string { return topic_; }

		auto set_group_id(const std::string& group_id) -> void { group_id_ = group_id; }
		auto get_group_id() const -> std::string { return group_id_; }

		auto set_partition(int partition) -> void { partition_ = partition; }
		
		auto get_partition() const -> int { return partition_; }

		auto set_timeout_ms(int timeout_ms) -> void { timeout_ms_ = timeout_ms; }
		auto get_timeout_ms() const -> int { return timeout_ms_; }

		auto set_auto_commit(bool auto_commit) -> void { auto_commit_ = auto_commit; }
		auto get_auto_commit() const -> bool { return auto_commit_; }

	
	private:
		std::string brokers_;
		std::string topic_;
		std::string group_id_;

		kafka::Properties properties_;

		int partition_;
		int timeout_ms_;
		bool auto_commit_;
	};

}