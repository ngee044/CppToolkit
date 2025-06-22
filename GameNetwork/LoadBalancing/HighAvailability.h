#pragma once

#include <LoadBalancer.h>
#include <SeamlessMigration.h>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <functional>

namespace GameNetwork
{
    namespace LoadBalancing
    {
        enum class ServerHealthStatus
        {
            Healthy,
            Degraded,
            Unhealthy,
            Failed,
            Recovering
        };

        struct HealthCheck
        {
            std::string endpoint;
            std::chrono::milliseconds timeout;
            uint32_t consecutive_failures;
            std::chrono::steady_clock::time_point last_check;
            bool is_critical;
        };

        struct ServerFailureInfo
        {
            std::string server_id;
            std::chrono::steady_clock::time_point failure_time;
            std::string failure_reason;
            uint32_t affected_sessions;
            bool recovery_attempted;
        };

        // Circuit breaker pattern
        enum class CircuitState
        {
            Closed,     // Normal operation
            Open,       // Failures exceeded threshold
            HalfOpen    // Testing if service recovered
        };
        struct CircuitBreaker
        {
            CircuitState state;
            uint32_t failure_count;
            uint32_t success_count;
            std::chrono::steady_clock::time_point last_state_change;
            std::chrono::steady_clock::time_point next_retry_time;
        };

        class HighAvailability
        {
        public:
            HighAvailability(std::shared_ptr<LoadBalancer> load_balancer,
                           std::shared_ptr<SeamlessMigration> migration_manager);
            virtual ~HighAvailability();

            // Health monitoring
            auto start_health_monitoring() -> void;
            auto stop_health_monitoring() -> void;
            auto add_health_check(const std::string& server_id, 
                                const HealthCheck& check) -> void;
            auto get_server_health(const std::string& server_id) const 
                -> ServerHealthStatus;

            // Failure detection
            auto detect_server_failure() -> void;
            auto mark_server_failed(const std::string& server_id, 
                                  const std::string& reason) -> void;
            auto is_server_available(const std::string& server_id) const -> bool;

            // Automatic failover
            auto automatic_failover(const std::string& failed_server) -> void;
            auto block_incoming_traffic(const std::string& server_id) -> void;
            auto recover_sessions_from_backup(const std::string& failed_server) 
                -> std::tuple<bool, std::optional<std::string>>;
            auto redistribute_load(const std::vector<std::string>& healthy_servers) -> void;
            // Standby server management
            auto maintain_standby_servers(uint32_t count) -> void;
            auto activate_standby_server() -> std::optional<std::string>;
            auto get_standby_servers() const -> std::vector<std::string>;

            // Circuit breaker
            auto get_circuit_breaker(const std::string& server_id) -> CircuitBreaker&;
            auto should_allow_request(const std::string& server_id) -> bool;
            auto record_success(const std::string& server_id) -> void;
            auto record_failure(const std::string& server_id) -> void;

            // Session backup
            auto enable_session_replication(bool enable) -> void;
            auto set_replication_factor(uint32_t factor) -> void;
            auto get_session_replicas(uint64_t session_id) const 
                -> std::vector<std::string>;

            // Configuration
            struct HAConfig
            {
                std::chrono::seconds health_check_interval = std::chrono::seconds(5);
                uint32_t max_consecutive_failures = 3;
                std::chrono::seconds failure_timeout = std::chrono::seconds(30);
                
                // Circuit breaker settings
                uint32_t circuit_failure_threshold = 5;
                float circuit_failure_rate_threshold = 0.5f;
                std::chrono::seconds circuit_timeout = std::chrono::seconds(60);
                std::chrono::seconds circuit_half_open_timeout = std::chrono::seconds(30);
                
                // Replication settings
                uint32_t replication_factor = 2;
                bool enable_async_replication = true;
                
                // Standby settings
                uint32_t standby_server_count = 2;
                float standby_activation_threshold = 0.9f; // Load threshold
            };
            auto configure(const HAConfig& config) -> void;

            // Callbacks
            using FailureCallback = std::function<void(const ServerFailureInfo&)>;
            using RecoveryCallback = std::function<void(const std::string&)>;
            
            auto set_failure_callback(FailureCallback callback) -> void;
            auto set_recovery_callback(RecoveryCallback callback) -> void;

            // Statistics
            struct HAStats
            {
                uint64_t health_checks_performed;
                uint64_t failures_detected;
                uint64_t failovers_performed;
                uint64_t sessions_recovered;
                uint64_t circuit_breaker_trips;
                std::chrono::milliseconds average_failover_time;
                float availability_percentage;
            };

            auto get_statistics() const -> HAStats;

        private:
            // Health check implementation
            auto perform_health_check(const std::string& server_id) -> bool;
            auto update_server_health(const std::string& server_id, bool is_healthy) -> void;
            auto health_monitoring_loop() -> void;

            // Failover implementation
            auto initiate_failover(const std::string& failed_server) -> void;
            auto select_failover_targets(uint32_t session_count) -> std::vector<std::string>;
            auto transfer_sessions(const std::vector<uint64_t>& sessions,
                                 const std::vector<std::string>& targets) -> void;

            // Circuit breaker implementation
            auto update_circuit_state(CircuitBreaker& breaker) -> void;
            auto should_open_circuit(const CircuitBreaker& breaker) const -> bool;
            auto should_close_circuit(const CircuitBreaker& breaker) const -> bool;

        private:
            std::shared_ptr<LoadBalancer> load_balancer_;
            std::shared_ptr<SeamlessMigration> migration_manager_;
            
            HAConfig config_;
            HAStats stats_;
            
            // Server health tracking
            std::unordered_map<std::string, ServerHealthStatus> server_health_;
            std::unordered_map<std::string, std::vector<HealthCheck>> health_checks_;
            std::unordered_map<std::string, CircuitBreaker> circuit_breakers_;
            
            // Standby servers
            std::unordered_set<std::string> standby_servers_;
            
            // Session replication
            std::unordered_map<uint64_t, std::vector<std::string>> session_replicas_;
            
            // Monitoring
            std::atomic<bool> monitoring_active_{false};
            std::thread monitoring_thread_;
            
            // Callbacks
            FailureCallback failure_callback_;
            RecoveryCallback recovery_callback_;
            
            mutable std::mutex mutex_;
        };
    }
}