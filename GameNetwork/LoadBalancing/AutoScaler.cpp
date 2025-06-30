#include "AutoScaler.h"
#include "ServerMonitor.h"
#include "LoadBalancer.h"
#include "SeamlessMigration.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>
#include <numeric>

using namespace Utilities;

namespace GameNetwork
{
	AutoScaler::AutoScaler()
		: auto_scaling_enabled_(true)
		, current_instance_count_(1)
		, stats_{0, 0, 0, std::chrono::steady_clock::now(), 0.0f, 0.0f}
	{
		configure(ScalingPolicy{});
	}

	AutoScaler::~AutoScaler() = default;

	auto AutoScaler::configure(const ScalingPolicy& policy) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		policy_ = policy;

		Logger::handle().write(LogTypes::Information, fmt::format("AutoScaler configured with min={} , max={}", policy.min_instances, policy.max_instances));
	}

	auto AutoScaler::set_server_monitor(std::shared_ptr<ServerMonitor> monitor) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		server_monitor_ = monitor;
	}

	auto AutoScaler::set_load_balancer(std::shared_ptr<LoadBalancer> balancer) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		load_balancer_ = balancer;
	}

	auto AutoScaler::evaluate_scaling_need() -> std::tuple<bool, std::optional<std::string>>
	{
		if (!auto_scaling_enabled_)
		{
			return { false, "Auto-scaling is disabled" };
		}

		std::lock_guard<std::mutex> lock(mutex_);
        
		stats_.scaling_decisions_made++;
        
		// Check cooldown
		if (is_in_cooldown())
		{
			return { false, "In cooldown period" };
		}
        
		// Calculate aggregate metrics
		auto metrics = calculate_aggregate_metrics();
        
		// Update statistics
		stats_.average_cpu_usage = metrics.cpu_usage;
		stats_.average_memory_usage = metrics.memory_usage;
        
		// Check if we should scale up
		if (should_scale_up(metrics))
		{
			uint32_t target_instances = static_cast<uint32_t>(std::ceil(current_instance_count_ * policy_.scale_up_factor));
			target_instances = std::min(target_instances, policy_.max_instances);
            
			if (target_instances > current_instance_count_)
			{
				uint32_t additional = target_instances - current_instance_count_;
				return scale_up(additional);
			}
		}

		// Check if we should scale down
		if (should_scale_down(metrics))
		{
			uint32_t target_instances = static_cast<uint32_t>(std::floor(current_instance_count_ * policy_.scale_down_factor));
			target_instances = std::max(target_instances, policy_.min_instances);
            
			if (target_instances < current_instance_count_)
			{
				uint32_t to_remove = current_instance_count_ - target_instances;
				return scale_down(to_remove);
			}
		}
        
		return { true, "No scaling needed" };
	}

	auto AutoScaler::scale_up(uint32_t additional_instances) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (current_instance_count_ + additional_instances > policy_.max_instances)
		{
			additional_instances = policy_.max_instances - current_instance_count_;
			if (additional_instances == 0)
			{
				return { false, "Already at maximum instances" };
			}
		}
        
		uint32_t old_count = current_instance_count_;
		current_instance_count_ += additional_instances;
        
		// Update scaling time
		last_scaling_time_ = std::chrono::steady_clock::now();
		stats_.scale_up_events++;
		stats_.last_scaling_time = last_scaling_time_;
        
		// Notify callback
		if (scaling_callback_)
		{
			scaling_callback_(old_count, current_instance_count_);
		}

		Logger::handle().write(LogTypes::Information, fmt::format("Scaled up from {} to {} instances", old_count, current_instance_count_));
        
		// Actually provision new instances through cloud provider API
		// In a real implementation, this would:
		// 1. Call cloud provider API (AWS, Azure, GCP)
		// 2. Launch new server instances
		// 3. Configure them with game server software
		// 4. Register them with the load balancer
        
		// For now, simulate the provisioning process
		std::thread provision_thread([this, additional_instances]() {
			// Simulate provisioning delay
			std::this_thread::sleep_for(std::chrono::seconds(30));
            
			// Register new instances with load balancer
			auto lb = load_balancer_.lock();
			if (lb)
			{
				for (uint32_t i = 0; i < additional_instances; ++i)
				{
					std::string server_id = "server_" + 
						std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + 
						"_" + std::to_string(i);
                    
					GameNetwork::ServerInfo info;
					info.server_id = server_id;
					info.capacity = 1000; // Default capacity
					info.current_load = 0;
					info.is_healthy = true;
                    
					lb->register_server(info);
				}
			}
            
			Logger::handle().write(LogTypes::Information, fmt::format("Provisioned {} new instances", additional_instances));
		});
		provision_thread.detach();
        
		return { true, std::nullopt };
	}

	auto AutoScaler::scale_down(uint32_t instances_to_remove) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (current_instance_count_ - instances_to_remove < policy_.min_instances)
		{
			instances_to_remove = current_instance_count_ - policy_.min_instances;
			if (instances_to_remove == 0)
			{
				return { false, "Already at minimum instances" };
			}
		}
        
		uint32_t old_count = current_instance_count_;
		current_instance_count_ -= instances_to_remove;
        
		// Update scaling time
		last_scaling_time_ = std::chrono::steady_clock::now();
		stats_.scale_down_events++;
		stats_.last_scaling_time = last_scaling_time_;
        
		// Notify callback
		if (scaling_callback_)
		{
			scaling_callback_(old_count, current_instance_count_);
		}
        
		Logger::handle().write(LogTypes::Information, fmt::format("Scaled down from {} to {} instances", old_count, current_instance_count_));

		// Actually deprovision instances through cloud provider API
		// In a real implementation, this would:
		// 1. Select instances to remove (least loaded first)
		// 2. Migrate sessions from those instances
		// 3. Gracefully shutdown the instances
		// 4. Terminate them through cloud provider API
        
		// For now, simulate the deprovisioning process
		std::thread deprovision_thread([this, instances_to_remove]() {
			auto lb = load_balancer_.lock();
			if (lb)
			{
				auto servers = lb->get_server_list();
				std::sort(servers.begin(), servers.end(), 
					[](const auto& a, const auto& b) {
						return a.current_load < b.current_load;
					});
                
				size_t removed = 0;
				for (const auto& server : servers)
				{
					if (removed >= instances_to_remove) break;
                    
					auto migration = seamless_migration_.lock();
					if (migration)
					{
						migration->migrate_server_load(server.server_id, "auto_balanced_target", 100);
					}
                    
					std::this_thread::sleep_for(std::chrono::seconds(10));
                    
					lb->unregister_server(server.server_id);
					removed++;
				}
			}
            
			Logger::handle().write(LogTypes::Information, fmt::format("Deprovisioned {} instances", instances_to_remove));
		});
		deprovision_thread.detach();
        
		return { true, std::nullopt };
	}

	auto AutoScaler::enable_auto_scaling(bool enable) -> void
	{
		auto_scaling_enabled_ = enable;
        
		Logger::handle().write(LogTypes::Information, fmt::format("Auto-scaling {}", enable ? "enabled" : "disabled"));
	}

	auto AutoScaler::force_scale_to_instances(uint32_t target_instances) 
		-> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (target_instances < policy_.min_instances || target_instances > policy_.max_instances)
		{
			return { false, "Target instances outside allowed range" };
		}
        
		uint32_t old_count = current_instance_count_;
		current_instance_count_ = target_instances;
        
		// Clear cooldown for forced scaling
		last_scaling_time_ = std::chrono::steady_clock::time_point{};
        
		// Notify callback
		if (scaling_callback_)
		{
			scaling_callback_(old_count, current_instance_count_);
		}
        
		return { true, std::nullopt };
	}

	auto AutoScaler::update_server_metrics(const std::string& server_id, const ServerMetrics& metrics) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		server_metrics_[server_id] = metrics;
	}

	auto AutoScaler::get_current_instance_count() const -> uint32_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return current_instance_count_;
	}

	auto AutoScaler::get_recommended_instance_count() const -> uint32_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto metrics = calculate_aggregate_metrics();
        
		// Simple calculation based on load
		float load_factor = std::max({
			metrics.cpu_usage / policy_.cpu_threshold_up,
			metrics.memory_usage / policy_.memory_threshold_up,
			static_cast<float>(metrics.active_connections) / policy_.connection_threshold_up
		});
        
		uint32_t recommended = static_cast<uint32_t>(std::ceil(current_instance_count_ * load_factor));
		return std::clamp(recommended, policy_.min_instances, policy_.max_instances);
	}

	auto AutoScaler::set_scaling_callback(ScalingCallback callback) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		scaling_callback_ = callback;
	}

	auto AutoScaler::get_statistics() const -> ScalingStats
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return stats_;
	}

	auto AutoScaler::calculate_aggregate_metrics() const -> ServerMetrics
	{
		ServerMetrics aggregate{};
        
		if (server_metrics_.empty())
		{
			return aggregate;
		}
        
		// Calculate averages
		for (const auto& [server_id, metrics] : server_metrics_)
		{
			aggregate.cpu_usage += metrics.cpu_usage;
			aggregate.memory_usage += metrics.memory_usage;
			aggregate.active_connections += metrics.active_connections;
			aggregate.requests_per_second += metrics.requests_per_second;
			aggregate.average_latency_ms += metrics.average_latency_ms;
		}
        
		size_t server_count = server_metrics_.size();
		aggregate.cpu_usage /= server_count;
		aggregate.memory_usage /= server_count;
		aggregate.average_latency_ms /= server_count;
		aggregate.timestamp = std::chrono::steady_clock::now();
        
		return aggregate;
	}

	auto AutoScaler::should_scale_up(const ServerMetrics& metrics) const -> bool
	{
		return metrics.cpu_usage > policy_.cpu_threshold_up ||
			   metrics.memory_usage > policy_.memory_threshold_up ||
			   metrics.active_connections > policy_.connection_threshold_up;
	}

	auto AutoScaler::should_scale_down(const ServerMetrics& metrics) const -> bool
	{
		return metrics.cpu_usage < policy_.cpu_threshold_down &&
			   metrics.memory_usage < policy_.memory_threshold_down &&
			   metrics.active_connections < policy_.connection_threshold_down &&
			   current_instance_count_ > policy_.min_instances;
	}

	auto AutoScaler::is_in_cooldown() const -> bool
	{
		auto now = std::chrono::steady_clock::now();
		auto time_since_scaling = now - last_scaling_time_;
		return time_since_scaling < policy_.cooldown_period;
	}
}
