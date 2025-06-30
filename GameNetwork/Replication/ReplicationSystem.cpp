#include "ReplicationSystem.h"
namespace GameNetwork { 
	ReplicationSystem::ReplicationSystem(std::shared_ptr<EntityReplicator> entity_replicator,
										std::shared_ptr<NetworkMetrics> network_metrics)
		: entity_replicator_(entity_replicator)
		, network_metrics_(network_metrics) {}
	ReplicationSystem::~ReplicationSystem() = default;
}
