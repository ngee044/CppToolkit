#pragma once

#include <GameNetworkConstants.h>
#include <NetworkPrediction.h>
#include <chrono>
#include <unordered_map>
#include <deque>
#include <memory>
#include <tuple>
#include <optional>
#include <functional>
#include <mutex>
#include <glm/glm.hpp>

namespace GameNetwork
{
    namespace Prediction
    {
        // Hit validation data
        struct HitData
        {
            uint64_t attacker_id;
            uint64_t target_id;
            glm::vec3 hit_position;
            glm::vec3 hit_direction;
            float damage;
            std::chrono::steady_clock::time_point client_timestamp;
            uint32_t client_tick;
        };

        // World state snapshot for rewind
        struct WorldSnapshot
        {
            std::chrono::steady_clock::time_point timestamp;
            uint32_t tick;
            std::unordered_map<uint64_t, EntityState> entities;
            std::unordered_map<uint64_t, glm::mat4> hitboxes; // Transform matrices
        };

        class LagCompensation
        {
        public:
            LagCompensation();
            virtual ~LagCompensation() = default;

            // Configuration
            struct Config
            {
                std::chrono::milliseconds max_rewind_time = std::chrono::milliseconds(1000);
                uint32_t snapshot_buffer_size = 60; // 1 second at 60 Hz
                float position_tolerance = 0.1f;
                bool enable_client_side_prediction = true;
                bool strict_validation = false;
            };
            auto configure(const Config& config) -> void;

            // World state management
            auto save_world_snapshot(const WorldSnapshot& snapshot) -> void;
            auto rewind_world_state(std::chrono::steady_clock::time_point timestamp) 
                -> std::optional<WorldSnapshot>;
            auto rewind_to_tick(uint32_t tick) -> std::optional<WorldSnapshot>;

            // Hit validation
            auto verify_hit_at_timestamp(const HitData& hit_data) 
                -> std::tuple<bool, std::optional<std::string>>;
            
            auto validate_shot(uint64_t shooter_id,
                             const glm::vec3& origin,
                             const glm::vec3& direction,
                             std::chrono::steady_clock::time_point timestamp)
                -> std::optional<uint64_t>; // Returns hit entity ID

            // Server reconciliation
            auto apply_server_reconciliation(const ServerSnapshot& snapshot) -> void;
            auto get_authoritative_state(uint64_t entity_id) const 
                -> std::optional<EntityState>;

            // Lag compensation for abilities
            using AbilityValidator = std::function<bool(const WorldSnapshot&)>;
            auto register_ability_validator(uint32_t ability_id, 
                                          AbilityValidator validator) -> void;
            auto validate_ability_use(uint32_t ability_id,
                                    uint64_t caster_id,
                                    std::chrono::steady_clock::time_point timestamp)
                -> std::tuple<bool, std::optional<std::string>>;

            // Client lag estimation
            auto update_client_lag(uint64_t client_id, 
                                 std::chrono::milliseconds lag) -> void;
            auto get_client_lag(uint64_t client_id) const 
                -> std::chrono::milliseconds;
            // Statistics
            struct LagCompensationStats
            {
                uint64_t hits_validated;
                uint64_t hits_rejected;
                uint64_t rewinds_performed;
                float average_rewind_time_ms;
                uint64_t snapshots_stored;
                uint64_t ability_validations;
            };

            auto get_statistics() const -> LagCompensationStats;

        private:
            // Collision detection helpers
            auto check_ray_entity_intersection(const glm::vec3& origin,
                                              const glm::vec3& direction,
                                              const EntityState& entity,
                                              const glm::mat4& hitbox) const
                -> std::optional<glm::vec3>;

            // Snapshot management
            auto cleanup_old_snapshots() -> void;
            auto find_snapshot_at_time(std::chrono::steady_clock::time_point time) const
                -> std::optional<WorldSnapshot>;
            auto interpolate_snapshots(const WorldSnapshot& before,
                                     const WorldSnapshot& after,
                                     std::chrono::steady_clock::time_point target_time) const
                -> WorldSnapshot;

        private:
            Config config_;
            mutable std::mutex mutex_;

            // Snapshot buffer
            std::deque<WorldSnapshot> snapshot_history_;

            // Client lag tracking
            std::unordered_map<uint64_t, std::chrono::milliseconds> client_lag_;

            // Ability validators
            std::unordered_map<uint32_t, AbilityValidator> ability_validators_;

            // Statistics
            LagCompensationStats stats_;

            // Current authoritative state
            WorldSnapshot current_state_;
        };
    }
}