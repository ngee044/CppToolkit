#pragma once

#include <GameNetworkConstants.h>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <memory>
#include <tuple>
#include <optional>
#include <mutex>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace GameNetwork
{
    namespace Prediction
    {
        // Entity state for prediction
        struct EntityState
        {
            uint64_t entity_id;
            glm::vec3 position;
            glm::vec3 velocity;
            glm::vec3 acceleration;
            glm::quat rotation;
            glm::vec3 angular_velocity;
            std::chrono::steady_clock::time_point timestamp;
            uint32_t sequence_number;
        };

        // Server snapshot
        struct ServerSnapshot
        {
            uint32_t tick;
            std::chrono::steady_clock::time_point timestamp;
            std::unordered_map<uint64_t, EntityState> entities;
            uint32_t acknowledged_input_sequence;
        };

        // Client input
        struct ClientInput
        {
            uint32_t sequence;
            std::chrono::steady_clock::time_point timestamp;
            glm::vec3 movement_input;
            glm::vec2 look_input;
            uint32_t action_flags;
        };
        class NetworkPrediction
        {
        public:
            NetworkPrediction();
            virtual ~NetworkPrediction() = default;

            // Configuration
            struct Config
            {
                uint32_t max_prediction_frames = 10;
                float position_error_threshold = 0.1f;
                float rotation_error_threshold = 5.0f; // degrees
                uint32_t input_buffer_size = 120;
                bool enable_smoothing = true;
                float smoothing_rate = 10.0f;
            };

            auto configure(const Config& config) -> void;

            // Prediction
            auto predict_movement(const EntityState& entity, float delta_time) 
                -> EntityState;
            
            auto predict_entity_state(uint64_t entity_id, 
                                    std::chrono::steady_clock::time_point target_time)
                -> std::optional<EntityState>;

            // Server reconciliation
            auto rollback_on_server_correction(const ServerSnapshot& snapshot) -> void;
            auto apply_server_state(const EntityState& server_state) -> void;
            
            // Interpolation
            auto interpolate_positions(const EntityState& from, 
                                     const EntityState& to,
                                     float alpha) -> EntityState;
            
            auto get_interpolated_state(uint64_t entity_id,
                                      std::chrono::steady_clock::time_point render_time)
                -> std::optional<EntityState>;

            // Input handling
            auto add_local_input(const ClientInput& input) -> void;
            auto acknowledge_input(uint32_t sequence) -> void;
            auto reapply_unacknowledged_inputs() -> void;
            // State management
            auto update_entity_state(const EntityState& state) -> void;
            auto remove_entity(uint64_t entity_id) -> void;
            
            // Statistics
            struct PredictionStats
            {
                uint64_t predictions_made;
                uint64_t rollbacks_performed;
                float average_prediction_error;
                uint32_t max_prediction_frames_used;
                uint64_t interpolations_performed;
            };

            auto get_statistics() const -> PredictionStats;

        private:
            // Prediction helpers
            auto integrate_physics(const EntityState& state, float delta_time) 
                -> EntityState;
            auto calculate_prediction_error(const EntityState& predicted,
                                          const EntityState& actual) -> float;
            auto should_smooth_correction(float error) const -> bool;

            // State buffers
            using StateHistory = std::deque<EntityState>;
            auto get_state_at_time(const StateHistory& history,
                                 std::chrono::steady_clock::time_point time)
                -> std::optional<EntityState>;

        private:
            Config config_;
            mutable std::mutex mutex_;

            // Entity states
            std::unordered_map<uint64_t, EntityState> current_states_;
            std::unordered_map<uint64_t, StateHistory> state_history_;
            std::unordered_map<uint64_t, EntityState> predicted_states_;

            // Input buffer
            std::deque<ClientInput> unacknowledged_inputs_;
            uint32_t last_acknowledged_input_;

            // Statistics
            PredictionStats stats_;
        };
    }
}