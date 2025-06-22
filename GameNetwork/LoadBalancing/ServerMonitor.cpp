#include "ServerMonitor.h"
#include <Logger.h>
#include <Job.h>

#include <fmt/format.h>
#include <fmt/chrono.h>

using namespace Utilities;
using namespace fmt;

namespace GameNetwork
{
    ServerMonitor::ServerMonitor(std::shared_ptr<LoadBalancer> load_balancer,
                               const ServerMonitorConfig& config)
        : load_balancer_(load_balancer)
        , config_(config)
    {
        thread_pool_ = std::make_shared<Thread::ThreadPool>("ServerMonitor");
        
        // Generate local server ID
        local_server_id_ = fmt::format("server_{}", 
            std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    ServerMonitor::~ServerMonitor()
    {
        stop();
    }
    
    auto ServerMonitor::start() -> std::tuple<bool, std::optional<std::string>>
    {
        if (is_running_)
        {
            return {false, "Server monitor already running"};
        }
        
        should_stop_ = false;
        
        auto [start_success, start_error] = thread_pool_->start();
        if (!start_success)
        {
            return {false, fmt::format("Failed to start thread pool: {}", 
                start_error.value_or("Unknown error"))};
        }        
        // Schedule monitoring jobs
        auto metrics_job = std::make_shared<Thread::Job>(
            Thread::JobPriorities::Normal,
            [this]() -> std::tuple<bool, std::optional<std::string>> {
                monitoring_loop();
                return {true, std::nullopt};
            },
            "MetricsMonitor"
        );
        
        auto health_job = std::make_shared<Thread::Job>(
            Thread::JobPriorities::Normal,
            [this]() -> std::tuple<bool, std::optional<std::string>> {
                health_check_loop();
                return {true, std::nullopt};
            },
            "HealthCheck"
        );
        
        auto migration_job = std::make_shared<Thread::Job>(
            Thread::JobPriorities::Low,
            [this]() -> std::tuple<bool, std::optional<std::string>> {
                migration_check_loop();
                return {true, std::nullopt};
            },
            "MigrationCheck"
        );
        
        thread_pool_->push(metrics_job);
        thread_pool_->push(health_job);
        
        if (config_.enable_auto_migration)
        {
            thread_pool_->push(migration_job);
        }        
        is_running_ = true;
        
        Logger::handle().write(LogTypes::Information,
            fmt::format("Server monitor started for {}", local_server_id_));
        
        return {true, std::nullopt};
    }
    
    auto ServerMonitor::stop() -> std::tuple<bool, std::optional<std::string>>
    {
        if (!is_running_)
        {
            return {true, std::nullopt};
        }
        
        should_stop_ = true;
        
        if (thread_pool_)
        {
            thread_pool_->stop(true);
        }
        
        is_running_ = false;
        
        Logger::handle().write(LogTypes::Information,
            "Server monitor stopped");
        
        return {true, std::nullopt};
    }
    
    auto ServerMonitor::monitoring_loop() -> void
    {
        while (!should_stop_)
        {
            update_local_metrics();            
            std::this_thread::sleep_for(config_.update_interval);
        }
    }
    
    auto ServerMonitor::health_check_loop() -> void
    {
        while (!should_stop_)
        {
            load_balancer_->check_server_health();
            
            std::this_thread::sleep_for(config_.health_check_interval);
        }
    }
    
    auto ServerMonitor::migration_check_loop() -> void
    {
        while (!should_stop_)
        {
            if (should_trigger_migration())
            {
                auto [success, error] = trigger_rebalance();
                if (!success)
                {
                    Logger::handle().write(LogTypes::Error,
                        fmt::format("Failed to trigger rebalance: {}", error.value_or("Unknown error")));
                }
            }
            
            std::this_thread::sleep_for(config_.migration_check_interval);
        }
    }
    
    auto ServerMonitor::update_local_metrics() -> void
    {
        ServerMetrics metrics = collect_local_metrics();        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_metrics_ = metrics;
        }
        
        auto [success, error] = load_balancer_->update_server_metrics(metrics);
        if (!success)
        {
            Logger::handle().write(LogTypes::Error,
                fmt::format("Failed to update server metrics: {}", 
                    error.value_or("Unknown error")));
        }
    }
    
    auto ServerMonitor::collect_local_metrics() -> ServerMetrics
    {
        ServerMetrics metrics;
        metrics.server_id = local_server_id_;
        
        if (metrics_provider_)
        {
            metrics = metrics_provider_();
        }
        else
        {
            // Default metrics collection
            metrics.server_address = "localhost";
            metrics.port = 8080;
            metrics.current_players = 0;
            metrics.max_players = 1000;
            metrics.cpu_usage = 50.0f; // TODO: Implement actual CPU monitoring
            metrics.memory_usage = 60.0f; // TODO: Implement actual memory monitoring
            metrics.network_usage = 30.0f;
            metrics.latency_ms = 10;
            metrics.is_healthy = true;
            metrics.is_accepting_players = true;
        }        
        metrics.last_update = std::chrono::steady_clock::now();
        return metrics;
    }
    
    auto ServerMonitor::should_trigger_migration() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if our server is overloaded
        float load_ratio = static_cast<float>(last_metrics_.current_players) / static_cast<float>(last_metrics_.max_players);
        
        if (load_ratio > (1.0f - config_.migration_threshold))
        {
            // Check if there are less loaded servers
            auto distribution = load_balancer_->get_load_distribution();
            
            for (const auto& [server_id, server_load] : distribution)
            {
                if (server_id != local_server_id_ && 
                    server_load < (load_ratio - config_.migration_threshold))
                {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    auto ServerMonitor::set_metrics_provider(MetricsCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_provider_ = callback;
    }
    
    auto ServerMonitor::set_migration_handler(MigrationCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        migration_handler_ = callback;
    }

    auto ServerMonitor::trigger_rebalance() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (migration_handler_)
        {
            try
            {
                // Create a dummy migration info for the callback
                SessionMigrationInfo migration_info;
                migration_info.session_id = 0; // Placeholder
                migration_info.source_server = local_server_id_;
                migration_info.target_server = ""; // Will be determined by load balancer
                migration_info.source_channel = 0;
                migration_info.target_channel = 0;
                migration_info.start_time = std::chrono::steady_clock::now();
                migration_info.is_completed = false;
                migration_info.error = std::nullopt;
                
                migration_handler_(migration_info);
                Logger::handle().write(LogTypes::Information, 
                    "Server rebalancing triggered successfully");
                return {true, std::nullopt};
            }
            catch (const std::exception& e)
            {
                std::string error_msg = std::string("Migration handler failed: ") + e.what();
                Logger::handle().write(LogTypes::Error, error_msg);
                return {false, error_msg};
            }
        }
        else
        {
            // Default rebalancing behavior - delegate to load balancer
            auto [success, result] = load_balancer_->rebalance_load();
            if (success)
            {
                Logger::handle().write(LogTypes::Information, 
                    "Default rebalancing completed successfully");
            }
            else
            {
                Logger::handle().write(LogTypes::Error, 
                    std::string(format("Default rebalancing failed: {}", 
                        result.value_or("Unknown error"))));
            }
            return {success, result};
        }
    }
}