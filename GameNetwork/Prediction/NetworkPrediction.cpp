#include "NetworkPrediction.h"

#include <algorithm>
#include <cmath>

namespace GameNetwork
{
	namespace Prediction
	{
		NetworkPrediction::NetworkPrediction()
			: last_acknowledged_input_(0)
			, stats_{0, 0, 0.0f, 0, 0}
		{
		}

		auto NetworkPrediction::configure(const Config& config) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
			config_ = config;
		}

		auto NetworkPrediction::predict_movement(const EntityState& entity, float delta_time) 
			-> EntityState
		{
			return integrate_physics(entity, delta_time);
		}

		auto NetworkPrediction::predict_entity_state(uint64_t entity_id, 
													std::chrono::steady_clock::time_point target_time)
			-> std::optional<EntityState>
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			// Check if we have current state
			auto it = current_states_.find(entity_id);
			if (it == current_states_.end())
			{
				return std::nullopt;
			}
            
			EntityState current = it->second;
			auto current_time = current.timestamp;
            
			// If target time is in the past, use history
			if (target_time <= current_time)
			{
				auto history_it = state_history_.find(entity_id);
				if (history_it != state_history_.end())
				{
					return get_state_at_time(history_it->second, target_time);
				}
				return current;
			}
            
			// Predict future state
			float delta_seconds = std::chrono::duration<float>(target_time - current_time).count();
			uint32_t prediction_frames = static_cast<uint32_t>(delta_seconds * 60.0f); // Assuming 60fps
            
			if (prediction_frames > config_.max_prediction_frames)
			{
				return std::nullopt; // Too far in the future
			}
            
			EntityState predicted = current;
			float frame_time = 1.0f / 60.0f;
            
			for (uint32_t i = 0; i < prediction_frames; ++i)
			{
				predicted = integrate_physics(predicted, frame_time);
			}
            
			predicted.timestamp = target_time;
			predicted_states_[entity_id] = predicted;
            
			stats_.predictions_made++;
			stats_.max_prediction_frames_used = std::max(stats_.max_prediction_frames_used, prediction_frames);
            
