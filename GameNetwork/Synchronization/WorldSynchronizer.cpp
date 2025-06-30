#include "WorldSynchronizer.h"
#include "GameSessionManager.h"
#include "PacketProcessor.h"

#include <Generator.h>
#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>
#include <cmath>

using namespace Utilities;

namespace GameNetwork
{
	WorldSynchronizer::WorldSynchronizer()
		: lag_compensation_enabled_(true)
		, snapshot_history_duration_(std::chrono::milliseconds(1000))
		, sync_frequency_hz_(30)
		, max_entities_per_update_(50)
		, aoi_radius_(100.0f)
		, prediction_enabled_(true)
		, is_running_(false)
	{
		// Set default world bounds
		world_min_ = { glm::vec3(-1000.0f, -1000.0f, -1000.0f) };
		world_max_ = { glm::vec3(1000.0f, 1000.0f, 1000.0f) };
        
		// Initialize stats
		stats_ = {};
		last_sync_time_ = std::chrono::steady_clock::now();
	}

	WorldSynchronizer::~WorldSynchronizer()
	{
		shutdown();
	}

	auto WorldSynchronizer::initialize(std::shared_ptr<GameSessionManager> session_manager) -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (is_running_)
		{
			return false;
		}
        
		session_manager_ = session_manager;
        
		Logger::handle().write(LogTypes::Information, "WorldSynchronizer initialized successfully");
        
