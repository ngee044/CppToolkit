#include "NetworkMetrics.h"

#include <algorithm>
#include <numeric>
#include <limits>
#include <sstream>
#include <thread>

namespace GameNetwork
{
	NetworkMetrics::NetworkMetrics()
		: current_rtt_(0.0f)
		, average_rtt_(0.0f)
		, packets_sent_(0)
		, packets_received_(0)
		, packets_lost_(0)
		, current_jitter_(0.0f)
		, external_export_enabled_(false)
		, export_interval_(std::chrono::seconds(60))
		, monitoring_active_(false)
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
        
		// Set default alert thresholds
		alert_thresholds_.rtt_threshold_ms = 200.0f;
		alert_thresholds_.packet_loss_threshold = 0.05f;
		alert_thresholds_.jitter_threshold_ms = 50.0f;
		alert_thresholds_.bandwidth_threshold_bps = 1000000;
		alert_thresholds_.connection_score_threshold = 50.0f;
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
            
			current_rtt_ = rtt_ms;
            
			rtt_history_.push_back(rtt_ms);
			if (rtt_history_.size() > MAX_RTT_HISTORY)
			{
				rtt_history_.pop_front();
			}
            
			if (!rtt_history_.empty())
			{
				average_rtt_ = std::accumulate(rtt_history_.begin(), rtt_history_.end(), 0.0f) / rtt_history_.size();
			}
            
