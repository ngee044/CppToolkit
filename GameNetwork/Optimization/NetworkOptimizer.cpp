#include "NetworkOptimizer.h"
namespace GameNetwork { 
	NetworkOptimizer::NetworkOptimizer(std::shared_ptr<NetworkMetrics> metrics)
		: network_metrics_(metrics) {}
}
