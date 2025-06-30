#include "LatencyCompensator.h"
#include <numeric>
#include <algorithm>
#include <cmath>

namespace GameNetwork
{
	LatencyCompensator::LatencyCompensator() = default;
	LatencyCompensator::~LatencyCompensator() = default;
    
	auto LatencyCompensator::update_player_latency(uint64_t player_id, std::chrono::milliseconds latency) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		// Add to samples queue
		auto& samples = latency_samples_[player_id];
		samples.push(latency);
        
		// Keep only recent samples
		while (samples.size() > MAX_LATENCY_SAMPLES)
		{
			samples.pop();
		}
        
		// Update player latency info
		auto& info = player_latencies_[player_id];
		info.player_id = player_id;
		info.last_update = std::chrono::steady_clock::now();
		info.sample_count = static_cast<uint32_t>(samples.size());
        
		// Calculate statistics
		if (!samples.empty())
		{
			std::vector<std::chrono::milliseconds> sample_vec;
			auto temp_queue = samples;
			while (!temp_queue.empty())
			{
				sample_vec.push_back(temp_queue.front());
				temp_queue.pop();
			}
            
			// Calculate average
			auto sum = std::accumulate(sample_vec.begin(), sample_vec.end(), std::chrono::milliseconds(0));
			info.average_latency = sum / sample_vec.size();
            
			// Calculate min/max
			auto [min_it, max_it] = std::minmax_element(sample_vec.begin(), sample_vec.end());
			info.min_latency = *min_it;
			info.max_latency = *max_it;
            
			// Calculate jitter
			info.jitter = calculate_jitter(samples);
		}
	}
    
	auto LatencyCompensator::get_player_latency(uint64_t player_id) const -> std::optional<PlayerLatencyInfo>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = player_latencies_.find(player_id);
		if (it != player_latencies_.end())
		{
			return it->second;
		}
        
		return std::nullopt;
	}
    
	auto LatencyCompensator::get_average_latency(uint64_t player_id) const -> std::chrono::milliseconds
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = player_latencies_.find(player_id);
		if (it != player_latencies_.end())
		{
			return it->second.average_latency;
		}
        
		return std::chrono::milliseconds(0);
	}
    
	auto LatencyCompensator::compensate_event(const CompensatedEvent& event) -> CompensatedEvent
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		CompensatedEvent compensated = event;
        
		// Get player's average latency
		auto latency = get_average_latency(event.player_id);
        
		// Apply latency compensation
		compensated.server_timestamp = event.client_timestamp + latency;
		compensated.estimated_latency = latency;
		compensated.is_compensated = true;
        
		// Update statistics
		events_compensated_++;
		total_compensation_time_ += latency;
        
		// Notify callbacks
		for (const auto& callback : compensation_callbacks_)
		{
			callback(compensated);
		}
        
		return compensated;
	}
    
	auto LatencyCompensator::add_event_for_compensation(uint64_t player_id, 
														 uint32_t event_id,
														 const std::vector<uint8_t>& event_data,
														 std::chrono::steady_clock::time_point client_timestamp) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		// Check if within compensation window
		if (!is_within_compensation_window(client_timestamp))
		{
			events_rejected_++;
			return;
		}
        
		CompensatedEvent event;
		event.player_id = player_id;
		event.event_id = event_id;
		event.client_timestamp = client_timestamp;
		event.server_timestamp = std::chrono::steady_clock::now();
		event.event_data = event_data;
		event.is_compensated = false;
        
		pending_events_[player_id].push(event);
	}
    
	auto LatencyCompensator::synchronize_time(uint64_t player_id, 
											   std::chrono::steady_clock::time_point client_time,
											   std::chrono::steady_clock::time_point server_time) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto offset = std::chrono::duration_cast<std::chrono::milliseconds>(server_time - client_time);
		time_offsets_[player_id] = offset;
	}
    
	auto LatencyCompensator::get_compensated_time(uint64_t player_id, 
												   std::chrono::steady_clock::time_point client_time) const 
		-> std::chrono::steady_clock::time_point
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = time_offsets_.find(player_id);
		if (it != time_offsets_.end())
		{
			return client_time + it->second;
		}
        
		return client_time;
	}
    
	auto LatencyCompensator::set_max_compensation_window(std::chrono::milliseconds window) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		max_compensation_window_ = window;
	}
    
	auto LatencyCompensator::set_interpolation_delay(std::chrono::milliseconds delay) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		interpolation_delay_ = delay;
	}
    
	auto LatencyCompensator::enable_jitter_smoothing(bool enable) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		jitter_smoothing_enabled_ = enable;
	}
    
	auto LatencyCompensator::on_event_compensated(CompensationCallback callback) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		compensation_callbacks_.push_back(callback);
	}
    
	auto LatencyCompensator::get_compensation_stats() const 
		-> std::tuple<uint64_t, uint64_t, std::chrono::milliseconds>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto avg_compensation = events_compensated_ > 0 
			? total_compensation_time_ / events_compensated_ 
			: std::chrono::milliseconds(0);
            
		return {events_compensated_, events_rejected_, avg_compensation};
	}
    
	auto LatencyCompensator::reset_stats() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		events_compensated_ = 0;
		events_rejected_ = 0;
		total_compensation_time_ = std::chrono::milliseconds(0);
	}
    
	auto LatencyCompensator::calculate_jitter(const std::queue<std::chrono::milliseconds>& samples) const 
		-> std::chrono::milliseconds
	{
		if (samples.size() < 2)
		{
			return std::chrono::milliseconds(0);
		}
        
		std::vector<std::chrono::milliseconds> sample_vec;
		auto temp_queue = samples;
		while (!temp_queue.empty())
		{
			sample_vec.push_back(temp_queue.front());
			temp_queue.pop();
		}
        
		// Calculate mean absolute deviation
		auto mean = std::accumulate(sample_vec.begin(), sample_vec.end(), std::chrono::milliseconds(0)) / sample_vec.size();
        
		int64_t total_deviation = 0;
		for (const auto& sample : sample_vec)
		{
			total_deviation += std::abs(static_cast<int64_t>((sample - mean).count()));
		}
        
		return std::chrono::milliseconds(total_deviation / sample_vec.size());
	}
    
	auto LatencyCompensator::smooth_latency(uint64_t player_id, std::chrono::milliseconds raw_latency) const 
		-> std::chrono::milliseconds
	{
		if (!jitter_smoothing_enabled_)
		{
			return raw_latency;
		}
        
		auto it = player_latencies_.find(player_id);
		if (it != player_latencies_.end())
		{
			// Apply exponential moving average
			const double alpha = 0.3; // Smoothing factor
			auto smoothed = static_cast<int64_t>(
				alpha * raw_latency.count() + (1.0 - alpha) * it->second.average_latency.count()
			);
			return std::chrono::milliseconds(smoothed);
		}
        
		return raw_latency;
	}
    
	auto LatencyCompensator::is_within_compensation_window(std::chrono::steady_clock::time_point event_time) const -> bool
	{
		auto now = std::chrono::steady_clock::now();
		auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - event_time);
        
		return age <= max_compensation_window_;
	}
    
} // namespace GameNetwork
