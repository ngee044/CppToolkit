#pragma once

#include <chrono>
#include <atomic>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <optional>
#include <tuple>
#include <functional>
#include <vector>
#include <thread>
#include <functional>

namespace GameNetwork
{
	struct NetworkQualityInfo
	{
		float rtt_ms;                // Round Trip Time in milliseconds
		float packet_loss_rate;      // 0.0 ~ 1.0
		float jitter_ms;             // Jitter in milliseconds
		uint64_t bandwidth_usage_bps; // Bits per second
		std::chrono::steady_clock::time_point timestamp;
	};
    
	enum class NetworkQuality
	{
		Excellent = 0,  // RTT < 50ms, Loss < 0.1%
		Good = 1,       // RTT < 100ms, Loss < 1%
		Fair = 2,       // RTT < 200ms, Loss < 3%
		Poor = 3,       // RTT < 300ms, Loss < 5%
		Unplayable = 4  // RTT >= 300ms or Loss >= 5%
	};
    
	class NetworkMetrics
	{
	public:
		NetworkMetrics();
		virtual ~NetworkMetrics() = default;
        
		// RTT measurement
		auto record_ping_sent(uint32_t sequence) -> void;
		auto record_ping_received(uint32_t sequence) -> void;
		auto get_average_rtt() const -> float;
		auto get_current_rtt() const -> float;
        
		// Packet loss tracking
		auto record_packet_sent() -> void;
		auto record_packet_received() -> void;
		auto record_packet_lost() -> void;
		auto get_packet_loss_rate() const -> float;
        
		// Bandwidth monitoring
		auto record_bytes_sent(uint64_t bytes) -> void;
		auto record_bytes_received(uint64_t bytes) -> void;
		auto get_bandwidth_usage() const -> uint64_t;
		auto get_send_bandwidth() const -> uint64_t;
		auto get_receive_bandwidth() const -> uint64_t;
        
		// Jitter measurement
		auto calculate_jitter() -> float;
		auto get_current_jitter() const -> float;
        
		// Network quality assessment
		auto get_network_quality() const -> NetworkQuality;
		auto get_quality_info() const -> NetworkQualityInfo;
        
		// Adaptive settings based on quality
		auto get_recommended_send_rate() const -> uint32_t;
		auto get_recommended_update_rate() const -> uint32_t;
		auto should_enable_compression() const -> bool;
		auto should_reduce_quality() const -> bool;
        
		// Statistics
		struct MetricsStats
		{
			uint64_t total_packets_sent;
			uint64_t total_packets_received;
			uint64_t total_packets_lost;
			uint64_t total_bytes_sent;
			uint64_t total_bytes_received;
			float min_rtt_ms;
			float max_rtt_ms;
			float average_rtt_ms;
			std::chrono::steady_clock::time_point start_time;
		};
        
		auto get_stats() const -> MetricsStats;
		auto reset_stats() -> void;
        
		// Connection scoring (0-100)
		auto get_connection_score() const -> float;
        
		// Metrics export
		auto export_metrics_json() const -> std::string;
		auto export_metrics_csv() const -> std::string;
		auto export_metrics_prometheus() const -> std::string;
        
		// Alert thresholds and callbacks
		struct AlertThreshold
		{
			float rtt_threshold_ms = 200.0f;
			float packet_loss_threshold = 0.05f; // 5%
			float jitter_threshold_ms = 50.0f;
			uint64_t bandwidth_threshold_bps = 1000000; // 1 Mbps
			float connection_score_threshold = 50.0f;
		};
        
		using AlertCallback = std::function<void(const std::string& metric_name, 
												float current_value, 
												float threshold, 
												const std::string& severity)>;
        
		auto set_alert_thresholds(const AlertThreshold& thresholds) -> void;
		auto get_alert_thresholds() const -> AlertThreshold;
		auto register_alert_callback(AlertCallback callback) -> void;
		auto clear_alert_callbacks() -> void;
        
		// Metrics change callbacks
		using MetricsChangeCallback = std::function<void(const NetworkQualityInfo& quality_info)>;
		auto register_metrics_callback(MetricsChangeCallback callback) -> void;
		auto clear_metrics_callbacks() -> void;
        
		// External monitoring integration
		auto enable_external_export(bool enable) -> void;
		auto set_export_interval(std::chrono::seconds interval) -> void;
		auto get_export_interval() const -> std::chrono::seconds;
        
		// Real-time monitoring
		auto start_monitoring() -> void;
		auto stop_monitoring() -> void;
		auto is_monitoring_active() const -> bool;
        
	private:
		struct PingInfo
		{
			std::chrono::steady_clock::time_point sent_time;
			bool received;
		};
        
		struct BandwidthSample
		{
			uint64_t bytes;
			std::chrono::steady_clock::time_point timestamp;
		};
        
		auto update_bandwidth_samples() -> void;
		auto calculate_bandwidth(const std::deque<BandwidthSample>& samples) const -> uint64_t;
        
	private:
		mutable std::mutex mutex_;
        
		// RTT tracking
		std::unordered_map<uint32_t, PingInfo> pending_pings_;
		std::deque<float> rtt_history_;
		float current_rtt_;
		float average_rtt_;
        
		// Packet loss tracking
		std::atomic<uint64_t> packets_sent_;
		std::atomic<uint64_t> packets_received_;
		std::atomic<uint64_t> packets_lost_;
        
		// Bandwidth tracking
		std::deque<BandwidthSample> sent_bandwidth_samples_;
		std::deque<BandwidthSample> received_bandwidth_samples_;
		std::chrono::steady_clock::time_point last_bandwidth_update_;
        
		// Jitter tracking
		std::deque<float> jitter_samples_;
		float current_jitter_;
        
		// Statistics
		MetricsStats stats_;
        
		// Alert system
		AlertThreshold alert_thresholds_;
		std::vector<AlertCallback> alert_callbacks_;
		std::vector<MetricsChangeCallback> metrics_callbacks_;
        
		// External monitoring
		bool external_export_enabled_;
		std::chrono::seconds export_interval_;
		std::thread monitoring_thread_;
		std::atomic<bool> monitoring_active_;
        
		// Helper methods for alerts
		auto check_alerts(const NetworkQualityInfo& quality_info) -> void;
		auto trigger_alert(const std::string& metric_name, float current_value, 
						  float threshold, const std::string& severity) -> void;
		auto trigger_metrics_callbacks(const NetworkQualityInfo& quality_info) -> void;
		auto monitoring_thread_func() -> void;
        
		// Configuration constants
		static constexpr size_t MAX_RTT_HISTORY = 100;
		static constexpr size_t MAX_BANDWIDTH_SAMPLES = 60;
		static constexpr size_t MAX_JITTER_SAMPLES = 30;
		static constexpr auto BANDWIDTH_SAMPLE_INTERVAL = std::chrono::seconds(1);
	};
}
