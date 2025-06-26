#include "HighAvailability.h"
#include "HealthCheck.h"
#include "LoadBalancer.h"
#include "CircuitBreaker.h"
#include <Logger.h>
#include <algorithm>
#include <random>

namespace GameNetwork
{
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
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "HighAvailability configured with strategy: " + 
            std::to_string(static_cast<int>(config.strategy)));
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
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Registered node: " + node.server_id + " with role: " + 
            std::to_string(static_cast<int>(node.role)));
        
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
            
            // If primary became unhealthy, initiate failover
            if (server_id == current_primary_id_ && !is_healthy && config_.automatic_failover)
            {
                Utilities::Logger::handle().write(Utilities::LogTypes::Warning,
                    "Primary node " + server_id + " became unhealthy, initiating failover");
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
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Promoted secondary " + server_id + " to primary");
        
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
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Demoted primary " + server_id + " to secondary");
        
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
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Started HighAvailability monitoring");
    }

    auto HighAvailability::stop_monitoring() -> void
    {
        monitoring_active_ = false;
        
        if (monitor_thread_.valid())
        {
            monitor_thread_.wait();
        }
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Stopped HighAvailability monitoring");
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
                
                Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                    "Node " + server_id + " recovered");
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
        
        // TODO: Implement actual state synchronization
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Syncing state to " + std::to_string(secondary_ids.size()) + " secondary nodes");
        
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
        
        // TODO: Implement actual state request
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Requesting state from primary: " + current_primary_id_);
        
        return { true, std::nullopt };
    }

    auto HighAvailability::elect_new_primary() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // For consensus-based election (e.g., Raft)
        // This is a simplified version
        
        auto candidate_opt = select_new_primary();
        if (!candidate_opt.has_value())
        {
            return { false, "No suitable candidate for election" };
        }
        
        // Count votes (simplified - in reality would involve network communication)
        uint32_t votes_needed = (stats_.healthy_nodes / 2) + 1; // Majority
        uint32_t votes_received = 1; // Self vote
        
        // Simulate getting votes from other nodes
        for (const auto& [id, node] : nodes_)
        {
            if (id != candidate_opt.value() && node.is_healthy)
            {
                // In reality, would send vote request to node
                // For now, assume they vote for highest priority candidate
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
        
        // In a real implementation, this would participate in consensus protocol
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Voting for primary candidate: " + candidate_id);
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
                
                // Check for failed nodes
                auto failed_nodes = detect_failed_nodes();
                
                for (const auto& node_id : failed_nodes)
                {
                    update_node_health(node_id, false);
                }
            }
            
            // Sleep for monitoring interval
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
        
        // Select node with highest priority
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
        
        // Demote old primary if it exists
        if (!old_primary.empty())
        {
            auto old_primary_it = nodes_.find(old_primary);
            if (old_primary_it != nodes_.end())
            {
                old_primary_it->second.role = ServerRole::Secondary;
            }
        }
        
        // Promote new primary
        new_primary_it->second.role = ServerRole::Primary;
        current_primary_id_ = new_primary_id;
        
        // Update statistics
        stats_.failover_count++;
        stats_.last_failover_time = std::chrono::steady_clock::now();
        
        // Notify callback
        if (failover_callback_)
        {
            failover_callback_(old_primary, new_primary_id);
        }
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Failover completed: " + old_primary + " -> " + new_primary_id);
        
        return { true, std::nullopt };
    }
}
