#pragma once

#include <LoadBalancer.h>
#include <SystemMonitor.h>
#include <chrono>
#include <functional>
#include <queue>
#include <atomic>
#include <thread>

namespace GameNetwork
{
    namespace LoadBalancing
    {
        enum class ScalingAction
        {
            None,
            ScaleUp,
            ScaleDown,
            Emergency
        };

        struct ScalingPolicy
        {
            float scale_up_cpu_threshold = 80.0f;
            float scale_up_memory_threshold = 85.0f;
            uint32_t scale_up_queue_threshold = 100;
            std::chrono::milliseconds scale_up_response_time = std::chrono::milliseconds(200);
            
            float scale_down_cpu_threshold = 20.0f;
            float scale_down_memory_threshold = 30.0f;
            std::chrono::minutes scale_down_cooldown = std::chrono::minutes(10);
            
            uint32_t min_servers = 2;
            uint32_t max_servers = 50;
            uint32_t scale_increment = 1;
            
            bool enable_predictive_scaling = true;
            bool enable_emergency_scaling = true;
        };

        struct ServerProvisionRequest
        {
            std::string request_id;
            ScalingAction action;
            uint32_t server_count;
            std::chrono::steady_clock::time_point requested_at;
            std::chrono::steady_clock::time_point completed_at;
            bool is_completed;
            std::optional<std::string> error;
        };
        class AutoScaler
        {
        public:
            AutoScaler(std::shared_ptr<LoadBalancer> load_balancer,
                      std::shared_ptr<Monitoring::SystemMonitor> system_monitor);
            virtual ~AutoScaler();

            // Configuration
            auto configure(const ScalingPolicy& policy) -> void;
            auto get_policy() const -> ScalingPolicy;

            // Scaling triggers
            auto scale_up_trigger() -> bool;
            auto scale_down_trigger() -> bool;
            auto emergency_scale_trigger() -> bool;

            // Scaling operations
            auto provision_new_server() -> std::tuple<bool, std::optional<std::string>>;
            auto provision_servers(uint32_t count) -> std::tuple<bool, std::optional<std::string>>;
            auto deprovision_server(const std::string& server_id) 
                -> std::tuple<bool, std::optional<std::string>>;
            auto deprovision_servers(const std::vector<std::string>& server_ids)
                -> std::tuple<bool, std::optional<std::string>>;

            // Session migration
            auto migrate_sessions_before_shutdown(const std::string& server_id) -> void;
            auto ensure_graceful_shutdown(const std::string& server_id,
                                        std::chrono::seconds timeout) 
                -> std::tuple<bool, std::optional<std::string>>;

            // Auto scaling control
            auto start_auto_scaling() -> void;
            auto stop_auto_scaling() -> void;
            auto is_auto_scaling_enabled() const -> bool;

            // Callbacks
            using ProvisionCallback = std::function<std::string()>; // Returns new server_id
            using DeprovisionCallback = std::function<bool(const std::string&)>;
            using MigrationCallback = std::function<void(uint64_t, const std::string&, const std::string&)>;            
            auto set_provision_callback(ProvisionCallback callback) -> void;
            auto set_deprovision_callback(DeprovisionCallback callback) -> void;
            auto set_migration_callback(MigrationCallback callback) -> void;

            // Statistics
            struct ScalingStats
            {
                uint64_t scale_up_events;
                uint64_t scale_down_events;
                uint64_t emergency_scale_events;
                uint64_t servers_provisioned;
                uint64_t servers_deprovisioned;
                uint64_t migrations_performed;
                std::chrono::milliseconds average_provision_time;
                std::chrono::milliseconds average_migration_time;
            };

            auto get_statistics() const -> ScalingStats;

        private:
            auto scaling_loop() -> void;
            auto evaluate_scaling_need() -> ScalingAction;
            auto execute_scaling_action(ScalingAction action) -> void;
            auto calculate_required_servers() -> uint32_t;
            auto select_servers_for_deprovisioning(uint32_t count) -> std::vector<std::string>;
            auto wait_for_cooldown() -> bool;

        private:
            std::shared_ptr<LoadBalancer> load_balancer_;
            std::shared_ptr<Monitoring::SystemMonitor> system_monitor_;
            
            ScalingPolicy policy_;
            ScalingStats stats_;
            
            // Callbacks
            ProvisionCallback provision_callback_;
            DeprovisionCallback deprovision_callback_;
            MigrationCallback migration_callback_;
            
            // Scaling state
            std::atomic<bool> auto_scaling_enabled_{false};
            std::thread scaling_thread_;
            std::chrono::steady_clock::time_point last_scale_up_;
            std::chrono::steady_clock::time_point last_scale_down_;
            
            // Request tracking
            std::queue<ServerProvisionRequest> pending_requests_;
            
            mutable std::mutex mutex_;
        };
    }
}