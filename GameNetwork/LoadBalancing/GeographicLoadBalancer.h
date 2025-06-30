#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cmath>
#include <optional>

namespace GameNetwork
{
	struct GeoLocation
	{
		double latitude;
		double longitude;
		std::string region;
		std::string country;
		std::string city;
	};

	struct ServerGeoInfo
	{
		std::string server_id;
		GeoLocation location;
		float current_load;
		uint32_t capacity;
		bool is_available;
		float latency_factor; // Multiplier for distance-based routing
	};

	struct ClientGeoInfo
	{
		GeoLocation location;
		std::string ip_address;
		std::string detected_region;
	};

	class GeographicLoadBalancer
	{
	public:
		GeographicLoadBalancer();
		~GeographicLoadBalancer() = default;

		auto register_server(const ServerGeoInfo& server_info) -> void;
		auto unregister_server(const std::string& server_id) -> void;
		auto update_server_load(const std::string& server_id, float load) -> void;

		auto select_nearest_server(const ClientGeoInfo& client_info) -> std::optional<std::string>;
		auto select_optimal_server(const ClientGeoInfo& client_info, float distance_weight = 0.6f, float load_weight = 0.4f) -> std::optional<std::string>;

		auto get_servers_in_region(const std::string& region) -> std::vector<std::string>;
		auto set_region_preference(const std::string& client_region, const std::vector<std::string>& preferred_regions) -> void;

		auto calculate_distance(const GeoLocation& loc1, const GeoLocation& loc2) -> double;
		auto estimate_latency(const GeoLocation& client_loc, const GeoLocation& server_loc) -> float;

		auto set_max_distance_km(double max_distance) -> void;
		auto enable_failover_to_distant_servers(bool enable) -> void;

		auto get_region_statistics() -> std::unordered_map<std::string, size_t>;
		auto get_average_distance_per_region() -> std::unordered_map<std::string, double>;

	private:
		auto calculate_haversine_distance(double lat1, double lon1, double lat2, double lon2) -> double;
		auto calculate_server_score(const ServerGeoInfo& server, const ClientGeoInfo& client, float distance_weight, float load_weight) -> float;

	private:
		mutable std::mutex mutex_;
		std::unordered_map<std::string, ServerGeoInfo> servers_;
		std::unordered_map<std::string, std::vector<std::string>> region_preferences_;
        
		double max_distance_km_{10000.0}; 
		bool enable_distant_failover_{true};
        
		static constexpr double SPEED_OF_LIGHT_KM_MS = 300.0; // km/ms in fiber
		static constexpr double LATENCY_OVERHEAD_MS = 10.0;   // Processing overhead
	};
}
