#pragma once

#include <chrono>
#include <queue>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>
#include <tuple>
#include <optional>

namespace GameNetwork
{
    struct CompensatedEvent
    {
        uint64_t player_id;
        uint32_t event_id;
        std::chrono::steady_clock::time_point client_timestamp;
        std::chrono::steady_clock::time_point server_timestamp;
        std::chrono::milliseconds estimated_latency;
        std::vector<uint8_t> event_data;
        bool is_compensated;
    };
    
    struct PlayerLatencyInfo
    {
        uint64_t player_id;
        std::chrono::milliseconds average_latency;
        std::chrono::milliseconds min_latency;
        std::chrono::milliseconds max_latency;
        std::chrono::milliseconds jitter;
        uint32_t sample_count;
        std::chrono::steady_clock::time_point last_update;
    };
    
    class LatencyCompensator
    {
    public:
        using CompensationCallback = std::function<void(const CompensatedEvent&)>;
        
        LatencyCompensator();
        ~LatencyCompensator();
        
        // Latency tracking
        auto update_player_latency(uint64_t player_id, std::chrono::milliseconds latency) -> void;
        auto get_player_latency(uint64_t player_id) const -> std::optional<PlayerLatencyInfo>;
        auto get_average_latency(uint64_t player_id) const -> std::chrono::milliseconds;
        
        // Event compensation
        auto compensate_event(const CompensatedEvent& event) -> CompensatedEvent;
        auto add_event_for_compensation(uint64_t player_id, 
                                        uint32_t event_id,
                                        const std::vector<uint8_t>& event_data,
                                        std::chrono::steady_clock::time_point client_timestamp) -> void;
        
        // Time synchronization
        auto synchronize_time(uint64_t player_id, 
                              std::chrono::steady_clock::time_point client_time,
                              std::chrono::steady_clock::time_point server_time) -> void;
        auto get_compensated_time(uint64_t player_id, 
                                  std::chrono::steady_clock::time_point client_time) const 
            -> std::chrono::steady_clock::time_point;
        
        // Configuration
        auto set_max_compensation_window(std::chrono::milliseconds window) -> void;
        auto set_interpolation_delay(std::chrono::milliseconds delay) -> void;
        auto enable_jitter_smoothing(bool enable) -> void;
        
        // Callbacks
        auto on_event_compensated(CompensationCallback callback) -> void;
        
        // Statistics
        auto get_compensation_stats() const -> std::tuple<uint64_t, uint64_t, std::chrono::milliseconds>;
        auto reset_stats() -> void;
        
    private:
        mutable std::mutex mutex_;
        
        // Player latency tracking
        std::unordered_map<uint64_t, PlayerLatencyInfo> player_latencies_;
        std::unordered_map<uint64_t, std::queue<std::chrono::milliseconds>> latency_samples_;
        static constexpr size_t MAX_LATENCY_SAMPLES = 20;
        
        // Time offset tracking
        std::unordered_map<uint64_t, std::chrono::milliseconds> time_offsets_;
        
        // Event queues
        std::unordered_map<uint64_t, std::queue<CompensatedEvent>> pending_events_;
        
        // Configuration
        std::chrono::milliseconds max_compensation_window_{1000};
        std::chrono::milliseconds interpolation_delay_{100};
        bool jitter_smoothing_enabled_{true};
        
        // Callbacks
        std::vector<CompensationCallback> compensation_callbacks_;
        
        // Statistics
        uint64_t events_compensated_{0};
        uint64_t events_rejected_{0};
        std::chrono::milliseconds total_compensation_time_{0};
        
        // Helper functions
        auto calculate_jitter(const std::queue<std::chrono::milliseconds>& samples) const 
            -> std::chrono::milliseconds;
        auto smooth_latency(uint64_t player_id, std::chrono::milliseconds raw_latency) const 
            -> std::chrono::milliseconds;
        auto is_within_compensation_window(std::chrono::steady_clock::time_point event_time) const -> bool;
    };
} // namespace GameNetwork