			return predicted;
		}

		auto NetworkPrediction::rollback_on_server_correction(const ServerSnapshot& snapshot) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			// Apply server states
			for (const auto& [entity_id, server_state] : snapshot.entities)
			{
				// Check prediction error
				auto predicted_it = predicted_states_.find(entity_id);
				if (predicted_it != predicted_states_.end())
				{
					float error = calculate_prediction_error(predicted_it->second, server_state);
					stats_.average_prediction_error = 
						(stats_.average_prediction_error * stats_.rollbacks_performed + error) / 
						(stats_.rollbacks_performed + 1);
				}
                
				// Update current state
				current_states_[entity_id] = server_state;
                
				// Add to history
				state_history_[entity_id].push_back(server_state);
				if (state_history_[entity_id].size() > 60) // Keep 1 second of history at 60fps
				{
					state_history_[entity_id].pop_front();
				}
			}
            
			// Update acknowledged input
			last_acknowledged_input_ = snapshot.acknowledged_input_sequence;
            
			// Reapply unacknowledged inputs
			reapply_unacknowledged_inputs();
            
			stats_.rollbacks_performed++;
		}

		auto NetworkPrediction::apply_server_state(const EntityState& server_state) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			uint64_t entity_id = server_state.entity_id;
            
			// Check if we need smoothing
			auto current_it = current_states_.find(entity_id);
			if (current_it != current_states_.end() && config_.enable_smoothing)
			{
				float error = calculate_prediction_error(current_it->second, server_state);
				if (should_smooth_correction(error))
				{
					// Apply smoothed correction
					EntityState& current = current_it->second;
					float alpha = config_.smoothing_rate * (1.0f / 60.0f); // Frame time
                    
					current.position = glm::mix(current.position, server_state.position, alpha);
					current.rotation = glm::slerp(current.rotation, server_state.rotation, alpha);
					current.velocity = server_state.velocity;
					current.acceleration = server_state.acceleration;
					current.angular_velocity = server_state.angular_velocity;
					current.timestamp = server_state.timestamp;
					current.sequence_number = server_state.sequence_number;
                    
					return;
				}
			}
            
			// Direct application
			current_states_[entity_id] = server_state;
		}

		auto NetworkPrediction::interpolate_positions(const EntityState& from, 
													 const EntityState& to,
													 float alpha) -> EntityState
		{
			EntityState result = from;
            
			result.position = glm::mix(from.position, to.position, alpha);
			result.velocity = glm::mix(from.velocity, to.velocity, alpha);
			result.acceleration = glm::mix(from.acceleration, to.acceleration, alpha);
			result.rotation = glm::slerp(from.rotation, to.rotation, alpha);
			result.angular_velocity = glm::mix(from.angular_velocity, to.angular_velocity, alpha);
            
			// Interpolate timestamp
			auto time_diff = to.timestamp - from.timestamp;
			auto alpha_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_diff);
			auto interpolated_ns = std::chrono::nanoseconds(static_cast<int64_t>(alpha_ns.count() * alpha));
			result.timestamp = from.timestamp + interpolated_ns;
            
			stats_.interpolations_performed++;
            
			return result;
		}
        
		auto NetworkPrediction::get_interpolated_state(uint64_t entity_id,
													  std::chrono::steady_clock::time_point render_time)
			-> std::optional<EntityState>
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			auto history_it = state_history_.find(entity_id);
			if (history_it == state_history_.end() || history_it->second.empty())
			{
				// No history, return current state if available
				auto current_it = current_states_.find(entity_id);
				if (current_it != current_states_.end())
				{
					return current_it->second;
				}
				return std::nullopt;
			}
            
			const auto& history = history_it->second;
            
			// Find two states to interpolate between
			auto it = std::lower_bound(history.begin(), history.end(), render_time,
				[](const EntityState& state, std::chrono::steady_clock::time_point time) {
					return state.timestamp < time;
				});
            
			if (it == history.begin())
			{
				// Render time is before all history
				return history.front();
			}
			else if (it == history.end())
			{
				// Render time is after all history
				return history.back();
			}
            
			// Interpolate between two states
			auto prev_it = std::prev(it);
            
			auto total_time = std::chrono::duration<float>(it->timestamp - prev_it->timestamp).count();
			auto elapsed_time = std::chrono::duration<float>(render_time - prev_it->timestamp).count();
            
			float alpha = elapsed_time / total_time;
			alpha = std::clamp(alpha, 0.0f, 1.0f);
            
			return interpolate_positions(*prev_it, *it, alpha);
		}

		auto NetworkPrediction::add_local_input(const ClientInput& input) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			unacknowledged_inputs_.push_back(input);
            
			// Limit buffer size
			while (unacknowledged_inputs_.size() > config_.input_buffer_size)
			{
				unacknowledged_inputs_.pop_front();
			}
		}
        
		auto NetworkPrediction::acknowledge_input(uint32_t sequence) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			last_acknowledged_input_ = sequence;
            
			// Remove acknowledged inputs
			unacknowledged_inputs_.erase(
				std::remove_if(unacknowledged_inputs_.begin(), unacknowledged_inputs_.end(),
					[sequence](const ClientInput& input) {
						return input.sequence <= sequence;
					}),
				unacknowledged_inputs_.end()
			);
		}
        
		auto NetworkPrediction::reapply_unacknowledged_inputs() -> void
		{
			// This is called after server correction
			// Reapply all unacknowledged inputs to maintain client prediction
            
			for (const auto& input : unacknowledged_inputs_)
			{
				// Apply input to local player entity
				// This would typically update the local player's state
				// based on the input
			}
		}
        
		auto NetworkPrediction::update_entity_state(const EntityState& state) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			uint64_t entity_id = state.entity_id;
            
			// Update current state
			current_states_[entity_id] = state;
            
			// Add to history
			state_history_[entity_id].push_back(state);
            
			// Limit history size (keep 2 seconds at 60fps)
			while (state_history_[entity_id].size() > 120)
			{
				state_history_[entity_id].pop_front();
			}
		}
        
		auto NetworkPrediction::remove_entity(uint64_t entity_id) -> void
		{
			std::lock_guard<std::mutex> lock(mutex_);
            
			current_states_.erase(entity_id);
			state_history_.erase(entity_id);
			predicted_states_.erase(entity_id);
		}

		auto NetworkPrediction::get_statistics() const -> PredictionStats
		{
			std::lock_guard<std::mutex> lock(mutex_);
			return stats_;
		}
        
		auto NetworkPrediction::integrate_physics(const EntityState& state, float delta_time) 
			-> EntityState
		{
			EntityState result = state;
            
			// Update position based on velocity and acceleration
			result.position += state.velocity * delta_time + 
							  0.5f * state.acceleration * delta_time * delta_time;
            
			// Update velocity
			result.velocity += state.acceleration * delta_time;
            
			// Update rotation based on angular velocity
			float angle = glm::length(state.angular_velocity) * delta_time;
			if (angle > 0.0001f)
			{
				glm::vec3 axis = glm::normalize(state.angular_velocity);
				glm::quat rotation = glm::angleAxis(angle, axis);
				result.rotation = rotation * state.rotation;
			}
            
			return result;
		}
        
		auto NetworkPrediction::calculate_prediction_error(const EntityState& predicted,
														 const EntityState& actual) -> float
		{
			float position_error = glm::length(predicted.position - actual.position);
            
			// Calculate rotation error in degrees
			glm::quat rotation_diff = actual.rotation * glm::inverse(predicted.rotation);
			float rotation_error = glm::degrees(2.0f * std::acos(std::abs(rotation_diff.w)));
            
			// Weighted error calculation
			float total_error = position_error + (rotation_error / config_.rotation_error_threshold) * config_.position_error_threshold;
            
			return total_error;
		}
        
		auto NetworkPrediction::should_smooth_correction(float error) const -> bool
		{
			return error < config_.position_error_threshold * 5.0f; // Smooth if error is reasonable
		}
        
		auto NetworkPrediction::get_state_at_time(const StateHistory& history,
												std::chrono::steady_clock::time_point time)
			-> std::optional<EntityState>
		{
			if (history.empty())
			{
				return std::nullopt;
			}
            
			// Binary search for the closest state
			auto it = std::lower_bound(history.begin(), history.end(), time,
				[](const EntityState& state, std::chrono::steady_clock::time_point t) {
					return state.timestamp < t;
				});
            
			if (it == history.begin())
			{
				return history.front();
			}
			else if (it == history.end())
			{
				return history.back();
			}
            
			// Return the closest state
			auto prev_it = std::prev(it);
			auto diff_prev = std::abs(std::chrono::duration<float>(time - prev_it->timestamp).count());
			auto diff_next = std::abs(std::chrono::duration<float>(it->timestamp - time).count());
            
			return (diff_prev < diff_next) ? *prev_it : *it;
		}
	}
}