			stats_.min_rtt_ms = std::min(stats_.min_rtt_ms, rtt_ms);
			stats_.max_rtt_ms = std::max(stats_.max_rtt_ms, rtt_ms);
			stats_.average_rtt_ms = average_rtt_;
            
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
			std::remove_if(sent_bandwidth_samples_.begin(), sent_bandwidth_samples_.end(), remove_old), sent_bandwidth_samples_.end());
        
		received_bandwidth_samples_.erase(
			std::remove_if(received_bandwidth_samples_.begin(), received_bandwidth_samples_.end(), remove_old), received_bandwidth_samples_.end());
        
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
        
		auto time_span = samples.back().timestamp - samples.front().timestamp;
		auto seconds = std::chrono::duration<float>(time_span).count();
        
		if (seconds <= 0.0f)
		{
			return 0;
		}
        
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
    
	auto NetworkMetrics::export_metrics_json() const -> std::string
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto quality_info = get_quality_info();
		auto stats = get_stats();
        
		// TODO
		// using boost json
		std::ostringstream json;
		json << "{\n";
		json << "  \"timestamp\": \"" << std::chrono::duration_cast<std::chrono::seconds>(
				   std::chrono::steady_clock::now().time_since_epoch()).count() << "\",\n";
		json << "  \"rtt_ms\": " << quality_info.rtt_ms << ",\n";
		json << "  \"packet_loss_rate\": " << quality_info.packet_loss_rate << ",\n";
		json << "  \"jitter_ms\": " << quality_info.jitter_ms << ",\n";
		json << "  \"bandwidth_usage_bps\": " << quality_info.bandwidth_usage_bps << ",\n";
		json << "  \"connection_score\": " << get_connection_score() << ",\n";
		json << "  \"network_quality\": \"";
        
		switch (get_network_quality()) {
			case NetworkQuality::Excellent: json << "Excellent"; break;
			case NetworkQuality::Good: json << "Good"; break;
			case NetworkQuality::Fair: json << "Fair"; break;
			case NetworkQuality::Poor: json << "Poor"; break;
			case NetworkQuality::Unplayable: json << "Unplayable"; break;
		}
        
		json << "\",\n";
		json << "  \"stats\": {\n";
		json << "    \"total_packets_sent\": " << stats.total_packets_sent << ",\n";
		json << "    \"total_packets_received\": " << stats.total_packets_received << ",\n";
		json << "    \"total_packets_lost\": " << stats.total_packets_lost << ",\n";
		json << "    \"total_bytes_sent\": " << stats.total_bytes_sent << ",\n";
		json << "    \"total_bytes_received\": " << stats.total_bytes_received << ",\n";
		json << "    \"min_rtt_ms\": " << stats.min_rtt_ms << ",\n";
		json << "    \"max_rtt_ms\": " << stats.max_rtt_ms << ",\n";
		json << "    \"average_rtt_ms\": " << stats.average_rtt_ms << "\n";
		json << "  }\n";
		json << "}";
        
		return json.str();
	}

	auto NetworkMetrics::export_metrics_csv() const -> std::string
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto quality_info = get_quality_info();
		auto stats = get_stats();
        
		std::ostringstream csv;
        
		// Header (only write if this is the first export)
		static bool header_written = false;
		if (!header_written) {
			csv << "timestamp,rtt_ms,packet_loss_rate,jitter_ms,bandwidth_usage_bps,connection_score,";
			csv << "total_packets_sent,total_packets_received,total_packets_lost,";
			csv << "total_bytes_sent,total_bytes_received,min_rtt_ms,max_rtt_ms,average_rtt_ms\n";
			header_written = true;
		}
        
		// Data
		csv << std::chrono::duration_cast<std::chrono::seconds>(
				   std::chrono::steady_clock::now().time_since_epoch()).count() << ",";
		csv << quality_info.rtt_ms << ",";
		csv << quality_info.packet_loss_rate << ",";
		csv << quality_info.jitter_ms << ",";
		csv << quality_info.bandwidth_usage_bps << ",";
		csv << get_connection_score() << ",";
		csv << stats.total_packets_sent << ",";
		csv << stats.total_packets_received << ",";
		csv << stats.total_packets_lost << ",";
		csv << stats.total_bytes_sent << ",";
		csv << stats.total_bytes_received << ",";
		csv << stats.min_rtt_ms << ",";
		csv << stats.max_rtt_ms << ",";
		csv << stats.average_rtt_ms << "\n";
        
		return csv.str();
	}

	auto NetworkMetrics::export_metrics_prometheus() const -> std::string
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto quality_info = get_quality_info();
		auto stats = get_stats();
		auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
        
		std::ostringstream prometheus;
        
		prometheus << "# HELP network_rtt_milliseconds Round trip time in milliseconds\n";
		prometheus << "# TYPE network_rtt_milliseconds gauge\n";
		prometheus << "network_rtt_milliseconds " << quality_info.rtt_ms << " " << timestamp << "\n";
        
		prometheus << "# HELP network_packet_loss_rate Packet loss rate (0.0-1.0)\n";
		prometheus << "# TYPE network_packet_loss_rate gauge\n";
		prometheus << "network_packet_loss_rate " << quality_info.packet_loss_rate << " " << timestamp << "\n";
        
		prometheus << "# HELP network_jitter_milliseconds Jitter in milliseconds\n";
		prometheus << "# TYPE network_jitter_milliseconds gauge\n";
		prometheus << "network_jitter_milliseconds " << quality_info.jitter_ms << " " << timestamp << "\n";
        
		prometheus << "# HELP network_bandwidth_usage_bps Bandwidth usage in bits per second\n";
		prometheus << "# TYPE network_bandwidth_usage_bps gauge\n";
		prometheus << "network_bandwidth_usage_bps " << quality_info.bandwidth_usage_bps << " " << timestamp << "\n";
        
		prometheus << "# HELP network_connection_score Connection quality score (0-100)\n";
		prometheus << "# TYPE network_connection_score gauge\n";
		prometheus << "network_connection_score " << get_connection_score() << " " << timestamp << "\n";
        
		prometheus << "# HELP network_packets_sent_total Total number of packets sent\n";
		prometheus << "# TYPE network_packets_sent_total counter\n";
		prometheus << "network_packets_sent_total " << stats.total_packets_sent << " " << timestamp << "\n";
        
		prometheus << "# HELP network_packets_received_total Total number of packets received\n";
		prometheus << "# TYPE network_packets_received_total counter\n";
		prometheus << "network_packets_received_total " << stats.total_packets_received << " " << timestamp << "\n";
        
		prometheus << "# HELP network_packets_lost_total Total number of packets lost\n";
		prometheus << "# TYPE network_packets_lost_total counter\n";
		prometheus << "network_packets_lost_total " << stats.total_packets_lost << " " << timestamp << "\n";
        
		return prometheus.str();
	}

	auto NetworkMetrics::set_alert_thresholds(const AlertThreshold& thresholds) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		alert_thresholds_ = thresholds;
	}

	auto NetworkMetrics::get_alert_thresholds() const -> AlertThreshold
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return alert_thresholds_;
	}

	auto NetworkMetrics::register_alert_callback(AlertCallback callback) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		alert_callbacks_.push_back(callback);
	}

	auto NetworkMetrics::clear_alert_callbacks() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		alert_callbacks_.clear();
	}

	auto NetworkMetrics::register_metrics_callback(MetricsChangeCallback callback) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		metrics_callbacks_.push_back(callback);
	}

	auto NetworkMetrics::clear_metrics_callbacks() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		metrics_callbacks_.clear();
	}

	auto NetworkMetrics::enable_external_export(bool enable) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		external_export_enabled_ = enable;
	}

	auto NetworkMetrics::set_export_interval(std::chrono::seconds interval) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		export_interval_ = interval;
	}

	auto NetworkMetrics::get_export_interval() const -> std::chrono::seconds
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return export_interval_;
	}

	auto NetworkMetrics::start_monitoring() -> void
	{
		if (monitoring_active_)
		{
			return;
		}
        
		monitoring_active_ = true;
		monitoring_thread_ = std::thread(&NetworkMetrics::monitoring_thread_func, this);
	}

	auto NetworkMetrics::stop_monitoring() -> void
	{
		monitoring_active_ = false;
		if (monitoring_thread_.joinable())
		{
			monitoring_thread_.join();
		}
	}

	auto NetworkMetrics::is_monitoring_active() const -> bool
	{
		return monitoring_active_;
	}

	auto NetworkMetrics::check_alerts(const NetworkQualityInfo& quality_info) -> void
	{
		if (quality_info.rtt_ms > alert_thresholds_.rtt_threshold_ms)
		{
			trigger_alert("rtt", quality_info.rtt_ms, alert_thresholds_.rtt_threshold_ms, 
						 quality_info.rtt_ms > alert_thresholds_.rtt_threshold_ms * 2 ? "critical" : "warning");
		}
        
		if (quality_info.packet_loss_rate > alert_thresholds_.packet_loss_threshold)
		{
			trigger_alert("packet_loss", quality_info.packet_loss_rate, alert_thresholds_.packet_loss_threshold,
						 quality_info.packet_loss_rate > alert_thresholds_.packet_loss_threshold * 2 ? "critical" : "warning");
		}
        
		if (quality_info.jitter_ms > alert_thresholds_.jitter_threshold_ms)
		{
			trigger_alert("jitter", quality_info.jitter_ms, alert_thresholds_.jitter_threshold_ms,
						 quality_info.jitter_ms > alert_thresholds_.jitter_threshold_ms * 2 ? "critical" : "warning");
		}
        
		float connection_score = get_connection_score();
		if (connection_score < alert_thresholds_.connection_score_threshold)
		{
			trigger_alert("connection_score", connection_score, alert_thresholds_.connection_score_threshold,
						 connection_score < alert_thresholds_.connection_score_threshold * 0.5f ? "critical" : "warning");
		}
	}

	auto NetworkMetrics::trigger_alert(const std::string& metric_name, float current_value, 
									   float threshold, const std::string& severity) -> void
	{
		for (const auto& callback : alert_callbacks_)
		{
			try
			{
				callback(metric_name, current_value, threshold, severity);
			}
			catch (const std::exception& e)
			{
				// Log callback error but don't throw
			}
		}
	}

	auto NetworkMetrics::trigger_metrics_callbacks(const NetworkQualityInfo& quality_info) -> void
	{
		for (const auto& callback : metrics_callbacks_)
		{
			try
			{
				callback(quality_info);
			}
			catch (const std::exception& e)
			{
				// Log callback error but don't throw
			}
		}
	}

	auto NetworkMetrics::monitoring_thread_func() -> void
	{
		while (monitoring_active_)
		{
			std::this_thread::sleep_for(export_interval_);
            
			if (!monitoring_active_)
			{
				break;
			}
            
			try
			{
				auto quality_info = get_quality_info();
                
				// Check alerts
				check_alerts(quality_info);
                
				// Trigger metrics callbacks
				trigger_metrics_callbacks(quality_info);
                
				// Export to external systems if enabled
				if (external_export_enabled_)
				{
					// In a real implementation, you would send these to external systems
					// For now, just prepare the export strings
					auto json_export = export_metrics_json();
					auto prometheus_export = export_metrics_prometheus();
                    
					// Example: Send to monitoring endpoints
					// http_client.post("/metrics", json_export);
					// prometheus_client.push(prometheus_export);
				}
			}
			catch (const std::exception& e)
			{
				// Log monitoring error but continue
			}
		}
	}
}