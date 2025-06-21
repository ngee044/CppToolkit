#include "NetworkMetrics.h"

#include <algorithm>
#include <numeric>
#include <limits>

namespace GameNetwork
{
    NetworkMetrics::NetworkMetrics()
        : current_rtt_(0.0f)
        , average_rtt_(0.0f)
        , packets_sent_(0)
        , packets_received_(0)
        , packets_lost_(0)
        , current_jitter_(0.0f)
    {
        stats_.start_time = std::chrono::steady_clock::now();
        stats_.min_rtt_ms = std::numeric_limits<float>::max();
        stats_.max_rtt_ms = 0.0f;
        stats_.average_rtt_ms = 0.0f;
        stats_.total_packets_sent = 0;
        stats_.total_packets_received = 0;
        stats_.total_packets_lost = 0;
        stats_.total_bytes_sent = 0;
        stats_.total_bytes_received = 0;
        
        last_bandwidth_update_ = std::chrono::steady_clock::now();
    }
    
    auto NetworkMetrics::record_ping_sent(uint32_t sequence) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        pending_pings_[sequence] = PingInfo{
            std::chrono::steady_clock::now(),
            false
        };
    }
    
    auto NetworkMetrics::record_ping_received(uint32_t sequence) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pending_pings_.find(sequence);
        if (it != pending_pings_.end())
        {
            auto now = std::chrono::steady_clock::now();
            auto rtt_duration = now - it->second.sent_time;
            float rtt_ms = std::chrono::duration<float, std::milli>(rtt_duration).count();
            
            // Update current RTT
            current_rtt_ = rtt_ms;
            
            // Add to history
            rtt_history_.push_back(rtt_ms);
            if (rtt_history_.size() > MAX_RTT_HISTORY)
            {
                rtt_history_.pop_front();
            }
            
            // Update average
            if (!rtt_history_.empty())
            {
                average_rtt_ = std::accumulate(rtt_history_.begin(), rtt_history_.end(), 0.0f) 
                               / rtt_history_.size();
            }
            
            // Update stats
            stats_.min_rtt_ms = std::min(stats_.min_rtt_ms, rtt_ms);
            stats_.max_rtt_ms = std::max(stats_.max_rtt_ms, rtt_ms);
            stats_.average_rtt_ms = average_rtt_;
            
            // Calculate jitter
            if (rtt_history_.size() > 1)
            {
                float prev_rtt = rtt_history_[rtt_history_.size() - 2];
                float jitter = std::abs(rtt_ms - prev_rtt);
                jitter_samples_.push_back(jitter);
                
                if (jitter_samples_.size() > MAX_JITTER_SAMPLES)
                {
                    jitter_samples_.pop_front();
                }
                
                current_jitter_ = calculate_jitter();
            }
            
            pending_pings_.erase(it);
        }
    }
    
    auto NetworkMetrics::get_average_rtt() const -> float
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return average_rtt_;
    }
    
    auto NetworkMetrics::get_current_rtt() const -> float
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_rtt_;
    }
    
    auto NetworkMetrics::record_packet_sent() -> void
    {
        packets_sent_++;
        stats_.total_packets_sent++;
    }
    
    auto NetworkMetrics::record_packet_received() -> void
    {
        packets_received_++;
        stats_.total_packets_received++;
    }
    
    auto NetworkMetrics::record_packet_lost() -> void
    {
        packets_lost_++;
        stats_.total_packets_lost++;
    }
    
    auto NetworkMetrics::get_packet_loss_rate() const -> float
    {
        uint64_t total_sent = packets_sent_.load();
        uint64_t total_lost = packets_lost_.load();
        
        if (total_sent == 0)
        {
            return 0.0f;
        }
        
        return static_cast<float>(total_lost) / static_cast<float>(total_sent);
    }
    
    auto NetworkMetrics::record_bytes_sent(uint64_t bytes) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        sent_bandwidth_samples_.push_back({bytes, std::chrono::steady_clock::now()});
        stats_.total_bytes_sent += bytes;
        
        update_bandwidth_samples();
    }
    
    auto NetworkMetrics::record_bytes_received(uint64_t bytes) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        received_bandwidth_samples_.push_back({bytes, std::chrono::steady_clock::now()});
        stats_.total_bytes_received += bytes;
        
        update_bandwidth_samples();
    }
    
    auto NetworkMetrics::update_bandwidth_samples() -> void
    {
        auto now = std::chrono::steady_clock::now();
        auto cutoff_time = now - std::chrono::seconds(1);
        
        // Remove old samples
        auto remove_old = [&cutoff_time](const BandwidthSample& sample)
        {
            return sample.timestamp < cutoff_time;
        };
        
        sent_bandwidth_samples_.erase(
            std::remove_if(sent_bandwidth_samples_.begin(), 
                           sent_bandwidth_samples_.end(), 
                           remove_old),
            sent_bandwidth_samples_.end());
        
        received_bandwidth_samples_.erase(
            std::remove_if(received_bandwidth_samples_.begin(), 
                           received_bandwidth_samples_.end(), 
                           remove_old),
            received_bandwidth_samples_.end());
        
        // Limit sample count
        while (sent_bandwidth_samples_.size() > MAX_BANDWIDTH_SAMPLES)
        {
            sent_bandwidth_samples_.pop_front();
        }
        
        while (received_bandwidth_samples_.size() > MAX_BANDWIDTH_SAMPLES)
        {
            received_bandwidth_samples_.pop_front();
        }
    }
    
    auto NetworkMetrics::calculate_bandwidth(const std::deque<BandwidthSample>& samples) const -> uint64_t
    {
        if (samples.empty())
        {
            return 0;
        }
        
        uint64_t total_bytes = 0;
        for (const auto& sample : samples)
        {
            total_bytes += sample.bytes;
        }
        
        // Calculate time span
        auto time_span = samples.back().timestamp - samples.front().timestamp;
        auto seconds = std::chrono::duration<float>(time_span).count();
        
        if (seconds <= 0.0f)
        {
            return 0;
        }
        
        // Convert to bits per second
        return static_cast<uint64_t>((total_bytes * 8) / seconds);
    }
    
    auto NetworkMetrics::get_bandwidth_usage() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint64_t send_bps = calculate_bandwidth(sent_bandwidth_samples_);
        uint64_t receive_bps = calculate_bandwidth(received_bandwidth_samples_);
        
        return send_bps + receive_bps;
    }
    
    auto NetworkMetrics::get_send_bandwidth() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return calculate_bandwidth(sent_bandwidth_samples_);
    }
    
    auto NetworkMetrics::get_receive_bandwidth() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return calculate_bandwidth(received_bandwidth_samples_);
    }
    
    auto NetworkMetrics::calculate_jitter() -> float
    {
        if (jitter_samples_.empty())
        {
            return 0.0f;
        }
        
        float sum = std::accumulate(jitter_samples_.begin(), jitter_samples_.end(), 0.0f);
        return sum / jitter_samples_.size();
    }
    
    auto NetworkMetrics::get_current_jitter() const -> float
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_jitter_;
    }
    
    auto NetworkMetrics::get_network_quality() const -> NetworkQuality
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        float loss_rate = get_packet_loss_rate();
        
        if (average_rtt_ < 50.0f && loss_rate < 0.001f)
        {
            return NetworkQuality::Excellent;
        }
        else if (average_rtt_ < 100.0f && loss_rate < 0.01f)
        {
            return NetworkQuality::Good;
        }
        else if (average_rtt_ < 200.0f && loss_rate < 0.03f)
        {
            return NetworkQuality::Fair;
        }
        else if (average_rtt_ < 300.0f && loss_rate < 0.05f)
        {
            return NetworkQuality::Poor;
        }
        else
        {
            return NetworkQuality::Unplayable;
        }
    }
    
    auto NetworkMetrics::get_quality_info() const -> NetworkQualityInfo
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        return NetworkQualityInfo{
            average_rtt_,
            get_packet_loss_rate(),
            current_jitter_,
            get_bandwidth_usage(),
            std::chrono::steady_clock::now()
        };
    }
    
    auto NetworkMetrics::get_recommended_send_rate() const -> uint32_t
    {
        NetworkQuality quality = get_network_quality();
        
        switch (quality)
        {
            case NetworkQuality::Excellent:
                return 60;  // 60 updates per second
            case NetworkQuality::Good:
                return 30;  // 30 updates per second
            case NetworkQuality::Fair:
                return 20;  // 20 updates per second
            case NetworkQuality::Poor:
                return 10;  // 10 updates per second
            case NetworkQuality::Unplayable:
            default:
                return 5;   // 5 updates per second
        }
    }
    
    auto NetworkMetrics::get_recommended_update_rate() const -> uint32_t
    {
        return get_recommended_send_rate();
    }
    
    auto NetworkMetrics::should_enable_compression() const -> bool
    {
        NetworkQuality quality = get_network_quality();
        return quality >= NetworkQuality::Fair;
    }
    
    auto NetworkMetrics::should_reduce_quality() const -> bool
    {
        NetworkQuality quality = get_network_quality();
        return quality >= NetworkQuality::Poor;
    }
    
    auto NetworkMetrics::get_stats() const -> MetricsStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    auto NetworkMetrics::reset_stats() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        stats_.total_packets_sent = 0;
        stats_.total_packets_received = 0;
        stats_.total_packets_lost = 0;
        stats_.total_bytes_sent = 0;
        stats_.total_bytes_received = 0;
        stats_.min_rtt_ms = std::numeric_limits<float>::max();
        stats_.max_rtt_ms = 0.0f;
        stats_.average_rtt_ms = 0.0f;
        stats_.start_time = std::chrono::steady_clock::now();
        
        // Reset atomic counters
        packets_sent_ = 0;
        packets_received_ = 0;
        packets_lost_ = 0;
    }
    
    auto NetworkMetrics::get_connection_score() const -> float
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Score based on RTT (40%), packet loss (40%), and jitter (20%)
        float rtt_score = std::max(0.0f, 100.0f - (average_rtt_ / 3.0f));
        float loss_score = std::max(0.0f, 100.0f - (get_packet_loss_rate() * 2000.0f));
        float jitter_score = std::max(0.0f, 100.0f - (current_jitter_ / 2.0f));
        
        return (rtt_score * 0.4f) + (loss_score * 0.4f) + (jitter_score * 0.2f);
    }
}