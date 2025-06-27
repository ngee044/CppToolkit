#pragma once

#include <GameNetworkConstants.h>
#include <chrono>
#include <string>
#include <vector>
#include <tuple>
#include <optional>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#elif __linux__
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#elif __APPLE__
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#endif

namespace GameNetwork
{
    namespace Monitoring
    {
        struct NetworkStats
        {
            uint64_t bytes_sent;
            uint64_t bytes_received;
            uint64_t packets_sent;
            uint64_t packets_received;
            float bandwidth_usage_mbps;
            float packet_loss_rate;
            std::chrono::milliseconds average_latency;
            
            // Additional missing members
            uint64_t bytes_sent_per_second;
            uint64_t bytes_received_per_second;
            uint64_t total_bytes_sent;
            uint64_t total_bytes_received;
            uint32_t active_connections;
        };

        struct SystemMetrics
        {
            float cpu_usage_percent;
            float memory_usage_percent;
            uint64_t memory_used_bytes;
            uint64_t memory_available_bytes;
            NetworkStats network_stats;
            float disk_io_read_mbps;
            float disk_io_write_mbps;
            std::chrono::steady_clock::time_point timestamp;
        };
        class SystemMonitor
        {
        public:
            SystemMonitor();
            virtual ~SystemMonitor();

            // Initialize monitoring
            auto initialize() -> std::tuple<bool, std::optional<std::string>>;
            auto shutdown() -> void;

            // Get current metrics
            auto get_current_metrics() -> SystemMetrics;
            auto get_cpu_usage() -> float;
            auto get_memory_usage() -> float;
            auto get_network_stats() -> NetworkStats;

            // Historical data
            auto start_recording(std::chrono::seconds interval) -> void;
            auto stop_recording() -> void;
            auto get_historical_metrics(std::chrono::seconds duration) 
                -> std::vector<SystemMetrics>;

            // Alerts
            struct AlertThresholds
            {
                float cpu_alert_percent = 90.0f;
                float memory_alert_percent = 85.0f;
                float network_alert_mbps = 900.0f; // For gigabit
                std::chrono::milliseconds latency_alert = std::chrono::milliseconds(100);
            };

            auto set_alert_thresholds(const AlertThresholds& thresholds) -> void;
            auto check_alerts() -> std::vector<std::string>;

        private:
            // Platform-specific implementations
#ifdef _WIN32
            auto get_cpu_usage_windows() -> float;
            auto get_memory_usage_windows() -> std::tuple<float, uint64_t, uint64_t>;            auto get_network_stats_windows() -> NetworkStats;
            
            PDH_HQUERY cpu_query_;
            PDH_HCOUNTER cpu_counter_;
            ULARGE_INTEGER last_cpu_, last_sys_cpu_, last_user_cpu_;
#elif __linux__
            auto get_cpu_usage_linux() -> float;
            auto get_memory_usage_linux() -> std::tuple<float, uint64_t, uint64_t>;
            auto get_network_stats_linux() -> NetworkStats;
            
            struct CpuStats {
                uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
            };
            CpuStats last_cpu_stats_;
#elif __APPLE__
            auto get_cpu_usage_mac() -> float;
            auto get_memory_usage_mac() -> std::tuple<float, uint64_t, uint64_t>;
            auto get_network_stats_mac() -> NetworkStats;
            
            host_cpu_load_info_data_t last_cpu_info_;
#endif

            // Common helpers
            auto update_network_statistics() -> void;
            auto calculate_bandwidth_usage() -> float;
            
        private:
            std::atomic<bool> initialized_{false};
            std::atomic<bool> recording_{false};
            
            // Current metrics
            SystemMetrics current_metrics_;
            mutable std::mutex metrics_mutex_;
            
            // Historical data
            std::vector<SystemMetrics> historical_metrics_;
            std::chrono::seconds recording_interval_;
            std::thread recording_thread_;
            
            // Network tracking
            std::chrono::steady_clock::time_point last_network_update_;
            uint64_t last_bytes_sent_;
            uint64_t last_bytes_received_;
            
            // Alert thresholds
            AlertThresholds alert_thresholds_;
        };
    }
}