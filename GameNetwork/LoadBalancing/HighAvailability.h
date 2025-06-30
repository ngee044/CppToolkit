#pragma once

#include <GameNetworkConstants.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <functional>
#include <mutex>
#include <tuple>
#include <optional>
#include <atomic>
#include <thread>
#include <future>
#include <boost/json.hpp>

namespace GameNetwork
{
	// Forward declarations
	class HealthCheck;
	class LoadBalancer;
	class CircuitBreaker;
    
	enum class ServerRole
	{
		Primary,
		Secondary,
		Standby
	};
    
	enum class NodeRole
	{
		Primary,
		Secondary,
		Standby
	};
    
	std::string role_to_string(NodeRole role);
    
	enum class FailoverStrategy
	{
		ActivePassive,    // One primary, multiple standbys
		ActiveActive,     // Multiple active servers
		MasterSlave,      // One master, multiple slaves
		PeerToPeer        // All servers are equal
	};
    
	struct ServerNode
	{
		std::string server_id;
		std::string address;
		uint16_t port;
		ServerRole role;
		bool is_healthy;
		std::chrono::steady_clock::time_point last_heartbeat;
		uint32_t priority;  // For failover ordering
	};
    
	class HighAvailability
	{
	public:
		HighAvailability();
		~HighAvailability();
        
		struct Config
		{
			FailoverStrategy strategy = FailoverStrategy::ActivePassive;
			std::chrono::milliseconds heartbeat_interval{1000};
			std::chrono::milliseconds failover_timeout{5000};
			uint32_t min_healthy_nodes = 1;
			bool automatic_failover = true;
			uint32_t quorum_size = 2; // For consensus
		};
        
		auto configure(const Config& config) -> void;
        
		auto register_node(const ServerNode& node) -> std::tuple<bool, std::optional<std::string>>;
		auto unregister_node(const std::string& server_id) -> void;
		auto update_node_health(const std::string& server_id, bool is_healthy) -> void;
        
		auto initiate_failover() -> std::tuple<bool, std::optional<std::string>>;
		auto promote_secondary(const std::string& server_id) -> std::tuple<bool, std::optional<std::string>>;
		auto demote_primary(const std::string& server_id) -> std::tuple<bool, std::optional<std::string>>;
        
		auto start_monitoring() -> void;
		auto stop_monitoring() -> void;
		auto process_heartbeat(const std::string& server_id) -> void;
		auto check_cluster_health() -> std::tuple<bool, std::optional<std::string>>;
        
		auto sync_state_to_secondaries() -> std::tuple<bool, std::optional<std::string>>;
		auto request_state_from_primary() -> std::tuple<bool, std::optional<std::string>>;
        
		auto elect_new_primary() -> std::tuple<bool, std::optional<std::string>>;
		auto vote_for_primary(const std::string& candidate_id) -> void;
        
		using FailoverCallback = std::function<void(const std::string& old_primary, const std::string& new_primary)>;
		auto set_failover_callback(FailoverCallback callback) -> void;
        
		auto get_primary_node() const -> std::optional<ServerNode>;
		auto get_secondary_nodes() const -> std::vector<ServerNode>;
		auto get_all_nodes() const -> std::vector<ServerNode>;
		auto is_primary() const -> bool;
        
		struct HAStats
		{
			uint64_t failover_count;
			uint64_t false_positive_failures;
			std::chrono::steady_clock::time_point last_failover_time;
			float cluster_availability_percentage;
			uint32_t total_nodes;
			uint32_t healthy_nodes;
			uint64_t state_syncs;
		};
        
		auto get_statistics() const -> HAStats;
        
	private:
		auto monitor_nodes() -> void;
		auto detect_failed_nodes() -> std::vector<std::string>;
		auto select_new_primary() -> std::optional<std::string>;
		auto perform_failover(const std::string& new_primary_id) -> std::tuple<bool, std::optional<std::string>>;
        
	private:
		mutable std::mutex mutex_;
        
		Config config_;
		std::string local_server_id_;
        
		std::unordered_map<std::string, ServerNode> nodes_;
		std::string current_primary_id_;
		std::string node_id_;
		ServerRole current_role_ = ServerRole::Secondary;
		boost::json::object last_synced_state_;
		std::chrono::steady_clock::time_point last_sync_time_;
        
		std::atomic<bool> monitoring_active_;
		std::future<void> monitor_thread_;
        
		std::weak_ptr<HealthCheck> health_check_;
		std::weak_ptr<LoadBalancer> load_balancer_;
		std::weak_ptr<CircuitBreaker> circuit_breaker_;
        
		FailoverCallback failover_callback_;
        
		HAStats stats_;
	};
}
