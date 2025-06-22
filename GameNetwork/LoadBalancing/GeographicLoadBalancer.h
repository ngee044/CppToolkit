#pragma once

#include <LoadBalancer.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <optional>
#include <cmath>

namespace GameNetwork
{
    namespace LoadBalancing
    {
        struct ClientLocation
        {
            std::string ip_address;
            std::string country_code;
            std::string region;
            float latitude;
            float longitude;
            std::string isp;
        };

        struct ServerLocation
        {
            std::string server_id;
            std::string region;
            std::string datacenter;
            float latitude;
            float longitude;
            std::vector<std::string> cdn_endpoints;
            uint32_t max_capacity;
            std::vector<std::string> supported_regions;
        };

        struct LatencyMap
        {
            std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> region_latencies;
            std::chrono::steady_clock::time_point last_update;
        };

        class GeographicLoadBalancer : public LoadBalancer
        {
        public:
            GeographicLoadBalancer();
            virtual ~GeographicLoadBalancer() = default;
            // Server location management
            auto register_server_location(const ServerLocation& location) 
                -> std::tuple<bool, std::optional<std::string>>;
            auto update_server_location(const std::string& server_id, 
                                       const ServerLocation& location) 
                -> std::tuple<bool, std::optional<std::string>>;

            // Client routing
            auto find_nearest_server(const ClientLocation& client) -> std::string;
            auto find_best_region(const ClientLocation& client) -> std::string;
            auto get_servers_in_region(const std::string& region) -> std::vector<std::string>;

            // Latency calculation
            auto calculate_network_latency(const std::string& client_ip, 
                                         const ServerLocation& server) -> uint32_t;
            auto estimate_geographic_latency(const ClientLocation& client,
                                           const ServerLocation& server) -> uint32_t;
            auto update_latency_map(const std::string& from_region,
                                   const std::string& to_region,
                                   uint32_t latency_ms) -> void;

            // Routing optimization
            auto route_by_geographic_proximity() -> std::string;
            auto route_with_latency_consideration(const ClientLocation& client) 
                -> std::tuple<std::string, uint32_t>; // server_id, expected_latency

            // CDN integration
            auto get_nearest_cdn_endpoint(const ClientLocation& client) -> std::string;
            auto should_use_cdn_for_content(const std::string& content_type) -> bool;

            // Multi-region support
            auto enable_cross_region_play(bool enable) -> void;
            auto get_region_player_counts() -> std::unordered_map<std::string, uint32_t>;
            auto balance_across_regions() -> void;
            // IP geolocation
            auto lookup_client_location(const std::string& ip_address) 
                -> std::optional<ClientLocation>;
            auto update_geolocation_database() -> std::tuple<bool, std::optional<std::string>>;

        private:
            // Geographic calculations
            auto calculate_distance(float lat1, float lon1, float lat2, float lon2) -> float;
            auto estimate_latency_from_distance(float distance_km) -> uint32_t;
            
            // Server selection algorithms
            auto select_by_proximity(const ClientLocation& client,
                                   const std::vector<ServerLocation>& servers) -> std::string;
            auto select_by_latency(const ClientLocation& client,
                                 const std::vector<ServerLocation>& servers) -> std::string;
            auto select_by_load_and_proximity(const ClientLocation& client) -> std::string;

        private:
            // Server locations
            std::unordered_map<std::string, ServerLocation> server_locations_;
            
            // Region mapping
            std::unordered_map<std::string, std::vector<std::string>> region_servers_;
            
            // Latency data
            LatencyMap latency_map_;
            
            // CDN endpoints
            std::vector<std::pair<ServerLocation, std::string>> cdn_endpoints_;
            
            // Configuration
            bool cross_region_enabled_{false};
            uint32_t max_acceptable_latency_ms_{150};
            float proximity_weight_{0.7f};
            float load_weight_{0.3f};
            
            mutable std::mutex geo_mutex_;
        };
    }
}