#include "GeographicLoadBalancer.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Utilities;

namespace GameNetwork
{
	GeographicLoadBalancer::GeographicLoadBalancer()
	{
		Logger::handle().write(LogTypes::Information, "GeographicLoadBalancer initialized");
	}

	auto GeographicLoadBalancer::register_server(const ServerGeoInfo& server_info) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		servers_[server_info.server_id] = server_info;

		Logger::handle().write(LogTypes::Information,
			fmt::format("Registered server {} in {}, {}", server_info.server_id,
			server_info.location.region, server_info.location.country));
	}

	auto GeographicLoadBalancer::unregister_server(const std::string& server_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		servers_.erase(server_id);

		Logger::handle().write(LogTypes::Information, fmt::format("Unregistered server {}", server_id));
	}

	auto GeographicLoadBalancer::update_server_load(const std::string& server_id, float load) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = servers_.find(server_id);
		if (it != servers_.end())
		{
			it->second.current_load = load;
		}
	}

	auto GeographicLoadBalancer::select_nearest_server(const ClientGeoInfo& client_info) -> std::optional<std::string>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::string best_server;
		double min_distance = std::numeric_limits<double>::max();
        
		for (const auto& [server_id, server_info] : servers_)
		{
			if (!server_info.is_available) continue;
            
			double distance = calculate_distance(client_info.location, server_info.location);
            
			if (distance < min_distance && distance <= max_distance_km_)
			{
				min_distance = distance;
				best_server = server_id;
			}
		}
        
		if (best_server.empty() && enable_distant_failover_)
		{
			// Fallback to any available server
			for (const auto& [server_id, server_info] : servers_)
			{
				if (server_info.is_available)
				{
					return server_id;
				}
			}
		}
        
		return best_server.empty() ? std::nullopt : std::make_optional(best_server);
	}

	auto GeographicLoadBalancer::select_optimal_server(const ClientGeoInfo& client_info, float distance_weight, float load_weight) -> std::optional<std::string>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::string best_server;
		float best_score = std::numeric_limits<float>::max();
        
		for (const auto& [server_id, server_info] : servers_)
		{
			if (!server_info.is_available) continue;
            
			float score = calculate_server_score(server_info, client_info, distance_weight, load_weight);
            
			if (score < best_score)
			{
				best_score = score;
				best_server = server_id;
			}
		}
        
		if (!best_server.empty())
		{
			Logger::handle().write(LogTypes::Debug, fmt::format("Selected server {} with score: {}", best_server, best_score));
		}
        
		return best_server.empty() ? std::nullopt : std::make_optional(best_server);
	}

	auto GeographicLoadBalancer::get_servers_in_region(const std::string& region) -> std::vector<std::string>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::vector<std::string> region_servers;
		for (const auto& [server_id, server_info] : servers_)
		{
			if (server_info.location.region == region && server_info.is_available)
			{
				region_servers.push_back(server_id);
			}
		}
        
		return region_servers;
	}

	auto GeographicLoadBalancer::set_region_preference(const std::string& client_region, const std::vector<std::string>& preferred_regions) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		region_preferences_[client_region] = preferred_regions;
	}

	auto GeographicLoadBalancer::calculate_distance(const GeoLocation& loc1, const GeoLocation& loc2) -> double
	{
		return calculate_haversine_distance(loc1.latitude, loc1.longitude, loc2.latitude, loc2.longitude);
	}

	auto GeographicLoadBalancer::estimate_latency(const GeoLocation& client_loc, const GeoLocation& server_loc) -> float
	{
		double distance_km = calculate_distance(client_loc, server_loc);
        
		// Estimate latency based on distance and speed of light in fiber
		double one_way_latency = (distance_km / SPEED_OF_LIGHT_KM_MS) + LATENCY_OVERHEAD_MS;
        
		return static_cast<float>(one_way_latency * 2); // Round trip time
	}

	auto GeographicLoadBalancer::set_max_distance_km(double max_distance) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		max_distance_km_ = max_distance;
	}

	auto GeographicLoadBalancer::enable_failover_to_distant_servers(bool enable) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		enable_distant_failover_ = enable;
	}

	auto GeographicLoadBalancer::get_region_statistics() -> std::unordered_map<std::string, size_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::unordered_map<std::string, size_t> stats;
		for (const auto& [server_id, server_info] : servers_)
		{
			stats[server_info.location.region]++;
		}
        
		return stats;
	}

	auto GeographicLoadBalancer::get_average_distance_per_region() -> std::unordered_map<std::string, double>
	{
		// This would require client connection history to be meaningful
		// For now, return empty map
		return {};
	}

	auto GeographicLoadBalancer::calculate_haversine_distance(double lat1, double lon1, double lat2, double lon2) -> double
	{
		const double R = 6371.0; // Earth radius in kilometers
        
		// Convert degrees to radians
		lat1 *= M_PI / 180.0;
		lon1 *= M_PI / 180.0;
		lat2 *= M_PI / 180.0;
		lon2 *= M_PI / 180.0;
        
		double dlat = lat2 - lat1;
		double dlon = lon2 - lon1;
        
		double a = std::sin(dlat/2) * std::sin(dlat/2) + 
				   std::cos(lat1) * std::cos(lat2) * 
				   std::sin(dlon/2) * std::sin(dlon/2);
        
		double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));
        
		return R * c; // Distance in kilometers
	}

	auto GeographicLoadBalancer::calculate_server_score(const ServerGeoInfo& server, const ClientGeoInfo& client,
														float distance_weight, float load_weight) -> float
	{
		double distance = calculate_distance(client.location, server.location);
		float normalized_distance = static_cast<float>(distance / max_distance_km_);
        
		float normalized_load = server.current_load / static_cast<float>(server.capacity);
        
		// Apply latency factor
		normalized_distance *= server.latency_factor;
        
		// Lower score is better
		return (distance_weight * normalized_distance) + (load_weight * normalized_load);
	}
}
