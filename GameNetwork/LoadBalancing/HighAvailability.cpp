#include "HighAvailability.h"
#include "HealthCheck.h"
#include "LoadBalancer.h"
#include "CircuitBreaker.h"
#include "ServerRegistry.h"
#include "GameSessionManager.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>
#include <random>
#include <boost/json.hpp>
#include <boost/system/error_code.hpp>

using namespace Utilities;

namespace GameNetwork
{
	std::string role_to_string(NodeRole role)
	{
		switch (role)
		{
		case NodeRole::Primary:
			return "Primary";
		case NodeRole::Secondary:
			return "Secondary";
		case NodeRole::Standby:
			return "Standby";
		default:
			return "Unknown";
		}
	}

	HighAvailability::HighAvailability()
		: monitoring_active_(false)
		, stats_{0, 0, std::chrono::steady_clock::now(), 100.0f, 0, 0}
	{
		configure(Config{});
	}

	HighAvailability::~HighAvailability()
	{
		stop_monitoring();
	}

	auto HighAvailability::configure(const Config& config) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		config_ = config;

		Logger::handle().write(LogTypes::Information,
			fmt::format("HighAvailability configured with strategy: {}", static_cast<int>(config.strategy)));
	}

	auto HighAvailability::register_node(const ServerNode& node) 
		-> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		// Check if node already exists
		if (nodes_.find(node.server_id) != nodes_.end())
		{
			return { false, "Node already registered" };
		}
        
		nodes_[node.server_id] = node;
		stats_.total_nodes++;
        
		if (node.is_healthy)
		{
			stats_.healthy_nodes++;
		}
        
		// If this is the first node and no primary exists, make it primary
		if (nodes_.size() == 1 && current_primary_id_.empty())
		{
			current_primary_id_ = node.server_id;
			nodes_[node.server_id].role = ServerRole::Primary;
		}
        
		Logger::handle().write(LogTypes::Information,
			fmt::format("Registered node: {} with role: {}", node.server_id, static_cast<int>(node.role)));
        
		return { true, std::nullopt };
	}

	auto HighAvailability::unregister_node(const std::string& server_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = nodes_.find(server_id);
		if (it != nodes_.end())
		{
			if (it->second.is_healthy)
			{
				stats_.healthy_nodes--;
			}
			stats_.total_nodes--;
			// If unregistering the primary, initiate failover
			if (server_id == current_primary_id_)
			{
				nodes_.erase(it);
				initiate_failover();
			}
			else
			{
				nodes_.erase(it);
			}
		}
	}

	auto HighAvailability::update_node_health(const std::string& server_id, bool is_healthy) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = nodes_.find(server_id);
		if (it != nodes_.end())
		{
			bool was_healthy = it->second.is_healthy;
			it->second.is_healthy = is_healthy;
            
			// Update healthy node count
			if (was_healthy && !is_healthy)
			{
				stats_.healthy_nodes--;
			}
			else if (!was_healthy && is_healthy)
			{
				stats_.healthy_nodes++;
			}
            
			if (server_id == current_primary_id_ && !is_healthy && config_.automatic_failover)
			{
				Logger::handle().write(LogTypes::Error, 
					fmt::format("Primary node {} became unhealthy, initiating failover", server_id));
				initiate_failover();
			}
		}
	}

	auto HighAvailability::initiate_failover() -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		// Check if we have minimum healthy nodes
		if (stats_.healthy_nodes < config_.min_healthy_nodes)
		{
			return { false, "Not enough healthy nodes for failover" };
		}
        
		// Select new primary
		auto new_primary_opt = select_new_primary();
		if (!new_primary_opt.has_value())
		{
			return { false, "No suitable node for failover" };
		}
        
		std::string old_primary = current_primary_id_;
		return perform_failover(new_primary_opt.value());
	}

	auto HighAvailability::promote_secondary(const std::string& server_id) 
		-> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = nodes_.find(server_id);
		if (it == nodes_.end())
		{
			return { false, "Node not found" };
		}
        
		if (it->second.role != ServerRole::Secondary)
		{
			return { false, "Node is not a secondary" };
		}
        
		if (!it->second.is_healthy)
		{
			return { false, "Node is not healthy" };
		}
        
		// Demote current primary if exists
		if (!current_primary_id_.empty())
		{
			auto primary_it = nodes_.find(current_primary_id_);
			if (primary_it != nodes_.end())
			{
				primary_it->second.role = ServerRole::Secondary;
			}
		}
        
		// Promote the secondary
		it->second.role = ServerRole::Primary;
		std::string old_primary = current_primary_id_;
		current_primary_id_ = server_id;
        
		// Notify callback
		if (failover_callback_)
		{
			failover_callback_(old_primary, server_id);
		}
        
		stats_.failover_count++;
		stats_.last_failover_time = std::chrono::steady_clock::now();

		Logger::handle().write(LogTypes::Information,
			fmt::format("Promoted secondary {} to primary", server_id));

		return { true, std::nullopt };
	}

	auto HighAvailability::demote_primary(const std::string& server_id)
		-> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (server_id != current_primary_id_)
		{
			return { false, "Node is not the current primary" };
		}
        
		auto it = nodes_.find(server_id);
		if (it == nodes_.end())
		{
			return { false, "Node not found" };
		}
        
		// Demote to secondary
		it->second.role = ServerRole::Secondary;
		current_primary_id_.clear();

		Logger::handle().write(LogTypes::Information,
			fmt::format("Demoted primary {} to secondary", server_id));

		return { true, std::nullopt };
	}

	auto HighAvailability::start_monitoring() -> void
	{
		if (monitoring_active_.exchange(true))
		{
			return; // Already monitoring
		}
        
		monitor_thread_ = std::async(std::launch::async, [this]() {
			monitor_nodes();
		});
        
		Logger::handle().write(LogTypes::Information, "Started HighAvailability monitoring");
	}

	auto HighAvailability::stop_monitoring() -> void
	{
		monitoring_active_ = false;
        
		if (monitor_thread_.valid())
		{
			monitor_thread_.wait();
		}
        
		Logger::handle().write(LogTypes::Information, "Stopped HighAvailability monitoring");
	}

	auto HighAvailability::process_heartbeat(const std::string& server_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = nodes_.find(server_id);
		if (it != nodes_.end())
		{
			it->second.last_heartbeat = std::chrono::steady_clock::now();
            
			// If node was unhealthy, mark as healthy
			if (!it->second.is_healthy)
			{
				it->second.is_healthy = true;
				stats_.healthy_nodes++;

				Logger::handle().write(LogTypes::Information, fmt::format("Node {} recovered", server_id));
			}
		}
	}

	auto HighAvailability::check_cluster_health() -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		// Update cluster availability
		if (stats_.total_nodes > 0)
		{
			stats_.cluster_availability_percentage = 
				(static_cast<float>(stats_.healthy_nodes) / stats_.total_nodes) * 100.0f;
		}
		else
		{
			stats_.cluster_availability_percentage = 0.0f;
		}
        
		// Check if we have minimum healthy nodes
		if (stats_.healthy_nodes < config_.min_healthy_nodes)
		{
			return { false, "Cluster unhealthy: not enough healthy nodes" };
		}
        
		// Check if primary is healthy
		if (!current_primary_id_.empty())
		{
			auto primary_it = nodes_.find(current_primary_id_);
			if (primary_it == nodes_.end() || !primary_it->second.is_healthy)
			{
				return { false, "Cluster unhealthy: primary node is not healthy" };
			}
		}
        
		return { true, std::nullopt };
	}

	auto HighAvailability::sync_state_to_secondaries() -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (current_primary_id_.empty())
		{
			return { false, "No primary node" };
		}
        
		// Get all secondary nodes
		std::vector<std::string> secondary_ids;
		for (const auto& [id, node] : nodes_)
		{
			if (node.role == ServerRole::Secondary && node.is_healthy)
			{
				secondary_ids.push_back(id);
			}
		}
        
		if (secondary_ids.empty())
		{
			return { false, "No healthy secondary nodes" };
		}
        
		// Implement actual state synchronization
		auto& server_registry = ServerRegistry::get_instance();
        
		// Collect current state
		boost::json::object state;
		state["node_id"] = node_id_;
		state["role"] = role_to_string(static_cast<NodeRole>(current_role_));
		state["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
        
		// Add session manager state if primary
		if (static_cast<int>(current_role_) == static_cast<int>(ServerRole::Primary))
		{
			auto session_manager = GameSessionManager::get_instance();
			if (session_manager)
			{
				auto sessions = session_manager->get_all_sessions();
				boost::json::array session_array;
                
				for (const auto& [session_id, session] : sessions)
				{
					boost::json::object session_obj;
					session_obj["session_id"] = session_id;
					session_obj["account_id"] = session->account_id();
					session_obj["state"] = static_cast<int>(session->state());
					session_array.push_back(session_obj);
				}
                
				state["sessions"] = session_array;
				state["session_count"] = sessions.size();
			}
		}
        
		// Send state to all secondary nodes
		size_t sync_count = 0;
		for (const auto& secondary_id : secondary_ids)
		{
			boost::json::object sync_msg;
			sync_msg["command"] = "sync_state";
			sync_msg["from_node"] = node_id_;
			sync_msg["state"] = state;
            
			auto result = ServerRegistry::get_instance().send_message(secondary_id, boost::json::serialize(sync_msg));
			bool success = std::get<0>(result);
			std::string error = std::get<1>(result);
			if (success)
			{
				sync_count++;
			}
			else
			{
				Logger::handle().write(LogTypes::Error,
					"Failed to sync to " + secondary_id + ": " + error);
			}
		}
        
		stats_.state_syncs++;
        
		Logger::handle().write(LogTypes::Information, fmt::format(
			"Synced state to {}/{} secondary nodes",
			sync_count,
			secondary_ids.size()
		));
        
		return { true, std::nullopt };
	}

	auto HighAvailability::request_state_from_primary() -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (current_primary_id_.empty())
		{
			return { false, "No primary node" };
		}
        
		auto primary_it = nodes_.find(current_primary_id_);
		if (primary_it == nodes_.end() || !primary_it->second.is_healthy)
		{
			return { false, "Primary node is not healthy" };
		}
        
		auto& server_registry = ServerRegistry::get_instance();
        
		boost::json::object request_msg;
		request_msg["command"] = "request_state";
		request_msg["from_node"] = node_id_;
		request_msg["reason"] = "failover_preparation";
        
		auto [success, response] = server_registry.send_and_wait(
			current_primary_id_,
			boost::json::serialize(request_msg),
			30000
		);
        
		if (!success)
		{
			return std::make_tuple(false, "Failed to request state: " + response);
		}
        
		try
		{
			boost::system::error_code ec;
			auto response_json = boost::json::parse(response, ec);
            
			if (ec)
			{
				return { false, "Invalid state response" };
			}
            
			auto response_obj = response_json.as_object();
			if (response_obj.contains("state"))
			{
				// Store received state for potential failover
				last_synced_state_ = response_obj["state"].as_object();
				last_sync_time_ = std::chrono::steady_clock::now();
			}
            
			Logger::handle().write(LogTypes::Information,
				fmt::format("Received state from primary: {}", current_primary_id_));
            
			return { true, std::nullopt };
		}
		catch (const std::exception& e)
		{
			return { false, fmt::format("Failed to process state: {}", e.what()) };
		}
        
		Logger::handle().write(LogTypes::Information,
			fmt::format("Requesting state from primary: {}", current_primary_id_));

		return { true, std::nullopt };
	}

	auto HighAvailability::elect_new_primary() -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto candidate_opt = select_new_primary();
		if (!candidate_opt.has_value())
		{
			return { false, "No suitable candidate for election" };
		}
        
		uint32_t votes_needed = (stats_.healthy_nodes / 2) + 1; // Majority
		uint32_t votes_received = 1; // Self vote
        
		for (const auto& [id, node] : nodes_)
		{
			if (id != candidate_opt.value() && node.is_healthy)
			{
				votes_received++;
			}
		}
        
		if (votes_received >= votes_needed)
		{
			return perform_failover(candidate_opt.value());
		}
        
		return { false, "Failed to get majority votes" };
	}

	auto HighAvailability::vote_for_primary(const std::string& candidate_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		Logger::handle().write(LogTypes::Information, fmt::format("Voting for primary candidate: {}", candidate_id));
	}

	auto HighAvailability::set_failover_callback(FailoverCallback callback) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		failover_callback_ = callback;
	}

	auto HighAvailability::get_primary_node() const -> std::optional<ServerNode>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (current_primary_id_.empty())
		{
			return std::nullopt;
		}
        
		auto it = nodes_.find(current_primary_id_);
		if (it != nodes_.end())
		{
			return it->second;
		}
        
		return std::nullopt;
	}

	auto HighAvailability::get_secondary_nodes() const -> std::vector<ServerNode>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::vector<ServerNode> secondaries;
		for (const auto& [id, node] : nodes_)
		{
			if (node.role == ServerRole::Secondary)
			{
				secondaries.push_back(node);
			}
		}
        
		return secondaries;
	}

	auto HighAvailability::get_all_nodes() const -> std::vector<ServerNode>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::vector<ServerNode> all_nodes;
		for (const auto& [id, node] : nodes_)
		{
			all_nodes.push_back(node);
		}
        
		return all_nodes;
	}

	auto HighAvailability::is_primary() const -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return local_server_id_ == current_primary_id_;
	}

	auto HighAvailability::get_statistics() const -> HAStats
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return stats_;
	}

	auto HighAvailability::monitor_nodes() -> void
	{
		while (monitoring_active_)
		{
			{
				std::lock_guard<std::mutex> lock(mutex_);
                
				auto failed_nodes = detect_failed_nodes();
                
				for (const auto& node_id : failed_nodes)
				{
					update_node_health(node_id, false);
				}
			}
            
			std::this_thread::sleep_for(config_.heartbeat_interval);
		}
	}

	auto HighAvailability::detect_failed_nodes() -> std::vector<std::string>
	{
		std::vector<std::string> failed_nodes;
		auto now = std::chrono::steady_clock::now();
        
		for (const auto& [id, node] : nodes_)
		{
			if (node.is_healthy)
			{
				auto time_since_heartbeat = now - node.last_heartbeat;
				if (time_since_heartbeat > config_.failover_timeout)
				{
					failed_nodes.push_back(id);
				}
			}
		}
        
		return failed_nodes;
	}

	auto HighAvailability::select_new_primary() -> std::optional<std::string>
	{
		std::vector<std::pair<std::string, uint32_t>> candidates;
        
		for (const auto& [id, node] : nodes_)
		{
			if (node.is_healthy && node.role != ServerRole::Primary)
			{
				candidates.emplace_back(id, node.priority);
			}
		}
        
		if (candidates.empty())
		{
			return std::nullopt;
		}
        
		std::sort(candidates.begin(), candidates.end(),
			[](const auto& a, const auto& b) { return a.second > b.second; });
        
		return candidates.front().first;
	}

	auto HighAvailability::perform_failover(const std::string& new_primary_id) 
		-> std::tuple<bool, std::optional<std::string>>
	{
		auto new_primary_it = nodes_.find(new_primary_id);
		if (new_primary_it == nodes_.end())
		{
			return { false, "New primary node not found" };
		}
        
		if (!new_primary_it->second.is_healthy)
		{
			return { false, "New primary node is not healthy" };
		}
        
		std::string old_primary = current_primary_id_;
        
		if (!old_primary.empty())
		{
			auto old_primary_it = nodes_.find(old_primary);
			if (old_primary_it != nodes_.end())
			{
				old_primary_it->second.role = ServerRole::Secondary;
			}
		}
        
		new_primary_it->second.role = ServerRole::Primary;
		current_primary_id_ = new_primary_id;
        
		stats_.failover_count++;
		stats_.last_failover_time = std::chrono::steady_clock::now();
        
		if (failover_callback_)
		{
			failover_callback_(old_primary, new_primary_id);
		}

		Logger::handle().write(LogTypes::Information, fmt::format("Failover completed: {} -> {}", old_primary, new_primary_id));

		return { true, std::nullopt };
	}
}
