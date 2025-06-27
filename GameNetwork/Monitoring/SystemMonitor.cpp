#include "SystemMonitor.h"
#include <sstream>

namespace GameNetwork::Monitoring
{
    SystemMonitor::SystemMonitor()
    {
#ifdef _WIN32
        cpu_query_ = nullptr;
        cpu_counter_ = nullptr;
#endif
    }
    
    SystemMonitor::~SystemMonitor()
    {
        shutdown();
    }
    
    auto SystemMonitor::initialize() -> std::tuple<bool, std::optional<std::string>>
    {
        if (initialized_.exchange(true))
        {
            return {true, std::nullopt};
        }
        
#ifdef _WIN32
        if (PdhOpenQuery(nullptr, 0, &cpu_query_) != ERROR_SUCCESS)
        {
            initialized_ = false;
            return {false, "Failed to open PDH query"};
        }
        
        if (PdhAddEnglishCounterA(cpu_query_, "\\Processor(_Total)\\% Processor Time", 
            0, &cpu_counter_) != ERROR_SUCCESS)
        {
            PdhCloseQuery(cpu_query_);
            cpu_query_ = nullptr;
            initialized_ = false;
            return {false, "Failed to add CPU counter"};
        }
        
        PdhCollectQueryData(cpu_query_);
#endif
        
        // Initialize network statistics
        last_network_update_ = std::chrono::steady_clock::now();
        last_bytes_sent_ = 0;
        last_bytes_received_ = 0;
        
        return {true, std::nullopt};
    }
    
    auto SystemMonitor::shutdown() -> void
    {
        if (!initialized_.exchange(false))
        {
            return;
        }
        
        stop_recording();
        
#ifdef _WIN32
        if (cpu_query_)
        {
            PdhCloseQuery(cpu_query_);
            cpu_query_ = nullptr;
        }
#endif
    }
    
    auto SystemMonitor::get_current_metrics() -> SystemMetrics
    {
        SystemMetrics metrics;
        metrics.timestamp = std::chrono::steady_clock::now();
        
        metrics.cpu_usage_percent = get_cpu_usage();
        metrics.memory_usage_percent = get_memory_usage();
        
#ifdef _WIN32
        auto [mem_percent, mem_used, mem_avail] = get_memory_usage_windows();
        metrics.memory_usage_percent = mem_percent;
        metrics.memory_used_bytes = mem_used;
        metrics.memory_available_bytes = mem_avail;
#elif __linux__
        auto [mem_percent, mem_used, mem_avail] = get_memory_usage_linux();
        metrics.memory_usage_percent = mem_percent;
        metrics.memory_used_bytes = mem_used;
        metrics.memory_available_bytes = mem_avail;
#elif __APPLE__
        auto [mem_percent, mem_used, mem_avail] = get_memory_usage_mac();
        metrics.memory_usage_percent = mem_percent;
        metrics.memory_used_bytes = mem_used;
        metrics.memory_available_bytes = mem_avail;
#endif
        
        metrics.network_stats = get_network_stats();
        
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_metrics_ = metrics;
        
        return metrics;
    }
    
    auto SystemMonitor::get_cpu_usage() -> float
    {
#ifdef _WIN32
        return get_cpu_usage_windows();
#elif __linux__
        return get_cpu_usage_linux();
#elif __APPLE__
        return get_cpu_usage_mac();
#else
        return 0.0f;
#endif
    }
    
    auto SystemMonitor::get_memory_usage() -> float
    {
#ifdef _WIN32
        auto [percent, used, avail] = get_memory_usage_windows();
        return percent;
#elif __linux__
        auto [percent, used, avail] = get_memory_usage_linux();
        return percent;
#elif __APPLE__
        auto [percent, used, avail] = get_memory_usage_mac();
        return percent;
#else
        return 0.0f;
#endif
    }
    
    auto SystemMonitor::get_network_stats() -> NetworkStats
    {
#ifdef _WIN32
        return get_network_stats_windows();
#elif __linux__
        return get_network_stats_linux();
#elif __APPLE__
        return get_network_stats_mac();
#else
        return NetworkStats{};
#endif
    }
    
#ifdef _WIN32
    auto SystemMonitor::get_cpu_usage_windows() -> float
    {
        if (!cpu_query_ || !cpu_counter_)
        {
            return 0.0f;
        }
        
        PDH_FMT_COUNTERVALUE counter_val;
        if (PdhCollectQueryData(cpu_query_) != ERROR_SUCCESS)
        {
            return 0.0f;
        }
        
        if (PdhGetFormattedCounterValue(cpu_counter_, PDH_FMT_DOUBLE, 
            nullptr, &counter_val) != ERROR_SUCCESS)
        {
            return 0.0f;
        }
        
        return static_cast<float>(counter_val.doubleValue);
    }
    
