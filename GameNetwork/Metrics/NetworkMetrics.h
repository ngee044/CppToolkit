#pragma once

#include <chrono>
#include <atomic>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <optional>
#include <tuple>

namespace GameNetwork
{
    struct NetworkQualityInfo
    {
        float rtt_ms;                // Round Trip Time in milliseconds
        float packet_loss_rate;      // 0.0 ~ 1.0
        float jitter_ms;             // Jitter in milliseconds
        uint64_t bandwidth_usage_bps; // Bits per second
        std::chrono::steady_clock::time_point timestamp;
    };
    
    enum class NetworkQuality
    {
        Excellent = 0,  // RTT < 50ms, Loss < 0.1%
        Good = 1,       // RTT < 100ms, Loss < 1%
        Fair = 2,       // RTT < 200ms, Loss < 3%
        Poor = 3,       // RTT < 300ms, Loss < 5%
        Unplayable = 4  // RTT >= 300ms or Loss >= 5%
    };
    
    class NetworkMetrics
    {
    public:
        NetworkMetrics();
        virtual ~NetworkMetrics() = default;
        
        // RTT measurement
        auto record_ping_sent(uint32_t sequence) -> void;
        auto record_ping_received(uint32_t sequence) -> void;
        auto get_average_rtt() const -> float;
        auto get_current_rtt() const -> float;
        
        // Packet loss tracking
        auto record_packet_sent() -> void;
        auto record_packet_received() -> void;
        auto record_packet_lost() -> void;
        auto get_packet_loss_rate() const -> float;
        
        // Bandwidth monitoring
        auto record_bytes_sent(uint64_t bytes) -> void;
        auto record_bytes_received(uint64_t bytes) -> void;
        auto get_bandwidth_usage() const -> uint64_t;
        auto get_send_bandwidth() const -> uint64_t;
        auto get_receive_bandwidth() const -> uint64_t;
        
        // Jitter measurement
        auto calculate_jitter() -> float;
        auto get_current_jitter() const -> float;
        
        // Network quality assessment
        auto get_network_quality() const -> NetworkQuality;
        auto get_quality_info() const -> NetworkQualityInfo;
        
        // Adaptive settings based on quality
        auto get_recommended_send_rate() const -> uint32_t;
        auto get_recommended_update_rate() const -> uint32_t;
        auto should_enable_compression() const -> bool;
        auto should_reduce_quality() const -> bool;
        
        // Statistics
        struct MetricsStats
        {
            uint64_t total_packets_sent;
            uint64_t total_packets_received;
            uint64_t total_packets_lost;
            uint64_t total_bytes_sent;
            uint64_t total_bytes_received;
            float min_rtt_ms;
            float max_rtt_ms;
            float average_rtt_ms;
            std::chrono::steady_clock::time_point start_time;
        };
        
        auto get_stats() const -> MetricsStats;
        auto reset_stats() -> void;
        
        // Connection scoring (0-100)
        auto get_connection_score() const -> float;
        
    private:
        struct PingInfo
        {
            std::chrono::steady_clock::time_point sent_time;
            bool received;
        };
        
        struct BandwidthSample
        {
            uint64_t bytes;
            std::chrono::steady_clock::time_point timestamp;
        };
        
        auto update_bandwidth_samples() -> void;
        auto calculate_bandwidth(const std::deque<BandwidthSample>& samples) const -> uint64_t;
        
    private:
        mutable std::mutex mutex_;
        
        // RTT tracking
        std::unordered_map<uint32_t, PingInfo> pending_pings_;
        std::deque<float> rtt_history_;
        float current_rtt_;
        float average_rtt_;
        
        // Packet loss tracking
        std::atomic<uint64_t> packets_sent_;
        std::atomic<uint64_t> packets_received_;
        std::atomic<uint64_t> packets_lost_;
        
        // Bandwidth tracking
        std::deque<BandwidthSample> sent_bandwidth_samples_;
        std::deque<BandwidthSample> received_bandwidth_samples_;
        std::chrono::steady_clock::time_point last_bandwidth_update_;
        
        // Jitter tracking
        std::deque<float> jitter_samples_;
        float current_jitter_;
        
        // Statistics
        MetricsStats stats_;
        
        // Configuration
        static constexpr size_t MAX_RTT_HISTORY = 100;
        static constexpr size_t MAX_BANDWIDTH_SAMPLES = 60;
        static constexpr size_t MAX_JITTER_SAMPLES = 30;
        static constexpr auto BANDWIDTH_SAMPLE_INTERVAL = std::chrono::seconds(1);
    };
}
