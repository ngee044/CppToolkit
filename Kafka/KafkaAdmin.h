#pragma once

#include "KafkaCommon.h"
#include "KafkaConfig.h"
#include <map>

namespace Kafka {

// 토픽 구성
struct TopicConfig {
    std::string name;
    int numPartitions;
    int replicationFactor;
    std::map<std::string, std::string> configs;
};

class KafkaAdmin {
public:
    KafkaAdmin(const KafkaConfig& config);
    ~KafkaAdmin();
    
    auto create_topic(const TopicConfig& topicConfig) -> KafkaError;
    auto delete_topic(const std::string& topic) -> KafkaError;
    auto topic_exists(const std::string& topic) -> bool;
    auto list_topics() -> std::vector<std::string>;
    
    auto get_topic_config(const std::string& topic) -> TopicConfig;
    auto get_partition_count(const std::string& topic) -> int;
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}