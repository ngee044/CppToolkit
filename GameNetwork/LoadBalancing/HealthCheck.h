#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <optional>
#include <tuple>
#include <mutex>

namespace GameNetwork
{
	namespace LoadBalancing
	{
		// Health check status
		enum class HealthStatus
		{
			Healthy,
			Degraded,
			Unhealthy,
			Unknown
		};
        
		// Health check result
		struct HealthCheckResult
		{
			HealthStatus status;
			std::chrono::milliseconds response_time;
			std::string details;
			std::chrono::steady_clock::time_point timestamp;
            
			// Additional metrics
			float cpu_usage;
			float memory_usage;
			uint32_t active_connections;
			uint32_t request_rate;
		};
        
		// Health check configuration
		struct HealthCheckConfig
		{
			std::chrono::seconds check_interval{30};
			std::chrono::seconds timeout{5};
			uint32_t failure_threshold{3};        // Failures before marking unhealthy
			uint32_t success_threshold{2};        // Successes before marking healthy
			bool enable_tcp_check{true};
			bool enable_http_check{false};
			bool enable_custom_check{true};
			std::string http_endpoint{"/health"};
			uint16_t check_port{0};              // 0 = use server port
		};
        
		// Health checker interface
		class IHealthChecker
		{
		public:
			virtual ~IHealthChecker() = default;
            
			virtual auto check_health() -> std::tuple<bool, HealthCheckResult> = 0;
			virtual auto get_check_type() const -> std::string = 0;
		};
        
		// TCP health checker
		class TcpHealthChecker : public IHealthChecker
		{
		public:
			TcpHealthChecker(const std::string& host, uint16_t port, 
							 std::chrono::seconds timeout = std::chrono::seconds(5));
            
			auto check_health() -> std::tuple<bool, HealthCheckResult> override;
			auto get_check_type() const -> std::string override { return "TCP"; }
            
		private:
			std::string host_;
			uint16_t port_;
			std::chrono::seconds timeout_;
		};
        
		// HTTP health checker
		class HttpHealthChecker : public IHealthChecker
		{
		public:
			HttpHealthChecker(const std::string& url, 
							  std::chrono::seconds timeout = std::chrono::seconds(5));
            
			auto check_health() -> std::tuple<bool, HealthCheckResult> override;
			auto get_check_type() const -> std::string override { return "HTTP"; }
            
		private:
			std::string url_;
			std::chrono::seconds timeout_;
		};
        
		// Custom health checker
		class CustomHealthChecker : public IHealthChecker
		{
		public:
			using CheckFunction = std::function<std::tuple<bool, HealthCheckResult>()>;
            
			CustomHealthChecker(CheckFunction check_func, const std::string& name = "Custom");
            
			auto check_health() -> std::tuple<bool, HealthCheckResult> override;
			auto get_check_type() const -> std::string override { return name_; }
            
		private:
			CheckFunction check_func_;
			std::string name_;
		};
        
		// Health monitor for a single server
		class ServerHealthMonitor
		{
		public:
			ServerHealthMonitor(const std::string& server_id, const HealthCheckConfig& config);
			~ServerHealthMonitor();
            
			// Start/stop monitoring
			auto start() -> void;
			auto stop() -> void;
			auto is_running() const -> bool { return is_running_; }
            
			// Add health checkers
			auto add_health_checker(std::unique_ptr<IHealthChecker> checker) -> void;
            
			// Get current health status
			auto get_current_status() const -> HealthStatus { return current_status_; }
			auto get_last_check_result() const -> std::optional<HealthCheckResult>;
			auto get_failure_count() const -> uint32_t { return consecutive_failures_; }
            
			// Callbacks
			using StatusChangeCallback = std::function<void(const std::string&, HealthStatus, HealthStatus)>;
			auto on_status_change(StatusChangeCallback callback) -> void { status_change_callback_ = callback; }
            
			// Force immediate check
			auto check_now() -> std::tuple<bool, HealthCheckResult>;
            
		private:
			std::string server_id_;
			HealthCheckConfig config_;
			std::vector<std::unique_ptr<IHealthChecker>> health_checkers_;
            
			std::atomic<HealthStatus> current_status_{HealthStatus::Unknown};
			std::atomic<uint32_t> consecutive_failures_{0};
			std::atomic<uint32_t> consecutive_successes_{0};
            
			std::optional<HealthCheckResult> last_result_;
			mutable std::mutex result_mutex_;
            
			std::thread monitor_thread_;
			std::atomic<bool> is_running_{false};
			std::atomic<bool> should_stop_{false};
            
			StatusChangeCallback status_change_callback_;
            
			auto monitor_loop() -> void;
			auto perform_health_checks() -> HealthCheckResult;
			auto update_status(const HealthCheckResult& result) -> void;
		};
        
	} // namespace LoadBalancing
} // namespace GameNetwork
