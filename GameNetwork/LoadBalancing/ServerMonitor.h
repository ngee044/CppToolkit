#pragma once

#include "LoadBalancer.h"
#include <ThreadPool.h>
#include <chrono>
#include <functional>

namespace GameNetwork
{
    // Server monitoring configuration
    struct ServerMonitorConfig
    {
        std::chrono::seconds update_interval{5};
        std::chrono::seconds health_check_interval{10};
        std::chrono::seconds migration_check_interval{30};
        bool enable_auto_migration{true};
        float migration_threshold{0.2f}; // 20% load difference
    };
    
    class ServerMonitor
    {
    public:
        explicit ServerMonitor(std::shared_ptr<LoadBalancer> load_balancer,
                             const ServerMonitorConfig& config = {});
        virtual ~ServerMonitor();
        
        // Start/stop monitoring
        auto start() -> std::tuple<bool, std::optional<std::string>>;
        auto stop() -> std::tuple<bool, std::optional<std::string>>;
        auto is_running() const -> bool;
        
        // Manual operations
        auto trigger_rebalance() -> std::tuple<bool, std::optional<std::string>>;
        auto update_local_metrics() -> void;
        
        // Callbacks
        using MetricsCallback = std::function<ServerMetrics()>;
        using MigrationCallback = std::function<void(const SessionMigrationInfo&)>;
        
        auto set_metrics_provider(MetricsCallback callback) -> void;
        auto set_migration_handler(MigrationCallback callback) -> void;        
    private:
        auto monitoring_loop() -> void;
        auto health_check_loop() -> void;
        auto migration_check_loop() -> void;
        auto collect_local_metrics() -> ServerMetrics;
        auto should_trigger_migration() const -> bool;
        
    private:
        std::shared_ptr<LoadBalancer> load_balancer_;
        ServerMonitorConfig config_;
        std::shared_ptr<Thread::ThreadPool> thread_pool_;
        
        std::atomic<bool> is_running_{false};
        std::atomic<bool> should_stop_{false};
        
        MetricsCallback metrics_provider_;
        MigrationCallback migration_handler_;
        
        std::string local_server_id_;
        ServerMetrics last_metrics_;
        
        mutable std::mutex mutex_;
    };
}