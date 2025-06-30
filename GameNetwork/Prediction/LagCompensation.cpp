#include "LagCompensation.h"

#include <Logger.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/intersect.hpp>

#include <algorithm>

using namespace Utilities;

namespace GameNetwork
{
    namespace Prediction
    {
        LagCompensation::LagCompensation()
            : stats_{0, 0, 0, 0.0f, 0, 0}
        {
            configure(Config{});
        }

        auto LagCompensation::configure(const Config& config) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            config_ = config;
        }

        auto LagCompensation::save_world_snapshot(const WorldSnapshot& snapshot) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            snapshot_history_.push_back(snapshot);
            stats_.snapshots_stored++;
            
            // Keep buffer size limited
            while (snapshot_history_.size() > config_.snapshot_buffer_size)
            {
                snapshot_history_.pop_front();
            }
            
            // Update current state
            current_state_ = snapshot;
        }

        auto LagCompensation::rewind_world_state(std::chrono::steady_clock::time_point timestamp)
            -> std::optional<WorldSnapshot>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Check if timestamp is too old
            if (!snapshot_history_.empty())
            {
                auto oldest = snapshot_history_.front().timestamp;
                if (timestamp < oldest)
                {
                    Logger::handle().write(LogTypes::Error,
                        "Requested timestamp is older than available history");
                    return std::nullopt;
                }
            }
            
            auto snapshot = find_snapshot_at_time(timestamp);
            if (snapshot.has_value())
            {
                stats_.rewinds_performed++;
                
                // Calculate rewind time
                auto rewind_duration = std::chrono::steady_clock::now() - timestamp;
                float rewind_ms = std::chrono::duration<float, std::milli>(rewind_duration).count();
                stats_.average_rewind_time_ms = 
                    (stats_.average_rewind_time_ms * (stats_.rewinds_performed - 1) + rewind_ms) / 
                    stats_.rewinds_performed;
            }
            
            return snapshot;
        }

        auto LagCompensation::rewind_to_tick(uint32_t tick) -> std::optional<WorldSnapshot>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = std::find_if(snapshot_history_.begin(), snapshot_history_.end(),
                [tick](const WorldSnapshot& snapshot) {
                    return snapshot.tick == tick;
                });

            if (it != snapshot_history_.end())
            {
                stats_.rewinds_performed++;
                return *it;
            }
            
            return std::nullopt;
        }

        auto LagCompensation::verify_hit_at_timestamp(const HitData& hit_data)
            -> std::tuple<bool, std::optional<std::string>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Get client lag
            auto lag = get_client_lag(hit_data.attacker_id);
            
            // Calculate server timestamp when the hit occurred on client
            auto server_timestamp = hit_data.client_timestamp + lag;
            
            // Check if rewind time is within limits
            auto now = std::chrono::steady_clock::now();
            auto rewind_duration = now - server_timestamp;
            if (rewind_duration > config_.max_rewind_time)
            {
                stats_.hits_rejected++;
                return { false, "Rewind time exceeds maximum allowed" };
            }
            
            // Rewind to the appropriate time
            auto snapshot_opt = rewind_world_state(server_timestamp);
            if (!snapshot_opt.has_value())
            {
                stats_.hits_rejected++;
                return { false, "No snapshot available for the requested time" };
            }
            
            const auto& snapshot = *snapshot_opt;

            // Find attacker and target in snapshot
            auto attacker_it = snapshot.entities.find(hit_data.attacker_id);
            auto target_it = snapshot.entities.find(hit_data.target_id);
            
            if (attacker_it == snapshot.entities.end())
            {
                stats_.hits_rejected++;
                return { false, "Attacker not found in snapshot" };
            }
            
            if (target_it == snapshot.entities.end())
            {
                stats_.hits_rejected++;
                return { false, "Target not found in snapshot" };
            }
            
            // Get target hitbox
            auto hitbox_it = snapshot.hitboxes.find(hit_data.target_id);
            if (hitbox_it == snapshot.hitboxes.end())
            {
                stats_.hits_rejected++;
                return { false, "Target hitbox not found" };
            }
            
            // Verify the hit
            auto hit_result = check_ray_entity_intersection(
                hit_data.hit_position,
                hit_data.hit_direction,
                target_it->second,
                hitbox_it->second
            );
            
            if (hit_result.has_value())
            {
                // Additional validation if strict mode is enabled
                if (config_.strict_validation)
                {
                    // Check if hit position is close to calculated intersection
                    float distance = glm::length(hit_result.value() - hit_data.hit_position);
                    if (distance > config_.position_tolerance)
                    {
                        stats_.hits_rejected++;
                        return { false, "Hit position mismatch" };
                    }
                }

                stats_.hits_validated++;
                return { true, std::nullopt };
            }
            
            stats_.hits_rejected++;
            return { false, "Ray does not intersect target" };
        }

        auto LagCompensation::validate_shot(uint64_t shooter_id,
                                            const glm::vec3& origin,
                                            const glm::vec3& direction,
                                            std::chrono::steady_clock::time_point timestamp)
            -> std::optional<uint64_t>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Get snapshot at timestamp
            auto snapshot_opt = rewind_world_state(timestamp);
            if (!snapshot_opt.has_value())
            {
                return std::nullopt;
            }
            
            const auto& snapshot = *snapshot_opt;
            
            // Check intersection with all entities except shooter
            float closest_distance = std::numeric_limits<float>::max();
            uint64_t hit_entity_id = 0;
            
            for (const auto& [entity_id, entity_state] : snapshot.entities)
            {
                if (entity_id == shooter_id)
                {
                    continue; // Skip self
                }
                
                auto hitbox_it = snapshot.hitboxes.find(entity_id);
                if (hitbox_it == snapshot.hitboxes.end())
                {
                    continue;
                }

                auto hit_point = check_ray_entity_intersection(origin, direction, entity_state, hitbox_it->second);
                if (hit_point.has_value())
                {
                    float distance = glm::length(hit_point.value() - origin);
                    if (distance < closest_distance)
                    {
                        closest_distance = distance;
                        hit_entity_id = entity_id;
                    }
                }
            }
            
            return (hit_entity_id != 0) ? std::optional<uint64_t>(hit_entity_id) : std::nullopt;
        }

        auto LagCompensation::apply_server_reconciliation(const ServerSnapshot& snapshot) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Update current state with server snapshot
            current_state_.timestamp = snapshot.timestamp;
            current_state_.tick = snapshot.tick;
            current_state_.entities = snapshot.entities;
            
            // Clear old snapshots that are no longer needed
            cleanup_old_snapshots();
        }

        auto LagCompensation::get_authoritative_state(uint64_t entity_id) const
            -> std::optional<EntityState>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = current_state_.entities.find(entity_id);
            if (it != current_state_.entities.end())
            {
                return it->second;
            }
            
            return std::nullopt;
        }

        auto LagCompensation::register_ability_validator(uint32_t ability_id, AbilityValidator validator) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ability_validators_[ability_id] = validator;
        }

        auto LagCompensation::validate_ability_use(uint32_t ability_id, uint64_t caster_id, std::chrono::steady_clock::time_point timestamp)
            -> std::tuple<bool, std::optional<std::string>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            stats_.ability_validations++;
            
            // Find validator for ability
            auto validator_it = ability_validators_.find(ability_id);
            if (validator_it == ability_validators_.end())
            {
                return { false, "No validator registered for ability" };
            }
            
            // Get client lag
            auto lag = get_client_lag(caster_id);
            auto server_timestamp = timestamp + lag;
            
            // Rewind to appropriate time
            auto snapshot_opt = rewind_world_state(server_timestamp);
            if (!snapshot_opt.has_value())
            {
                return { false, "No snapshot available for validation" };
            }
            
            // Run custom validator
            bool valid = validator_it->second(*snapshot_opt);
            
            if (!valid)
            {
                return { false, "Ability validation failed" };
            }
            
            return { true, std::nullopt };
        }

        auto LagCompensation::update_client_lag(uint64_t client_id, std::chrono::milliseconds lag) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            client_lag_[client_id] = lag;
        }

        auto LagCompensation::get_client_lag(uint64_t client_id) const
            -> std::chrono::milliseconds
        {
            auto it = client_lag_.find(client_id);
            if (it != client_lag_.end())
            {
                return it->second;
            }
            
            // Return default lag if not found
            return std::chrono::milliseconds(100);
        }

        auto LagCompensation::get_statistics() const -> LagCompensationStats
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return stats_;
        }

        auto LagCompensation::check_ray_entity_intersection(const glm::vec3& origin, const glm::vec3& direction, const EntityState& entity, const glm::mat4& hitbox) const
            -> std::optional<glm::vec3>
        {
            // Simple AABB intersection test
            // In production, use more sophisticated collision detection
            
            // Transform ray to entity's local space
            glm::mat4 inverse_hitbox = glm::inverse(hitbox);
            glm::vec3 local_origin = glm::vec3(inverse_hitbox * glm::vec4(origin, 1.0f));
            glm::vec3 local_direction = glm::normalize(glm::vec3(inverse_hitbox * glm::vec4(direction, 0.0f)));

            // Simple AABB test (assuming unit cube centered at origin in local space)
            glm::vec3 box_min(-0.5f, -0.5f, -0.5f);
            glm::vec3 box_max(0.5f, 0.5f, 0.5f);
            
            float t_min = 0.0f;
            float t_max = std::numeric_limits<float>::max();
            
            for (int i = 0; i < 3; ++i)
            {
                if (std::abs(local_direction[i]) < 0.0001f)
                {
                    // Ray is parallel to slab
                    if (local_origin[i] < box_min[i] || local_origin[i] > box_max[i])
                    {
                        return std::nullopt;
                    }
                }
                else
                {
                    float t1 = (box_min[i] - local_origin[i]) / local_direction[i];
                    float t2 = (box_max[i] - local_origin[i]) / local_direction[i];
                    
                    if (t1 > t2) std::swap(t1, t2);
                    
                    t_min = std::max(t_min, t1);
                    t_max = std::min(t_max, t2);
                    
                    if (t_min > t_max)
                    {
                        return std::nullopt;
                    }
                }
            }
            
            // Transform hit point back to world space
            glm::vec3 local_hit = local_origin + local_direction * t_min;
            glm::vec3 world_hit = glm::vec3(hitbox * glm::vec4(local_hit, 1.0f));
            
            return world_hit;
        }

        auto LagCompensation::cleanup_old_snapshots() -> void
        {
            auto now = std::chrono::steady_clock::now();
            
            snapshot_history_.erase(
                std::remove_if(snapshot_history_.begin(), snapshot_history_.end(),
                    [this, now](const WorldSnapshot& snapshot) {
                        auto age = now - snapshot.timestamp;
                        return age > config_.max_rewind_time;
                    }),
                snapshot_history_.end()
            );
        }

        auto LagCompensation::find_snapshot_at_time(std::chrono::steady_clock::time_point time) const
            -> std::optional<WorldSnapshot>
        {
            if (snapshot_history_.empty())
            {
                return std::nullopt;
            }
            
            // Find closest snapshots
            auto it = std::lower_bound(snapshot_history_.begin(), snapshot_history_.end(), time,
                [](const WorldSnapshot& snapshot, std::chrono::steady_clock::time_point t) {
                    return snapshot.timestamp < t;
                });
            
            if (it == snapshot_history_.end())
            {
                // Time is after all snapshots, return the last one
                return snapshot_history_.back();
            }
            
            if (it->timestamp == time)
            {
                // Exact match
                return *it;
            }
            
            if (it == snapshot_history_.begin())
            {
                // Time is before all snapshots
                return *it;
            }

            // Interpolate between two snapshots
            auto prev_it = std::prev(it);
            return interpolate_snapshots(*prev_it, *it, time);
        }

        auto LagCompensation::interpolate_snapshots(const WorldSnapshot& before,
                                                    const WorldSnapshot& after,
                                                    std::chrono::steady_clock::time_point target_time) const -> WorldSnapshot
        {
            WorldSnapshot result;
            result.timestamp = target_time;
            
            // Calculate interpolation factor
            auto total_duration = after.timestamp - before.timestamp;
            auto elapsed = target_time - before.timestamp;
            float alpha = std::chrono::duration<float>(elapsed).count() / std::chrono::duration<float>(total_duration).count();
            alpha = std::clamp(alpha, 0.0f, 1.0f);
            
            // Interpolate tick
            result.tick = static_cast<uint32_t>(
                std::round(before.tick * (1.0f - alpha) + after.tick * alpha));
            
            // Interpolate entities
            for (const auto& [entity_id, before_state] : before.entities)
            {
                auto after_it = after.entities.find(entity_id);
                if (after_it != after.entities.end())
                {
                    EntityState interpolated;
                    interpolated.entity_id = entity_id;
                    interpolated.position = glm::mix(before_state.position, after_it->second.position, alpha);
                    interpolated.velocity = glm::mix(before_state.velocity, after_it->second.velocity, alpha);
                    interpolated.acceleration = glm::mix(before_state.acceleration, after_it->second.acceleration, alpha);
                    interpolated.rotation = glm::slerp(before_state.rotation, after_it->second.rotation, alpha);
                    interpolated.angular_velocity = glm::mix(before_state.angular_velocity, after_it->second.angular_velocity, alpha);
                    interpolated.timestamp = target_time;
                    interpolated.sequence_number = after_it->second.sequence_number;
                    
                    result.entities[entity_id] = interpolated;
                }
            }

            // Interpolate hitboxes
            for (const auto& [entity_id, before_hitbox] : before.hitboxes)
            {
                auto after_it = after.hitboxes.find(entity_id);
                if (after_it != after.hitboxes.end())
                {
                    // Simple linear interpolation of transformation matrices
                    // In production, use proper transformation interpolation
                    glm::mat4 interpolated;
                    for (int i = 0; i < 4; ++i)
                    {
                        for (int j = 0; j < 4; ++j)
                        {
                            interpolated[i][j] = glm::mix(before_hitbox[i][j], 
                                                        after_it->second[i][j], 
                                                        alpha);
                        }
                    }
                    result.hitboxes[entity_id] = interpolated;
                }
            }
            
            return result;
        }
    }
}
