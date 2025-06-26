#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <tuple>
#include <optional>
#include <functional>
#include <chrono>
#include <glm/glm.hpp>

namespace GameNetwork
{
    struct Region
    {
        uint32_t region_id;
        std::string region_name;
        glm::vec3 center;
        glm::vec3 size;
        uint32_t max_entities;
        uint32_t current_entities;
        std::unordered_set<uint64_t> entity_ids;
        std::unordered_set<uint64_t> player_ids;
        bool is_active;
        std::chrono::steady_clock::time_point last_update;
    };
    
    struct EntityPosition
    {
        uint64_t entity_id;
        glm::vec3 position;
        uint32_t current_region_id;
        std::chrono::steady_clock::time_point last_update;
    };
    
    class RegionManager
    {
    public:
        using RegionChangeCallback = std::function<void(uint64_t entity_id, uint32_t old_region, uint32_t new_region)>;
        using RegionEventCallback = std::function<void(const Region& region)>;
        
        RegionManager();
        ~RegionManager();
        
        // Region management
        auto create_region(const std::string& name, const glm::vec3& center, const glm::vec3& size, uint32_t max_entities) 
            -> std::tuple<bool, uint32_t, std::optional<std::string>>;
        auto remove_region(uint32_t region_id) -> std::tuple<bool, std::optional<std::string>>;
        auto get_region(uint32_t region_id) const -> std::optional<Region>;
        auto get_all_regions() const -> std::vector<Region>;
        
        // Entity management
        auto update_entity_position(uint64_t entity_id, const glm::vec3& position) 
            -> std::tuple<bool, std::optional<uint32_t>, std::optional<std::string>>;
        auto remove_entity(uint64_t entity_id) -> std::tuple<bool, std::optional<std::string>>;
        auto get_entity_region(uint64_t entity_id) const -> std::optional<uint32_t>;
        auto get_entities_in_region(uint32_t region_id) const -> std::vector<uint64_t>;
        
        // Player management
        auto register_player(uint64_t player_id, uint64_t entity_id) -> void;
        auto unregister_player(uint64_t player_id) -> void;
        auto get_players_in_region(uint32_t region_id) const -> std::vector<uint64_t>;
        
        // Neighbor queries
        auto get_neighboring_regions(uint32_t region_id) const -> std::vector<uint32_t>;
        auto get_entities_in_radius(const glm::vec3& position, float radius) const -> std::vector<uint64_t>;
        auto get_visible_entities(uint64_t entity_id, float view_distance) const -> std::vector<uint64_t>;
        
        // Region properties
        auto set_region_active(uint32_t region_id, bool active) -> std::tuple<bool, std::optional<std::string>>;
        auto is_region_full(uint32_t region_id) const -> bool;
        auto get_region_load(uint32_t region_id) const -> float;
        
        // Callbacks
        auto on_entity_region_changed(RegionChangeCallback callback) -> void;
        auto on_region_activated(RegionEventCallback callback) -> void;
        auto on_region_deactivated(RegionEventCallback callback) -> void;
        
        // Load balancing
        auto get_least_loaded_region() const -> std::optional<uint32_t>;
        auto balance_regions() -> void;
        
        // Statistics
        auto get_total_entities() const -> size_t;
        auto get_active_regions() const -> size_t;
        
    private:
        mutable std::mutex mutex_;
        
        // Region data
        std::unordered_map<uint32_t, Region> regions_;
        uint32_t next_region_id_{1};
        
        // Entity tracking
        std::unordered_map<uint64_t, EntityPosition> entity_positions_;
        std::unordered_map<uint64_t, uint64_t> player_to_entity_;
        
        // Spatial indexing
        static constexpr float GRID_SIZE = 100.0f;
        std::unordered_map<int32_t, std::unordered_set<uint32_t>> spatial_grid_;
        
        // Callbacks
        std::vector<RegionChangeCallback> region_change_callbacks_;
        std::vector<RegionEventCallback> region_activated_callbacks_;
        std::vector<RegionEventCallback> region_deactivated_callbacks_;
        
        // Helper functions
        auto find_region_for_position(const glm::vec3& position) const -> std::optional<uint32_t>;
        auto is_position_in_region(const glm::vec3& position, const Region& region) const -> bool;
        auto get_grid_key(const glm::vec3& position) const -> int32_t;
        auto update_spatial_grid(uint32_t region_id) -> void;
        auto notify_region_change(uint64_t entity_id, uint32_t old_region, uint32_t new_region) -> void;
    };
} // namespace GameNetwork
