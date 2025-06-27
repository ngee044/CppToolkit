#pragma once

#include <GameNetworkConstants.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <mutex>
#include <tuple>
#include <optional>
#include <atomic>

namespace GameNetwork
{
    // Forward declarations
    class ServerMonitor;
    class LoadBalancer;
    class SeamlessMigration;
    
    struct ServerMetrics
    {
        float cpu_usage;
        float memory_usage;
        uint32_t active_connections;
        uint32_t requests_per_second;
        float average_latency_ms;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    struct ScalingPolicy
    {
        // Scale up thresholds
        float cpu_threshold_up = 80.0f;
        float memory_threshold_up = 85.0f;
        uint32_t connection_threshold_up = 1000;
        
        // Scale down thresholds
        float cpu_threshold_down = 20.0f;
        float memory_threshold_down = 30.0f;
        uint32_t connection_threshold_down = 100;
        
        // Scaling parameters
        std::chrono::seconds cooldown_period{300}; // 5 minutes
        uint32_t min_instances = 1;
        uint32_t max_instances = 10;
        float scale_up_factor = 1.5f;
        float scale_down_factor = 0.8f;
    };
    
    class AutoScaler
    {
    public:
        AutoScaler();
        ~AutoScaler();
        
        // Configuration
        auto configure(const ScalingPolicy& policy) -> void;
        auto set_server_monitor(std::shared_ptr<ServerMonitor> monitor) -> void;
        auto set_load_balancer(std::shared_ptr<LoadBalancer> balancer) -> void;
        
        // Scaling operations
        auto evaluate_scaling_need() -> std::tuple<bool, std::optional<std::string>>;
        auto scale_up(uint32_t additional_instances) -> std::tuple<bool, std::optional<std::string>>;
        auto scale_down(uint32_t instances_to_remove) -> std::tuple<bool, std::optional<std::string>>;
        
        // Manual control
        auto enable_auto_scaling(bool enable) -> void;
        auto force_scale_to_instances(uint32_t target_instances) -> std::tuple<bool, std::optional<std::string>>;
        
        // Monitoring
        auto update_server_metrics(const std::string& server_id, const ServerMetrics& metrics) -> void;
        auto get_current_instance_count() const -> uint32_t;
        auto get_recommended_instance_count() const -> uint32_t;
        
        // Callbacks
        using ScalingCallback = std::function<void(uint32_t from_count, uint32_t to_count)>;
        auto set_scaling_callback(ScalingCallback callback) -> void;
        
        // Statistics
        struct ScalingStats
        {
            uint64_t scale_up_events;
            uint64_t scale_down_events;
            uint64_t scaling_decisions_made;
            std::chrono::steady_clock::time_point last_scaling_time;
            float average_cpu_usage;
            float average_memory_usage;
        };
        
        auto get_statistics() const -> ScalingStats;
        
    private:
        auto calculate_aggregate_metrics() const -> ServerMetrics;
        auto should_scale_up(const ServerMetrics& metrics) const -> bool;
        auto should_scale_down(const ServerMetrics& metrics) const -> bool;
        auto is_in_cooldown() const -> bool;
        
    private:
        mutable std::mutex mutex_;
        
        // Configuration
        ScalingPolicy policy_;
        std::atomic<bool> auto_scaling_enabled_;
        
        // Dependencies
        std::weak_ptr<ServerMonitor> server_monitor_;
        std::weak_ptr<LoadBalancer> load_balancer_;
        std::weak_ptr<SeamlessMigration> seamless_migration_;
        
        // State
        std::unordered_map<std::string, ServerMetrics> server_metrics_;
        std::chrono::steady_clock::time_point last_scaling_time_;
        uint32_t current_instance_count_;
        
        // Callbacks
        ScalingCallback scaling_callback_;
        
        // Statistics
        ScalingStats stats_;
    };
}