		return true;
	}

	auto WorldSynchronizer::shutdown() -> void
	{
		stop_sync_loop();
        
		std::lock_guard<std::mutex> lock(mutex_);
		entity_states_.clear();
        
		std::lock_guard<std::mutex> snapshot_lock(snapshot_mutex_);
		snapshots_.clear();
        
		Logger::handle().write(LogTypes::Information, "WorldSynchronizer shutdown completed");
	}

	auto WorldSynchronizer::set_thread_pool(std::shared_ptr<Thread::ThreadPool> thread_pool) -> void
	{
		thread_pool_ = thread_pool;
	}

	// Entity synchronization
	auto WorldSynchronizer::register_entity(uint64_t entity_id, const WorldEntityState& initial_state) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		entity_states_[entity_id] = initial_state;
		entity_states_[entity_id].last_update_time = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
        
		stats_.active_entities = static_cast<uint32_t>(entity_states_.size());

		Logger::handle().write(LogTypes::Information, fmt::format("Registered entity {} for synchronization", entity_id));
	}

	auto WorldSynchronizer::unregister_entity(uint64_t entity_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		entity_states_.erase(entity_id);
        
		stats_.active_entities = static_cast<uint32_t>(entity_states_.size());

		Logger::handle().write(LogTypes::Information, fmt::format("Unregistered entity {} from synchronization", entity_id));
	}

	auto WorldSynchronizer::update_entity_state(uint64_t entity_id, const WorldEntityState& state) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = entity_states_.find(entity_id);
		if (it != entity_states_.end())
		{
			// Validate the position
			Location validated_position = state.position;
			clamp_to_world_bounds(validated_position);
            
			// Update the state
			WorldEntityState updated_state = state;
			updated_state.position = validated_position;
			updated_state.last_update_time = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
			updated_state.is_dirty = true;
            
			it->second = updated_state;
            
			// Add to sync queue
			SyncUpdate update;
			update.entity_id = entity_id;
			update.state = updated_state;
			update.created_time = std::chrono::steady_clock::now();
			update.priority = calculate_sync_priority(updated_state);
            
			sync_queue_.push(update);
			stats_.total_updates_processed++;
		}
	}

	auto WorldSynchronizer::get_entity_state(uint64_t entity_id) const -> std::optional<WorldEntityState>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = entity_states_.find(entity_id);
		if (it != entity_states_.end())
		{
			return it->second;
		}
        
		return std::nullopt;
	}

	// Real-time synchronization
	auto WorldSynchronizer::start_sync_loop() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (is_running_)
		{
			return;
		}
        
		is_running_ = true;
		sync_thread_ = std::thread(&WorldSynchronizer::sync_loop, this);
        
		Logger::handle().write(LogTypes::Information, fmt::format("WorldSynchronizer sync loop started at {} Hz", sync_frequency_hz_));
	}

	auto WorldSynchronizer::stop_sync_loop() -> void
	{
		{
			std::lock_guard<std::mutex> lock(mutex_);
			is_running_ = false;
		}
        
		sync_cv_.notify_all();
        
		if (sync_thread_.joinable())
		{
			sync_thread_.join();
		}
        
		Logger::handle().write(LogTypes::Information, "WorldSynchronizer sync loop stopped");
	}

	auto WorldSynchronizer::force_sync() -> void
	{
		sync_cv_.notify_all();
	}

	auto WorldSynchronizer::sync_entity_to_clients(uint64_t entity_id, const std::vector<std::shared_ptr<GameSession>>& clients) -> void
	{
		auto entity_state = get_entity_state(entity_id);
		if (!entity_state.has_value())
		{
			return;
		}
        
		// Create entity update packet
		auto update_packet = std::make_unique<EntityUpdatePacket>();
		update_packet->set_entity_id(entity_id);
		update_packet->set_position(entity_state->position);
		update_packet->set_health(entity_state->health);
		update_packet->set_rotation(entity_state->rotation);
		update_packet->set_is_moving(entity_state->is_moving);
        
		// Send to all specified clients
		for (auto& client : clients)
		{
			if (client && client->is_connected())
			{
				// Check if client should receive this update (AOI check)
				if (should_sync_to_client(entity_id, client))
				{
					auto serialized_data = update_packet->serialize();
					client->send_packet(serialized_data);
				}
			}
		}
        
		stats_.total_entities_synced++;
	}

	auto WorldSynchronizer::sync_area_to_client(const Location& center, float radius, std::shared_ptr<GameSession> client) -> void
	{
		if (!client || !client->is_connected())
		{
			return;
		}
        
		auto entities_in_range = get_entities_in_range(center, radius);
        
		for (uint64_t entity_id : entities_in_range)
		{
			sync_entity_to_clients(entity_id, { client });
		}
	}

	// Lag compensation
	auto WorldSynchronizer::enable_lag_compensation(bool enable) -> void
	{
		lag_compensation_enabled_ = enable;
        
		Logger::handle().write(LogTypes::Information, fmt::format("Lag compensation {}",
			enable ? "enabled" : "disabled"));
	}

	auto WorldSynchronizer::set_snapshot_history_duration(std::chrono::milliseconds duration) -> void
	{
		snapshot_history_duration_ = duration;
	}

	auto WorldSynchronizer::get_entity_position_at_time(uint64_t entity_id, uint64_t timestamp) const -> std::optional<Location>
	{
		if (!lag_compensation_enabled_)
		{
			return std::nullopt;
		}
        
		auto snapshot = find_snapshot_at_time(timestamp);
		if (!snapshot.has_value())
		{
			return std::nullopt;
		}
        
		auto it = snapshot->entity_positions.find(entity_id);
		if (it != snapshot->entity_positions.end())
		{
			return it->second;
		}
        
		return std::nullopt;
	}

	auto WorldSynchronizer::validate_movement(uint64_t entity_id, const Location& from, const Location& to, uint64_t client_timestamp) const -> bool
	{
		// Basic validation
		if (!is_position_valid(to))
		{
			return false;
		}
        
		// Distance validation - prevent teleporting
		float distance = calculate_distance(from, to);
		const float max_distance_per_second = 50.0f; // Configurable max speed
        
		auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
        
		auto entity_state = get_entity_state(entity_id);
		if (entity_state.has_value())
		{
			uint64_t time_delta = current_time - entity_state->last_update_time;
			float max_allowed_distance = max_distance_per_second * (time_delta / 1000.0f);
            
			if (distance > max_allowed_distance)
			{
				Logger::handle().write(LogTypes::Error, fmt::format("Movement validation failed for entity {}: distance {} exceeds max {}", entity_id, distance, max_allowed_distance));
				return false;
			}
		}
        
		return true;
	}

	auto WorldSynchronizer::compensate_for_lag(uint64_t entity_id, const Location& client_position, uint64_t client_timestamp) -> Location
	{
		if (!lag_compensation_enabled_)
		{
			return client_position;
		}
        
		// Get the server's authoritative position at the time the client made this action
		auto server_position_at_time = get_entity_position_at_time(entity_id, client_timestamp);
		if (!server_position_at_time.has_value())
		{
			// Fallback to current position
			auto current_state = get_entity_state(entity_id);
			return current_state.has_value() ? current_state->position : client_position;
		}
        
		// Calculate the difference and apply lag compensation
		Location compensated_position = client_position;
        
		// Apply some interpolation between client and server positions
		const float lag_compensation_factor = 0.7f; // How much to trust client vs server
		compensated_position.x = client_position.x * lag_compensation_factor + 
								server_position_at_time->x * (1.0f - lag_compensation_factor);
		compensated_position.y = client_position.y * lag_compensation_factor + 
								server_position_at_time->y * (1.0f - lag_compensation_factor);
		compensated_position.z = client_position.z * lag_compensation_factor + 
								server_position_at_time->z * (1.0f - lag_compensation_factor);
        
		stats_.lag_compensation_queries++;
        
		return compensated_position;
	}

	// Area of Interest (AOI)
	auto WorldSynchronizer::set_aoi_radius(float radius) -> void
	{
		aoi_radius_ = radius;
	}

	auto WorldSynchronizer::get_entities_in_range(const Location& center, float radius) const -> std::vector<uint64_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<uint64_t> result;
        
		for (const auto& [entity_id, state] : entity_states_)
		{
			float distance = calculate_distance(center, state.position);
			if (distance <= radius)
			{
				result.push_back(entity_id);
			}
		}
        
		return result;
	}

	auto WorldSynchronizer::get_clients_in_range(const Location& center, float radius) const -> std::vector<std::shared_ptr<GameSession>>
	{
		std::vector<std::shared_ptr<GameSession>> result;
        
		if (!session_manager_)
		{
			return result;
		}
        
		auto all_sessions = session_manager_->get_all_sessions();
        
		for (const auto& [session_id, session] : all_sessions)
		{
			if (session && session->is_connected())
			{
				// Get client's entity position (assuming client has an entity)
				auto client_entity_id = session->get_entity_id();
				if (client_entity_id == 0) continue; // Skip if no entity
                
				auto client_state = get_entity_state(client_entity_id);
                
				if (client_state.has_value())
				{
					float distance = calculate_distance(center, client_state->position);
					if (distance <= radius)
					{
						result.push_back(session);
					}
				}
			}
		}
        
		return result;
	}

	// Conflict resolution
	auto WorldSynchronizer::resolve_movement_conflict(uint64_t entity_id, const std::vector<WorldEntityState>& conflicting_states) -> WorldEntityState
	{
		if (conflicting_states.empty())
		{
			// Return current state if available
			auto current_state = get_entity_state(entity_id);
			return current_state.value_or(WorldEntityState{});
		}
        
		if (conflicting_states.size() == 1)
		{
			return conflicting_states[0];
		}
        
		// Use the most recent state based on timestamp
		WorldEntityState resolved_state = conflicting_states[0];
        
		for (const auto& state : conflicting_states)
		{
			if (state.last_update_time > resolved_state.last_update_time)
			{
				resolved_state = state;
			}
		}
        
		stats_.conflict_resolutions++;

		Logger::handle().write(LogTypes::Information, fmt::format("Resolved movement conflict for entity {}", entity_id));

		return resolved_state;
	}

	auto WorldSynchronizer::is_position_valid(const Location& position) const -> bool
	{
		return position.x >= world_min_.x && position.x <= world_max_.x &&
				position.y >= world_min_.y && position.y <= world_max_.y &&
				position.z >= world_min_.z && position.z <= world_max_.z;
	}

	auto WorldSynchronizer::clamp_to_world_bounds(Location& position) const -> void
	{
		position.x = std::clamp(position.x, world_min_.x, world_max_.x);
		position.y = std::clamp(position.y, world_min_.y, world_max_.y);
		position.z = std::clamp(position.z, world_min_.z, world_max_.z);
	}

	// Performance monitoring
	auto WorldSynchronizer::get_stats() const -> SyncStats
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return stats_;
	}

	auto WorldSynchronizer::reset_stats() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stats_ = {};
		stats_.active_entities = static_cast<uint32_t>(entity_states_.size());
        
		std::lock_guard<std::mutex> snapshot_lock(snapshot_mutex_);
		stats_.snapshots_stored = static_cast<uint32_t>(snapshots_.size());
	}

	// Configuration
	auto WorldSynchronizer::set_sync_frequency(uint32_t frequency_hz) -> void
	{
		sync_frequency_hz_ = frequency_hz;
	}

	auto WorldSynchronizer::set_max_entities_per_update(uint32_t max_entities) -> void
	{
		max_entities_per_update_ = max_entities;
	}

	auto WorldSynchronizer::set_prediction_enabled(bool enabled) -> void
	{
		prediction_enabled_ = enabled;
	}

	// Private implementation methods
	auto WorldSynchronizer::sync_loop() -> void
	{
		auto sync_interval = std::chrono::milliseconds(1000 / sync_frequency_hz_);
        
		while (is_running_)
		{
			auto start_time = std::chrono::steady_clock::now();
            
			// Process synchronization updates
			process_sync_updates();
            
			// Update snapshots for lag compensation
			if (lag_compensation_enabled_)
			{
				update_entity_snapshots();
				cleanup_old_snapshots();
			}
            
			auto end_time = std::chrono::steady_clock::now();
			auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
			// Update statistics
			{
				std::lock_guard<std::mutex> lock(mutex_);
				stats_.average_sync_time_ms = processing_time.count() / 1000.0;
			}
            
			// Wait for next sync cycle
			std::unique_lock<std::mutex> lock(mutex_);
			sync_cv_.wait_for(lock, sync_interval, [this] { return !is_running_; });
		}
	}

	auto WorldSynchronizer::process_sync_updates() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		uint32_t processed_count = 0;
        
		while (!sync_queue_.empty() && processed_count < max_entities_per_update_)
		{
			SyncUpdate update = sync_queue_.front();
			sync_queue_.pop();
            
			// Get nearby clients
			auto nearby_clients = get_nearby_clients(update.entity_id);
            
			// Sync to clients
			if (!nearby_clients.empty())
			{
				sync_entity_to_clients(update.entity_id, nearby_clients);
			}
            
			// Mark entity as clean
			auto it = entity_states_.find(update.entity_id);
			if (it != entity_states_.end())
			{
				it->second.is_dirty = false;
			}
            
			processed_count++;
		}
	}

	auto WorldSynchronizer::update_entity_snapshots() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		std::lock_guard<std::mutex> snapshot_lock(snapshot_mutex_);
        
		LagCompensationSnapshot snapshot;
		snapshot.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
        
		// Capture current entity states
		for (const auto& [entity_id, state] : entity_states_)
		{
			snapshot.entity_positions[entity_id] = state.position;
			snapshot.entity_states[entity_id] = state;
		}
        
		snapshots_.push_back(snapshot);
		stats_.snapshots_stored = static_cast<uint32_t>(snapshots_.size());
	}

	auto WorldSynchronizer::cleanup_old_snapshots() -> void
	{
		std::lock_guard<std::mutex> snapshot_lock(snapshot_mutex_);
        
		auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
        
		auto cutoff_time = current_time - snapshot_history_duration_.count();
        
		snapshots_.erase(
			std::remove_if(snapshots_.begin(), snapshots_.end(),
				[cutoff_time](const LagCompensationSnapshot& snapshot) {
					return snapshot.timestamp < static_cast<uint64_t>(cutoff_time);
				}),
			snapshots_.end());
        
		stats_.snapshots_stored = static_cast<uint32_t>(snapshots_.size());
	}

	auto WorldSynchronizer::calculate_sync_priority(const WorldEntityState& state) const -> uint32_t
	{
		uint32_t priority = 0;
        
		// Higher priority for moving entities
		if (state.is_moving)
		{
			priority += 10;
		}
        
		// Higher priority for entities with recent updates
		auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
        
		auto time_since_update = current_time - state.last_update_time;
		if (time_since_update < 100) // Less than 100ms ago
		{
			priority += 20;
		}
        
		// Higher priority for entities with low health (combat situations)
		if (state.health < state.max_health * 0.3f)
		{
			priority += 15;
		}
        
		return priority;
	}

	auto WorldSynchronizer::interpolate_entity_state(const WorldEntityState& from, const WorldEntityState& to, float factor) const -> WorldEntityState
	{
		WorldEntityState result = to;
        
		// Interpolate position
		result.position.x = from.position.x + (to.position.x - from.position.x) * factor;
		result.position.y = from.position.y + (to.position.y - from.position.y) * factor;
		result.position.z = from.position.z + (to.position.z - from.position.z) * factor;
        
		// Interpolate rotation
		result.rotation = from.rotation + (to.rotation - from.rotation) * factor;
        
		return result;
	}

	auto WorldSynchronizer::extrapolate_entity_position(const WorldEntityState& state, std::chrono::milliseconds delta) const -> Location
	{
		Location extrapolated = state.position;
        
		if (state.is_moving)
		{
			float delta_seconds = delta.count() / 1000.0f;
			extrapolated.x += state.velocity.x * delta_seconds;
			extrapolated.y += state.velocity.y * delta_seconds;
			extrapolated.z += state.velocity.z * delta_seconds;
		}
        
		return extrapolated;
	}

	auto WorldSynchronizer::create_snapshot() -> void
	{
		update_entity_snapshots();
	}

	auto WorldSynchronizer::find_snapshot_at_time(uint64_t timestamp) const -> std::optional<LagCompensationSnapshot>
	{
		std::lock_guard<std::mutex> snapshot_lock(snapshot_mutex_);
        
		if (snapshots_.empty())
		{
			return std::nullopt;
		}
        
		// Find the closest snapshot
		auto it = std::lower_bound(snapshots_.begin(), snapshots_.end(), timestamp,
			[](const LagCompensationSnapshot& snapshot, uint64_t time) {
				return snapshot.timestamp < time;
			});
        
		if (it != snapshots_.end())
		{
			return *it;
		}
        
		// Return the latest snapshot if timestamp is beyond all snapshots
		return snapshots_.back();
	}

	auto WorldSynchronizer::interpolate_snapshots(const LagCompensationSnapshot& earlier, const LagCompensationSnapshot& later, uint64_t timestamp) const -> LagCompensationSnapshot
	{
		if (earlier.timestamp >= later.timestamp)
		{
			return later;
		}
        
		float factor = static_cast<float>(timestamp - earlier.timestamp) / 
						static_cast<float>(later.timestamp - earlier.timestamp);
		factor = std::clamp(factor, 0.0f, 1.0f);
        
		LagCompensationSnapshot result;
		result.timestamp = timestamp;
        
		for (const auto& [entity_id, later_pos] : later.entity_positions)
		{
			auto earlier_it = earlier.entity_positions.find(entity_id);
			if (earlier_it != earlier.entity_positions.end())
			{
				Location interpolated_pos;
				interpolated_pos.x = earlier_it->second.x + (later_pos.x - earlier_it->second.x) * factor;
				interpolated_pos.y = earlier_it->second.y + (later_pos.y - earlier_it->second.y) * factor;
				interpolated_pos.z = earlier_it->second.z + (later_pos.z - earlier_it->second.z) * factor;
                
				result.entity_positions[entity_id] = interpolated_pos;
			}
			else
			{
				result.entity_positions[entity_id] = later_pos;
			}
		}
        
		return result;
	}

	auto WorldSynchronizer::calculate_distance(const Location& a, const Location& b) const -> float
	{
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		float dz = a.z - b.z;
        
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	auto WorldSynchronizer::get_nearby_clients(uint64_t entity_id) const -> std::vector<std::shared_ptr<GameSession>>
	{
		auto entity_state = get_entity_state(entity_id);
		if (!entity_state.has_value())
		{
			return {};
		}
        
		return get_clients_in_range(entity_state->position, aoi_radius_);
	}

	auto WorldSynchronizer::should_sync_to_client(uint64_t entity_id, std::shared_ptr<GameSession> client) const -> bool
	{
		if (!client || !client->is_connected())
		{
			return false;
		}
        
		auto client_entity_id = client->get_entity_id();
		if (client_entity_id == 0) return false; // No entity associated
        
		auto client_state = get_entity_state(client_entity_id);
		auto entity_state = get_entity_state(entity_id);
        
		if (!client_state.has_value() || !entity_state.has_value())
		{
			return false;
		}
        
		float distance = calculate_distance(client_state->position, entity_state->position);
		return distance <= aoi_radius_;
	}
}
