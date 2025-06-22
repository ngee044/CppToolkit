#pragma once

#include "GameNetworkConstants.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <atomic>
#include <mutex>
#include <optional>
#include <tuple>
#include <chrono>

namespace GameNetwork
{
    // Forward declarations
    class GameSession;
    class GameNetworkServer;
    
    // Server metrics for load balancing decisions
    struct ServerMetrics
    {
        std::string server_id;
        std::string server_address;
        uint16_t port;
        uint32_t current_players;
        uint32_t max_players;
        float cpu_usage;
        float memory_usage;
        float network_usage;
        uint32_t latency_ms;
        std::chrono::steady_clock::time_point last_update;
        bool is_healthy;
        bool is_accepting_players;
    };
    
    // Channel metrics
    struct ChannelMetrics
    {
        uint32_t channel_id;
        std::string channel_name;
        uint32_t current_players;
        uint32_t max_players;
        uint32_t map_id;
        float load_factor; // 0.0 to 1.0
        bool is_available;
        bool is_recommended;
    };
    
    // Load balancing strategies
    enum class LoadBalancingStrategy : uint8_t
    {
        RoundRobin = 0,
        LeastConnections = 1,
        LeastLoad = 2,
        LocationBased = 3,
        Custom = 4
    };
    
    // Session migration info
    struct SessionMigrationInfo
    {
        uint64_t session_id;
        std::string source_server;
        std::string target_server;
        uint32_t source_channel;
        uint32_t target_channel;
        std::chrono::steady_clock::time_point start_time;
        bool is_completed;
        std::optional<std::string> error;
    };
    
    class LoadBalancer
    {
    public:
        explicit LoadBalancer(LoadBalancingStrategy strategy = LoadBalancingStrategy::LeastLoad);
        virtual ~LoadBalancer() = default;
        
        // Server management
        auto register_server(const ServerMetrics& server) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto unregister_server(const std::string& server_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto update_server_metrics(const ServerMetrics& metrics) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Channel management
        auto register_channel(const std::string& server_id, const ChannelMetrics& channel) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto update_channel_metrics(const std::string& server_id, const ChannelMetrics& metrics) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Load balancing decisions
        auto get_best_server() const -> std::optional<ServerMetrics>;
        auto get_best_channel(const std::string& server_id = "") const 
            -> std::optional<std::pair<std::string, ChannelMetrics>>;
        auto get_server_for_session(uint64_t session_id) const -> std::optional<std::string>;
        auto get_recommended_channels(uint32_t count = 5) const 
            -> std::vector<std::pair<std::string, ChannelMetrics>>;
        
        // Session migration
        auto initiate_session_migration(uint64_t session_id, const std::string& target_server, 
                                       uint32_t target_channel = 0) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto complete_session_migration(uint64_t session_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto get_migration_status(uint64_t session_id) const -> std::optional<SessionMigrationInfo>;
        
        // Load distribution
        auto distribute_new_player(const std::string& preferred_server = "") 
            -> std::tuple<std::string, uint32_t>; // Returns server_id and channel_id
        auto rebalance_load() -> std::tuple<bool, std::optional<std::string>>;
        auto get_load_distribution() const -> std::unordered_map<std::string, float>;
        
        // Strategy management
        auto set_strategy(LoadBalancingStrategy strategy) -> void;
        auto get_strategy() const -> LoadBalancingStrategy;
        
        // Health monitoring
        auto mark_server_unhealthy(const std::string& server_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto check_server_health() -> void;
        auto get_healthy_servers() const -> std::vector<ServerMetrics>;
        
        // Statistics
        auto get_total_capacity() const -> uint32_t;
        auto get_total_players() const -> uint32_t;
        auto get_average_load() const -> float;
        auto get_server_count() const -> size_t;
        
    private:
        // Internal helpers
        auto calculate_server_score(const ServerMetrics& server) const -> float;
        auto calculate_channel_score(const ChannelMetrics& channel) const -> float;
        auto find_least_loaded_server() const -> std::optional<std::string>;
        auto find_least_loaded_channel(const std::string& server_id) const 
            -> std::optional<uint32_t>;
        auto should_migrate_session(const ServerMetrics& current, const ServerMetrics& target) const 
            -> bool;
        auto cleanup_completed_migrations() -> void;
        
    private:
        LoadBalancingStrategy strategy_;
        std::unordered_map<std::string, ServerMetrics> servers_;
        std::unordered_map<std::string, std::vector<ChannelMetrics>> channels_;
        std::unordered_map<uint64_t, std::string> session_assignments_;
        std::unordered_map<uint64_t, SessionMigrationInfo> active_migrations_;
        
        // Round-robin state
        mutable size_t round_robin_index_;
        
        // Statistics
        std::atomic<uint32_t> total_assignments_{0};
        std::atomic<uint32_t> total_migrations_{0};
        std::atomic<uint32_t> failed_migrations_{0};
        
        mutable std::mutex mutex_;
        
        // Configuration
        static constexpr float HEALTHY_CPU_THRESHOLD = 80.0f;
        static constexpr float HEALTHY_MEMORY_THRESHOLD = 85.0f;
        static constexpr float MIGRATION_THRESHOLD = 0.2f; // 20% load difference
        static constexpr auto HEALTH_CHECK_TIMEOUT = std::chrono::seconds(30);
    };
}