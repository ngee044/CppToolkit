#include "RegionManager.h"
#include <algorithm>
#include <cmath>

namespace GameNetwork
{
	RegionManager::RegionManager() = default;
	RegionManager::~RegionManager() = default;
    
	auto RegionManager::create_region(const std::string& name, const glm::vec3& center, const glm::vec3& size, uint32_t max_entities)
		-> std::tuple<bool, uint32_t, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		Region region;
		region.region_id = next_region_id_++;
		region.region_name = name;
		region.center = center;
		region.size = size;
		region.max_entities = max_entities;
		region.current_entities = 0;
		region.is_active = true;
		region.last_update = std::chrono::steady_clock::now();
        
		regions_[region.region_id] = region;
		update_spatial_grid(region.region_id);
        
		for (const auto& callback : region_activated_callbacks_)
		{
			callback(region);
		}
        
		return {true, region.region_id, std::nullopt};
	}
    
	auto RegionManager::remove_region(uint32_t region_id) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = regions_.find(region_id);
		if (it == regions_.end())
		{
			return {false, "Region not found"};
		}
        
		if (!it->second.entity_ids.empty())
		{
			return {false, "Region is not empty"};
		}
        
		regions_.erase(it);
		return {true, std::nullopt};
	}
    
	auto RegionManager::get_region(uint32_t region_id) const -> std::optional<Region>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = regions_.find(region_id);
		if (it != regions_.end())
		{
			return it->second;
		}
        
		return std::nullopt;
	}
    
	auto RegionManager::get_all_regions() const -> std::vector<Region>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::vector<Region> result;
		result.reserve(regions_.size());
        
		for (const auto& [id, region] : regions_)
		{
			result.push_back(region);
		}
        
		return result;
	}
    
	auto RegionManager::update_entity_position(uint64_t entity_id, const glm::vec3& position) 
		-> std::tuple<bool, std::optional<uint32_t>, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		// Find new region for position
		auto new_region_id = find_region_for_position(position);
		if (!new_region_id)
		{
			return {false, std::nullopt, "No region found for position"};
		}
        
		// Check if region is full
		if (is_region_full(*new_region_id))
		{
			return {false, std::nullopt, "Target region is full"};
		}
        
		// Get current entity position
		uint32_t old_region_id = 0;
		auto entity_it = entity_positions_.find(entity_id);
		if (entity_it != entity_positions_.end())
		{
			old_region_id = entity_it->second.current_region_id;
            
			// Remove from old region
			if (old_region_id != 0)
			{
				auto old_region_it = regions_.find(old_region_id);
				if (old_region_it != regions_.end())
				{
					old_region_it->second.entity_ids.erase(entity_id);
					old_region_it->second.current_entities--;
				}
			}
		}
        
		// Update entity position
		EntityPosition& pos = entity_positions_[entity_id];
		pos.entity_id = entity_id;
		pos.position = position;
		pos.current_region_id = *new_region_id;
		pos.last_update = std::chrono::steady_clock::now();
        
		// Add to new region
		regions_[*new_region_id].entity_ids.insert(entity_id);
		regions_[*new_region_id].current_entities++;
		regions_[*new_region_id].last_update = std::chrono::steady_clock::now();
        
		// Notify if region changed
		if (old_region_id != *new_region_id)
		{
			notify_region_change(entity_id, old_region_id, *new_region_id);
		}
        
		return {true, *new_region_id, std::nullopt};
	}
    
	auto RegionManager::remove_entity(uint64_t entity_id) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = entity_positions_.find(entity_id);
		if (it == entity_positions_.end())
		{
			return {false, "Entity not found"};
		}
        
		// Remove from region
		uint32_t region_id = it->second.current_region_id;        auto region_it = regions_.find(region_id);
		if (region_it != regions_.end())
		{
			region_it->second.entity_ids.erase(entity_id);
			region_it->second.current_entities--;
            
			// Remove player if this was a player entity
			for (auto& [player_id, entity] : player_to_entity_)
			{
				if (entity == entity_id)
				{
					region_it->second.player_ids.erase(player_id);
					break;
				}
			}
		}
        
		entity_positions_.erase(it);
		return {true, std::nullopt};
	}
    
	auto RegionManager::get_entity_region(uint64_t entity_id) const -> std::optional<uint32_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = entity_positions_.find(entity_id);
		if (it != entity_positions_.end())
		{
			return it->second.current_region_id;
		}
        
		return std::nullopt;
	}
    
	auto RegionManager::get_entities_in_region(uint32_t region_id) const -> std::vector<uint64_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = regions_.find(region_id);
		if (it != regions_.end())
		{            return std::vector<uint64_t>(it->second.entity_ids.begin(), it->second.entity_ids.end());
		}
        
		return {};
	}
    
	auto RegionManager::register_player(uint64_t player_id, uint64_t entity_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		player_to_entity_[player_id] = entity_id;
        
		// Add player to region
		auto entity_it = entity_positions_.find(entity_id);
		if (entity_it != entity_positions_.end())
		{
			uint32_t region_id = entity_it->second.current_region_id;
			auto region_it = regions_.find(region_id);
			if (region_it != regions_.end())
			{
				region_it->second.player_ids.insert(player_id);
			}
		}
	}
    
	auto RegionManager::unregister_player(uint64_t player_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = player_to_entity_.find(player_id);
		if (it != player_to_entity_.end())
		{
			// Remove player from region
			auto entity_it = entity_positions_.find(it->second);
			if (entity_it != entity_positions_.end())
			{
				uint32_t region_id = entity_it->second.current_region_id;
				auto region_it = regions_.find(region_id);
				if (region_it != regions_.end())
				{                    region_it->second.player_ids.erase(player_id);
				}
			}
            
			player_to_entity_.erase(it);
		}
	}
    
	auto RegionManager::get_players_in_region(uint32_t region_id) const -> std::vector<uint64_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = regions_.find(region_id);
		if (it != regions_.end())
		{
			return std::vector<uint64_t>(it->second.player_ids.begin(), it->second.player_ids.end());
		}
        
		return {};
	}
    
	auto RegionManager::get_neighboring_regions(uint32_t region_id) const -> std::vector<uint32_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::vector<uint32_t> neighbors;
        
		auto it = regions_.find(region_id);
		if (it == regions_.end())
		{
			return neighbors;
		}
        
		const Region& region = it->second;
        
		// Find regions that overlap or are adjacent
		for (const auto& [other_id, other_region] : regions_)
		{
			if (other_id == region_id)
				continue;                
			// Check if regions are neighbors (simplified box overlap check)
			glm::vec3 min1 = region.center - region.size * 0.5f;
			glm::vec3 max1 = region.center + region.size * 0.5f;
			glm::vec3 min2 = other_region.center - other_region.size * 0.5f;
			glm::vec3 max2 = other_region.center + other_region.size * 0.5f;
            
			// Extend bounds slightly to include adjacent regions
			const float adjacency_threshold = 1.0f;
			min1 -= glm::vec3(adjacency_threshold);
			max1 += glm::vec3(adjacency_threshold);
            
			if (min1.x <= max2.x && max1.x >= min2.x &&
				min1.y <= max2.y && max1.y >= min2.y &&
				min1.z <= max2.z && max1.z >= min2.z)
			{
				neighbors.push_back(other_id);
			}
		}
        
		return neighbors;
	}
    
	auto RegionManager::get_entities_in_radius(const glm::vec3& position, float radius) const -> std::vector<uint64_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::vector<uint64_t> result;
		float radius_squared = radius * radius;
        
		for (const auto& [entity_id, entity_pos] : entity_positions_)
		{
			glm::vec3 diff = position - entity_pos.position;
			float dist_squared = glm::dot(diff, diff);
			if (dist_squared <= radius_squared)
			{
				result.push_back(entity_id);
			}
		}        
		return result;
	}
    
	auto RegionManager::get_visible_entities(uint64_t entity_id, float view_distance) const -> std::vector<uint64_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = entity_positions_.find(entity_id);
		if (it != entity_positions_.end())
		{
			return get_entities_in_radius(it->second.position, view_distance);
		}
        
		return {};
	}
    
	auto RegionManager::set_region_active(uint32_t region_id, bool active) 
		-> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = regions_.find(region_id);
		if (it == regions_.end())
		{
			return {false, "Region not found"};
		}
        
		if (it->second.is_active != active)
		{
			it->second.is_active = active;
            
			if (active)
			{
				for (const auto& callback : region_activated_callbacks_)
				{
					callback(it->second);
				}
			}
			else            {
				for (const auto& callback : region_deactivated_callbacks_)
				{
					callback(it->second);
				}
			}
		}
        
		return {true, std::nullopt};
	}
    
	auto RegionManager::is_region_full(uint32_t region_id) const -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = regions_.find(region_id);
		if (it != regions_.end())
		{
			return it->second.current_entities >= it->second.max_entities;
		}
        
		return true; // Treat non-existent regions as full
	}
    
	auto RegionManager::get_region_load(uint32_t region_id) const -> float
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = regions_.find(region_id);
		if (it != regions_.end() && it->second.max_entities > 0)
		{
			return static_cast<float>(it->second.current_entities) / it->second.max_entities;
		}
        
		return 0.0f;
	}
    
	auto RegionManager::on_entity_region_changed(RegionChangeCallback callback) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);        region_change_callbacks_.push_back(callback);
	}
    
	auto RegionManager::on_region_activated(RegionEventCallback callback) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		region_activated_callbacks_.push_back(callback);
	}
    
	auto RegionManager::on_region_deactivated(RegionEventCallback callback) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		region_deactivated_callbacks_.push_back(callback);
	}
    
	auto RegionManager::get_least_loaded_region() const -> std::optional<uint32_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		std::optional<uint32_t> result;
		float min_load = 1.0f;
        
		for (const auto& [id, region] : regions_)
		{
			if (!region.is_active)
				continue;
                
			float load = get_region_load(id);
			if (load < min_load)
			{
				min_load = load;
				result = id;
			}
		}
        
		return result;
	}
    
	auto RegionManager::balance_regions() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		// Simple load balancing: move entities from overloaded regions to underloaded ones
		std::vector<std::pair<uint32_t, float>> region_loads;
        
		for (const auto& [id, region] : regions_)
		{
			if (region.is_active)
			{
				region_loads.emplace_back(id, get_region_load(id));
			}
		}
        
		// Sort by load
		std::sort(region_loads.begin(), region_loads.end(),
				[](const auto& a, const auto& b) { return a.second > b.second; });
        
		// Implement entity migration logic
		const float HIGH_LOAD_THRESHOLD = 0.8f;
		const float LOW_LOAD_THRESHOLD = 0.3f;
        
		// Find overloaded and underloaded regions
		std::vector<uint32_t> overloaded_regions;
		std::vector<uint32_t> underloaded_regions;
        
		for (const auto& [region_id, load] : region_loads)
		{
			if (load > HIGH_LOAD_THRESHOLD)
			{
				overloaded_regions.push_back(region_id);
			}
			else if (load < LOW_LOAD_THRESHOLD)
			{
				underloaded_regions.push_back(region_id);
			}
		}
        
		for (uint32_t overloaded_id : overloaded_regions)
		{
			if (underloaded_regions.empty())
				break;
                
			auto& overloaded_region = regions_[overloaded_id];
			auto entities_to_migrate = overloaded_region.entity_ids;
            
			for (uint64_t entity_id : entities_to_migrate)
			{
				auto entity_it = entity_positions_.find(entity_id);
				if (entity_it == entity_positions_.end())
					continue;
                    
				const auto& entity_pos = entity_it->second.position;
                
				auto neighbors = get_neighboring_regions(overloaded_id);
				for (uint32_t neighbor_id : neighbors)
				{
					auto neighbor_it = std::find(underloaded_regions.begin(), 
												underloaded_regions.end(), 
												neighbor_id);
					if (neighbor_it != underloaded_regions.end())
					{
						if (is_position_in_region(entity_pos, regions_[neighbor_id]))
						{
							notify_region_change(entity_id, overloaded_id, neighbor_id);
							break;
						}
					}
				}
                
				if (get_region_load(overloaded_id) < HIGH_LOAD_THRESHOLD)
					break;
			}
		}
	}
    
	auto RegionManager::get_total_entities() const -> size_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return entity_positions_.size();
	}
    
	auto RegionManager::get_active_regions() const -> size_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		return std::count_if(regions_.begin(), regions_.end(),
							[](const auto& pair) { return pair.second.is_active; });
	}
    
	auto RegionManager::find_region_for_position(const glm::vec3& position) const -> std::optional<uint32_t>
	{
		for (const auto& [id, region] : regions_)
		{
			if (region.is_active && is_position_in_region(position, region))            {
				return id;
			}
		}
        
		return std::nullopt;
	}
    
	auto RegionManager::is_position_in_region(const glm::vec3& position, const Region& region) const -> bool
	{
		glm::vec3 min = region.center - region.size * 0.5f;
		glm::vec3 max = region.center + region.size * 0.5f;
        
		return position.x >= min.x && position.x <= max.x &&
				position.y >= min.y && position.y <= max.y &&
				position.z >= min.z && position.z <= max.z;
	}
    
	auto RegionManager::get_grid_key(const glm::vec3& position) const -> int32_t
	{
		int32_t grid_x = static_cast<int32_t>(std::floor(position.x / GRID_SIZE));
		int32_t grid_y = static_cast<int32_t>(std::floor(position.y / GRID_SIZE));
		int32_t grid_z = static_cast<int32_t>(std::floor(position.z / GRID_SIZE));
        
		return (grid_x * 73856093) ^ (grid_y * 19349663) ^ (grid_z * 83492791);
	}
    
	auto RegionManager::update_spatial_grid(uint32_t region_id) -> void
	{
		auto it = regions_.find(region_id);
		if (it == regions_.end())
			return;
            
		const Region& region = it->second;
        
		glm::vec3 min_bounds = region.center - region.size * 0.5f;
		glm::vec3 max_bounds = region.center + region.size * 0.5f;
        
		for (float x = min_bounds.x; x <= max_bounds.x; x += GRID_SIZE)
		{
			for (float y = min_bounds.y; y <= max_bounds.y; y += GRID_SIZE)
			{
				for (float z = min_bounds.z; z <= max_bounds.z; z += GRID_SIZE)
				{
					int32_t key = get_grid_key(glm::vec3(x, y, z));
					spatial_grid_[key].insert(region_id);
				}
			}
		}
	}
    
	auto RegionManager::notify_region_change(uint64_t entity_id, uint32_t old_region, uint32_t new_region) -> void
	{
		for (const auto& callback : region_change_callbacks_)
		{
			callback(entity_id, old_region, new_region);
		}
	}
    
} // namespace GameNetwork
