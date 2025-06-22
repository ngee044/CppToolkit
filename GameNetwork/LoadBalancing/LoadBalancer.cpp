#include "LoadBalancer.h"
#include <GameSession.h>
#include <Logger.h>
#include <algorithm>
#include <random>
#include <fmt/format.h>

using namespace Utilities;
using namespace fmt;

namespace GameNetwork
{
    LoadBalancer::LoadBalancer(LoadBalancingStrategy strategy)
        : strategy_(strategy)
        , round_robin_index_(0)
    {
        Logger::handle().write(LogTypes::Information,
            std::string(format("LoadBalancer initialized with strategy: {}", 
                static_cast<int>(strategy))));
    }
    
    auto LoadBalancer::register_server(const ServerMetrics& server) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (server.server_id.empty())
        {
            return {false, "Server ID cannot be empty"};
        }
        
        servers_[server.server_id] = server;
        channels_[server.server_id] = std::vector<ChannelMetrics>();
        
        Logger::handle().write(LogTypes::Information,
            std::string(format("Registered server: {} at {}:{}", 
                server.server_id, server.server_address, server.port)));
        
        return {true, std::nullopt};
    }
    
    auto LoadBalancer::unregister_server(const std::string& server_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = servers_.find(server_id);
        if (it == servers_.end())
        {
            return {false, "Server not found"};
        }
        
        // Remove all session assignments for this server
        for (auto it = session_assignments_.begin(); it != session_assignments_.end();)
        {
            if (it->second == server_id)
            {
                it = session_assignments_.erase(it);
            }
            else
            {
                ++it;
            }
        }
        
        servers_.erase(server_id);
        channels_.erase(server_id);
        
        Logger::handle().write(LogTypes::Information,
            std::string(format("Unregistered server: {}", server_id)));
        
        return {true, std::nullopt};
    }    
    auto LoadBalancer::update_server_metrics(const ServerMetrics& metrics) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = servers_.find(metrics.server_id);
        if (it == servers_.end())
        {
            return {false, "Server not found"};
        }
        
        it->second = metrics;
        it->second.last_update = std::chrono::steady_clock::now();
        
        // Check if server became unhealthy
        if (metrics.cpu_usage > HEALTHY_CPU_THRESHOLD || 
            metrics.memory_usage > HEALTHY_MEMORY_THRESHOLD)
        {
            it->second.is_accepting_players = false;
            Logger::handle().write(LogTypes::Error,
                std::string(format("Server {} marked as not accepting players due to high load", 
                    metrics.server_id)));
        }
        
        return {true, std::nullopt};
    }
    
    auto LoadBalancer::register_channel(const std::string& server_id, const ChannelMetrics& channel) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);        
        auto it = channels_.find(server_id);
        if (it == channels_.end())
        {
            return {false, "Server not found"};
        }
        
        // Check if channel already exists
        auto& channels = it->second;
        auto channel_it = std::find_if(channels.begin(), channels.end(),
            [&channel](const ChannelMetrics& ch) {
                return ch.channel_id == channel.channel_id;
            });
        
        if (channel_it != channels.end())
        {
            *channel_it = channel;
        }
        else
        {
            channels.push_back(channel);
        }
        
        return {true, std::nullopt};
    }
    
    auto LoadBalancer::get_best_server() const -> std::optional<ServerMetrics>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (servers_.empty())
        {
            return std::nullopt;
        }        
        switch (strategy_)
        {
            case LoadBalancingStrategy::RoundRobin:
            {
                // Get healthy servers
                std::vector<std::string> healthy_servers;
                for (const auto& [id, server] : servers_)
                {
                    if (server.is_healthy && server.is_accepting_players)
                    {
                        healthy_servers.push_back(id);
                    }
                }
                
                if (healthy_servers.empty())
                {
                    return std::nullopt;
                }
                
                size_t index = round_robin_index_++ % healthy_servers.size();
                return servers_.at(healthy_servers[index]);
            }
            
            case LoadBalancingStrategy::LeastConnections:
            {
                const ServerMetrics* best = nullptr;
                uint32_t min_connections = std::numeric_limits<uint32_t>::max();
                
                for (const auto& [id, server] : servers_)
                {
                    if (server.is_healthy && server.is_accepting_players &&
                        server.current_players < min_connections)                    {
                        min_connections = server.current_players;
                        best = &server;
                    }
                }
                
                return best ? std::optional<ServerMetrics>(*best) : std::nullopt;
            }
            
            case LoadBalancingStrategy::LeastLoad:
            default:
            {
                const ServerMetrics* best = nullptr;
                float best_score = std::numeric_limits<float>::max();
                
                for (const auto& [id, server] : servers_)
                {
                    if (server.is_healthy && server.is_accepting_players)
                    {
                        float score = calculate_server_score(server);
                        if (score < best_score)
                        {
                            best_score = score;
                            best = &server;
                        }
                    }
                }
                
                return best ? std::optional<ServerMetrics>(*best) : std::nullopt;
            }
        }
    }    
    auto LoadBalancer::get_best_channel(const std::string& server_id) const 
        -> std::optional<std::pair<std::string, ChannelMetrics>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (server_id.empty())
        {
            // Find best channel across all servers
            std::string best_server;
            ChannelMetrics best_channel;
            float best_score = std::numeric_limits<float>::max();
            
            for (const auto& [sid, channels] : channels_)
            {
                for (const auto& channel : channels)
                {
                    if (channel.is_available)
                    {
                        float score = calculate_channel_score(channel);
                        if (score < best_score)
                        {
                            best_score = score;
                            best_server = sid;
                            best_channel = channel;
                        }
                    }
                }
            }
            
            if (!best_server.empty())
            {
                return std::make_pair(best_server, best_channel);
            }        }
        else
        {
            // Find best channel in specific server
            auto it = channels_.find(server_id);
            if (it != channels_.end())
            {
                const ChannelMetrics* best = nullptr;
                float best_score = std::numeric_limits<float>::max();
                
                for (const auto& channel : it->second)
                {
                    if (channel.is_available)
                    {
                        float score = calculate_channel_score(channel);
                        if (score < best_score)
                        {
                            best_score = score;
                            best = &channel;
                        }
                    }
                }
                
                if (best)
                {
                    return std::make_pair(server_id, *best);
                }
            }
        }
        
        return std::nullopt;
    }    
    auto LoadBalancer::distribute_new_player(const std::string& preferred_server) 
        -> std::tuple<std::string, uint32_t>
    {
        // Try preferred server first if specified
        if (!preferred_server.empty())
        {
            auto channel_result = get_best_channel(preferred_server);
            if (channel_result)
            {
                total_assignments_++;
                return {channel_result->first, channel_result->second.channel_id};
            }
        }
        
        // Otherwise find best server and channel
        auto server_result = get_best_server();
        if (!server_result)
        {
            Logger::handle().write(LogTypes::Error,
                "No available servers for new player");
            return {"", 0};
        }
        
        auto channel_result = get_best_channel(server_result->server_id);
        if (!channel_result)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("No available channels on server {}", server_result->server_id));
            return {"", 0};
        }
        
        total_assignments_++;
        return {channel_result->first, channel_result->second.channel_id};
    }    
    auto LoadBalancer::initiate_session_migration(uint64_t session_id, 
                                                 const std::string& target_server,
                                                 uint32_t target_channel) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if migration already in progress
        if (active_migrations_.find(session_id) != active_migrations_.end())
        {
            return {false, "Migration already in progress for this session"};
        }
        
        // Get current assignment
        auto it = session_assignments_.find(session_id);
        if (it == session_assignments_.end())
        {
            return {false, "Session not found"};
        }
        
        // Validate target server
        if (servers_.find(target_server) == servers_.end())
        {
            return {false, "Target server not found"};
        }
        
        // Create migration info
        SessionMigrationInfo migration;
        migration.session_id = session_id;
        migration.source_server = it->second;
        migration.target_server = target_server;
        migration.source_channel = 0; // TODO: Get from session
        migration.target_channel = target_channel;        migration.start_time = std::chrono::steady_clock::now();
        migration.is_completed = false;
        
        active_migrations_[session_id] = migration;
        total_migrations_++;
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("Initiated migration for session {} from {} to {}", 
                session_id, it->second, target_server));
        
        return {true, std::nullopt};
    }
    
    auto LoadBalancer::complete_session_migration(uint64_t session_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_migrations_.find(session_id);
        if (it == active_migrations_.end())
        {
            return {false, "No active migration found for session"};
        }
        
        // Update session assignment
        session_assignments_[session_id] = it->second.target_server;
        
        // Mark migration as completed
        it->second.is_completed = true;
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("Completed migration for session {}", session_id));        // Remove from active migrations
        active_migrations_.erase(it);
        
        return {true, std::nullopt};
    }
    
    auto LoadBalancer::calculate_server_score(const ServerMetrics& server) const -> float
    {
        // Calculate composite score based on multiple factors
        float player_ratio = static_cast<float>(server.current_players) / 
                           static_cast<float>(server.max_players);
        
        float score = player_ratio * 0.4f +
                     (server.cpu_usage / 100.0f) * 0.3f +
                     (server.memory_usage / 100.0f) * 0.2f +
                     (server.latency_ms / 1000.0f) * 0.1f;
        
        // Penalize unhealthy servers
        if (!server.is_healthy)
        {
            score += 10.0f;
        }
        
        return score;
    }
    
    auto LoadBalancer::calculate_channel_score(const ChannelMetrics& channel) const -> float
    {
        float player_ratio = static_cast<float>(channel.current_players) / 
                           static_cast<float>(channel.max_players);
        
        float score = player_ratio * 0.7f + channel.load_factor * 0.3f;        
        // Boost recommended channels
        if (channel.is_recommended)
        {
            score *= 0.8f;
        }
        
        return score;
    }
    
    auto LoadBalancer::check_server_health() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        
        for (auto& [id, server] : servers_)
        {
            auto time_since_update = std::chrono::duration_cast<std::chrono::seconds>(
                now - server.last_update);
            
            if (time_since_update > HEALTH_CHECK_TIMEOUT)
            {
                server.is_healthy = false;
                Logger::handle().write(LogTypes::Error,
                    fmt::format("Server {} marked unhealthy due to timeout", id));
            }
        }
        
        // Clean up completed migrations
        cleanup_completed_migrations();
    }    
    auto LoadBalancer::get_total_capacity() const -> uint32_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint32_t total = 0;
        for (const auto& [id, server] : servers_)
        {
            if (server.is_healthy)
            {
                total += server.max_players;
            }
        }
        
        return total;
    }
    
    auto LoadBalancer::get_total_players() const -> uint32_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint32_t total = 0;
        for (const auto& [id, server] : servers_)
        {
            total += server.current_players;
        }
        
        return total;
    }
    
    auto LoadBalancer::get_average_load() const -> float
    {
        auto capacity = get_total_capacity();
        if (capacity == 0)        {
            return 0.0f;
        }
        
        return static_cast<float>(get_total_players()) / static_cast<float>(capacity);
    }
    
    auto LoadBalancer::cleanup_completed_migrations() -> void
    {
        // Already locked by caller
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = active_migrations_.begin(); it != active_migrations_.end();)
        {
            if (it->second.is_completed)
            {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->second.start_time);
                
                if (duration > std::chrono::seconds(60))
                {
                    it = active_migrations_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            else
            {
                // Check for stale migrations
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->second.start_time);
                
                if (duration > std::chrono::minutes(5))
                {
                    Logger::handle().write(LogTypes::Error,
                        fmt::format("Removing stale migration for session {}", 
                            it->second.session_id));
                    failed_migrations_++;
                    it = active_migrations_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    auto LoadBalancer::get_load_distribution() const -> std::unordered_map<std::string, float>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<std::string, float> distribution;
        
        for (const auto& [server_id, server] : servers_)
        {
            if (server.max_players > 0)
            {
                float load_ratio = static_cast<float>(server.current_players) / 
                                 static_cast<float>(server.max_players);
                distribution[server_id] = load_ratio;
            }
            else
            {
                distribution[server_id] = 0.0f;
            }
        }
        
        return distribution;
    }

    auto LoadBalancer::rebalance_load() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (servers_.empty())
        {
            return {false, "No servers available for rebalancing"};
        }
        
        // Find the most loaded and least loaded servers
        std::string most_loaded_server;
        std::string least_loaded_server;
        float max_load = 0.0f;
        float min_load = 1.0f;
        
        for (const auto& [server_id, server] : servers_)
        {
            if (!server.is_healthy || !server.is_accepting_players)
            {
                continue;
            }
            
            if (server.max_players > 0)
            {
                float load_ratio = static_cast<float>(server.current_players) / 
                                 static_cast<float>(server.max_players);
                
                if (load_ratio > max_load)
                {
                    max_load = load_ratio;
                    most_loaded_server = server_id;
                }
                
                if (load_ratio < min_load)
                {
                    min_load = load_ratio;
                    least_loaded_server = server_id;
                }
            }
        }
        
        // Check if rebalancing is needed
        const float REBALANCE_THRESHOLD = 0.3f; // 30% difference
        if ((max_load - min_load) < REBALANCE_THRESHOLD)
        {
            return {true, "Load is already balanced"};
        }
        
        if (most_loaded_server.empty() || least_loaded_server.empty())
        {
            return {false, "Could not find suitable servers for rebalancing"};
        }
        
        // For now, just log the rebalancing decision
        // In a real implementation, this would trigger session migrations
        Logger::handle().write(LogTypes::Information,
            std::string(format("Load rebalancing needed: {} ({}%) -> {} ({}%)",
                most_loaded_server, static_cast<int>(max_load * 100),
                least_loaded_server, static_cast<int>(min_load * 100))));
        
        return {true, std::nullopt};
    }
}