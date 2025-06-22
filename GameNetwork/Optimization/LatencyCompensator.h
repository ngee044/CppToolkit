#pragma once

#include "GameNetworkConstants.h"

#include <memory>
#include <deque>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <tuple>
#include <functional>
#include <any>
#include <mutex>

namespace GameNetwork
{
    struct TimestampedState
    {
        std::chrono::steady_clock::time_point timestamp;
        Location position;
        float rotation;
        std::unordered_map<std::string, std::any> custom_data;
    };
    
    struct InputCommand
    {
        uint32_t sequence;
        std::chrono::steady_clock::time_point client_timestamp;
        std::chrono::steady_clock::time_point server_timestamp;
        std::vector<uint8_t> command_data;
    };
    
    class LatencyCompensator : public std::enable_shared_from_this<LatencyCompensator>
    {
    public:
        LatencyCompensator();
        virtual ~LatencyCompensator() = default;
        
        // Time synchronization
        auto sync_time(const std::string& session_id,
                       std::chrono::steady_clock::time_point client_time,
                       std::chrono::steady_clock::time_point server_time) -> void;
        auto get_time_offset(const std::string& session_id) const 
            -> std::chrono::milliseconds;
        auto convert_to_server_time(const std::string& session_id,
                                    std::chrono::steady_clock::time_point client_time) const 
            -> std::chrono::steady_clock::time_point;
        
        // State interpolation
        auto add_state_snapshot(uint64_t entity_id, 
                                const TimestampedState& state) -> void;
        auto interpolate_state(uint64_t entity_id,
                               std::chrono::steady_clock::time_point target_time) const 
            -> std::optional<TimestampedState>;
        auto extrapolate_state(uint64_t entity_id,
                               std::chrono::steady_clock::time_point target_time) const 
            -> std::optional<TimestampedState>;
        
        // Client-side prediction
        auto enable_prediction(const std::string& session_id, bool enable) -> void;
        auto record_predicted_state(const std::string& session_id,
                                    uint32_t sequence,
                                    const TimestampedState& state) -> void;
        auto confirm_prediction(const std::string& session_id,
                                uint32_t sequence,
                                const TimestampedState& server_state) 
            -> std::tuple<bool, std::optional<TimestampedState>>;
        
        // Lag compensation (for server)
        auto rewind_time(std::chrono::steady_clock::time_point target_time) 
            -> std::unordered_map<uint64_t, TimestampedState>;
        auto restore_time() -> void;
        
        // Input buffering
        auto buffer_input(const std::string& session_id,
                          const InputCommand& command) -> void;
        auto get_buffered_inputs(const std::string& session_id,
                                 std::chrono::steady_clock::time_point up_to_time) 
            -> std::vector<InputCommand>;
        auto acknowledge_input(const std::string& session_id,
                               uint32_t sequence) -> void;
        
        // Smoothing
        auto enable_smoothing(bool enable) -> void;
        auto set_smoothing_factor(float factor) -> void;
        auto smooth_position(const Location& current,
                             const Location& target,
                             float delta_time) const -> Location;
        
        // Jitter buffer
        auto set_jitter_buffer_size(std::chrono::milliseconds size) -> void;
        auto should_delay_packet(std::chrono::steady_clock::time_point packet_time) const -> bool;
        
        // Rollback and replay
        auto enable_rollback(bool enable) -> void;
        auto create_rollback_point(const std::string& session_id) -> uint32_t;
        auto rollback_to_point(const std::string& session_id, uint32_t rollback_id) -> void;
        auto replay_inputs_from(const std::string& session_id,
                                std::chrono::steady_clock::time_point from_time) -> void;
        
        // Configuration
        auto set_max_extrapolation_time(std::chrono::milliseconds max_time) -> void;
        auto set_interpolation_delay(std::chrono::milliseconds delay) -> void;
        auto set_max_prediction_error(float max_error) -> void;
        
        // Statistics
        struct CompensationStats
        {
            uint64_t interpolations_performed;
            uint64_t extrapolations_performed;
            uint64_t predictions_confirmed;
            uint64_t predictions_corrected;
            float average_time_offset_ms;
            float average_prediction_error;
            uint64_t rollbacks_performed;
        };
        
        auto get_stats() const -> CompensationStats;
        auto reset_stats() -> void;
        
    private:
        struct SessionTimeSync
        {
            std::deque<std::chrono::milliseconds> offset_samples;
            std::chrono::milliseconds average_offset;
            std::chrono::steady_clock::time_point last_sync;
        };
        
        struct EntityHistory
        {
            std::deque<TimestampedState> state_history;
            Location velocity;
            float angular_velocity;
        };
        
        struct PredictionData
        {
            bool enabled;
            std::unordered_map<uint32_t, TimestampedState> predicted_states;
            uint32_t last_confirmed_sequence;
        };
        
        struct RollbackPoint
        {
            uint32_t id;
            std::chrono::steady_clock::time_point timestamp;
            std::unordered_map<uint64_t, TimestampedState> entity_states;
            std::vector<InputCommand> inputs_since;
        };
        
        auto calculate_velocity(const TimestampedState& state1,
                                const TimestampedState& state2) const -> Location;
        auto apply_dead_reckoning(const TimestampedState& last_state,
                                  const Location& velocity,
                                  float delta_time) const -> TimestampedState;
        
    private:
        mutable std::mutex mutex_;
        
        // Time synchronization
        std::unordered_map<std::string, SessionTimeSync> time_sync_data_;
        
        // Entity state history
        std::unordered_map<uint64_t, EntityHistory> entity_histories_;
        
        // Prediction data
        std::unordered_map<std::string, PredictionData> prediction_data_;
        
        // Input buffering
        std::unordered_map<std::string, std::deque<InputCommand>> input_buffers_;
        
        // Rollback data
        std::unordered_map<std::string, std::vector<RollbackPoint>> rollback_points_;
        bool rollback_enabled_;
        
        // Configuration
        std::chrono::milliseconds max_extrapolation_time_;
        std::chrono::milliseconds interpolation_delay_;
        std::chrono::milliseconds jitter_buffer_size_;
        float max_prediction_error_;
        float smoothing_factor_;
        bool smoothing_enabled_;
        
        // Statistics
        CompensationStats stats_;
        
        // Constants
        static constexpr size_t MAX_STATE_HISTORY = 120;  // 2 seconds at 60Hz
        static constexpr size_t MAX_TIME_SAMPLES = 10;
        static constexpr auto DEFAULT_INTERPOLATION_DELAY = std::chrono::milliseconds(100);
    };
}