    auto SystemMonitor::get_memory_usage_windows() -> std::tuple<float, uint64_t, uint64_t>
    {
        MEMORYSTATUSEX mem_info;
        mem_info.dwLength = sizeof(MEMORYSTATUSEX);
        
        if (!GlobalMemoryStatusEx(&mem_info))
        {
            return {0.0f, 0, 0};
        }
        
        uint64_t total_phys = mem_info.ullTotalPhys;
        uint64_t avail_phys = mem_info.ullAvailPhys;
        uint64_t used_phys = total_phys - avail_phys;
        
        float percent = (static_cast<float>(used_phys) / static_cast<float>(total_phys)) * 100.0f;
        
        return {percent, used_phys, avail_phys};
    }
    
    auto SystemMonitor::get_network_stats_windows() -> NetworkStats
    {
        NetworkStats stats{};
        
        // Get network interface statistics using Windows Performance Counters
        // In a real implementation, this would use PDH (Performance Data Helper) APIs
        
        // For now, simulate realistic network stats
        static uint64_t total_bytes_sent = 0;
        static uint64_t total_bytes_received = 0;
        static auto last_check = std::chrono::steady_clock::now();
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - last_check).count();
        
        if (elapsed > 0.0f)
        {
            // Simulate some network activity
            uint64_t new_bytes_sent = static_cast<uint64_t>(1024 * 1024 * elapsed); // 1 MB/s
            uint64_t new_bytes_received = static_cast<uint64_t>(2 * 1024 * 1024 * elapsed); // 2 MB/s
            
            stats.bytes_sent_per_second = static_cast<float>(new_bytes_sent) / elapsed;
            stats.bytes_received_per_second = static_cast<float>(new_bytes_received) / elapsed;
            
            total_bytes_sent += new_bytes_sent;
            total_bytes_received += new_bytes_received;
            
            stats.total_bytes_sent = total_bytes_sent;
            stats.total_bytes_received = total_bytes_received;
            
            last_check = now;
        }
        
        stats.active_connections = 42; // Simulated active connections
        stats.average_latency = std::chrono::milliseconds(15); // Simulated latency
        
        return stats;
    }
#endif
    
    auto SystemMonitor::start_recording(std::chrono::seconds interval) -> void
    {
        if (recording_.exchange(true))
        {
            return;
        }
        
        recording_interval_ = interval;
        recording_thread_ = std::thread([this]()
        {
            while (recording_)
            {
                auto metrics = get_current_metrics();
                
                {
                    std::lock_guard<std::mutex> lock(metrics_mutex_);
                    historical_metrics_.push_back(metrics);
                    
                    // Keep only last hour of data
                    size_t max_entries = 3600 / recording_interval_.count();
                    if (historical_metrics_.size() > max_entries)
                    {
                        historical_metrics_.erase(
                            historical_metrics_.begin(), 
                            historical_metrics_.begin() + (historical_metrics_.size() - max_entries)
                        );
                    }
                }
                
                std::this_thread::sleep_for(recording_interval_);
            }
        });
    }
    
    auto SystemMonitor::stop_recording() -> void
    {
        if (!recording_.exchange(false))
        {
            return;
        }
        
        if (recording_thread_.joinable())
        {
            recording_thread_.join();
        }
    }
    
    auto SystemMonitor::get_historical_metrics(std::chrono::seconds duration) 
        -> std::vector<SystemMetrics>
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto cutoff_time = now - duration;
        
        std::vector<SystemMetrics> result;
        for (const auto& metric : historical_metrics_)
        {
            if (metric.timestamp >= cutoff_time)
            {
                result.push_back(metric);
            }
        }
        
        return result;
    }
    
    auto SystemMonitor::set_alert_thresholds(const AlertThresholds& thresholds) -> void
    {
        alert_thresholds_ = thresholds;
    }
    
    auto SystemMonitor::check_alerts() -> std::vector<std::string>
    {
        std::vector<std::string> alerts;
        
        auto metrics = get_current_metrics();
        
        if (metrics.cpu_usage_percent > alert_thresholds_.cpu_alert_percent)
        {
            std::stringstream ss;
            ss << "High CPU usage: " << metrics.cpu_usage_percent << "%";
            alerts.push_back(ss.str());
        }
        
        if (metrics.memory_usage_percent > alert_thresholds_.memory_alert_percent)
        {
            std::stringstream ss;
            ss << "High memory usage: " << metrics.memory_usage_percent << "%";
            alerts.push_back(ss.str());
        }
        
        if (metrics.network_stats.bandwidth_usage_mbps > alert_thresholds_.network_alert_mbps)
        {
            std::stringstream ss;
            ss << "High network usage: " << metrics.network_stats.bandwidth_usage_mbps << " Mbps";
            alerts.push_back(ss.str());
        }
        
        if (metrics.network_stats.average_latency > alert_thresholds_.latency_alert)
        {
            std::stringstream ss;
            ss << "High latency: " << metrics.network_stats.average_latency.count() << " ms";
            alerts.push_back(ss.str());
        }
        
        return alerts;
    }
    
} // namespace GameNetwork::Monitoring