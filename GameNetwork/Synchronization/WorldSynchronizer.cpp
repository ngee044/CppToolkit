#include "WorldSynchronizer.h"
#include "../Packet/GamePacket.h"

#include <cmath>
#include <algorithm>

namespace GameNetwork
{
    WorldSynchronizer::WorldSynchronizer()
        : view_distance_(100.0f)
        , grid_cell_size_(50.0f)
        , lod_high_distance_(30.0f)
        , lod_medium_distance_(60.0f)
        , lod_low_distance_(90.0f)
        , sync_running_(false)
        , sync_interval_(std::chrono::milliseconds(100))
    {
        stats_ = {};
    }
    
    WorldSynchronizer::~WorldSynchronizer()
    {
        stop_sync_timer();
    }
    
    auto WorldSynchronizer::set_view_distance(float distance) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        view_distance_ = distance;
    }
    
    auto WorldSynchronizer::get_view_distance() const -> float
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return view_distance_;
    }
    
    auto WorldSynchronizer::enter_area(std::shared_ptr<GameSession> session, const Location& location) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!session)
        {
            return {false, "Invalid session"};
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto session_id = session->session_id();
        
        // Remove from old grid cell if exists
        auto it = sessions_.find(session_id);
        if (it != sessions_.end())
        {
            auto old_key = get_grid_key(it->second.location);
            grid_[old_key].session_ids.erase(session_id);
        }
        
        // Add to new grid cell
        auto grid_key = get_grid_key(location);
        grid_[grid_key].session_ids.insert(session_id);
        
        // Update session info
        SessionInfo info;
        info.session = session;
        info.location = location;
        info.last_sync = std::chrono::steady_clock::now();
        sessions_[session_id] = info;
        
        // Get visible entities
        auto visible_entities = get_visible_entities(session);
        for (const auto& entity : visible_entities)
        {
            sessions_[session_id].visible_entities.insert(entity.id);
        }
        
        stats_.sessions_tracked++;
        
        return {true, std::nullopt};
    }
    
    auto WorldSynchronizer::leave_area(std::shared_ptr<GameSession> session) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!session)
        {
            return {false, "Invalid session"};
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto session_id = session->session_id();
        auto it = sessions_.find(session_id);
        if (it == sessions_.end())
        {
            return {false, "Session not in area"};
        }
        
        // Remove from grid
        auto grid_key = get_grid_key(it->second.location);
        grid_[grid_key].session_ids.erase(session_id);
        
        // Remove session info
        sessions_.erase(it);
        stats_.sessions_tracked--;
        
        return {true, std::nullopt};
    }
    
    auto WorldSynchronizer::update_position(std::shared_ptr<GameSession> session, const Location& location) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!session)
        {
            return {false, "Invalid session"};
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto session_id = session->session_id();
        auto it = sessions_.find(session_id);
        if (it == sessions_.end())
        {
            return {false, "Session not in area"};
        }
        
        // Check if grid cell changed
        auto old_key = get_grid_key(it->second.location);
        auto new_key = get_grid_key(location);
        
        if (old_key != new_key)
        {
            // Move to new grid cell
            grid_[old_key].session_ids.erase(session_id);
            grid_[new_key].session_ids.insert(session_id);
        }
        
        // Update location
        it->second.location = location;
        
        // Mark for sync
        sync_session(it->second);
        
        return {true, std::nullopt};
    }
    
    auto WorldSynchronizer::spawn_entity(const Entity& entity) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Add entity
        entities_[entity.id] = entity;
        
        // Add to grid
        auto grid_key = get_grid_key(entity.location);
        grid_[grid_key].entity_ids.insert(entity.id);
        
        stats_.entities_tracked++;
        
        // Notify nearby sessions
        auto interested = get_interested_sessions(entity.location, view_distance_);
        for (const auto& session : interested)
        {
            // Send spawn packet
            auto spawn_packet = std::make_unique<EntitySpawnPacket>();
            spawn_packet->set_entity_id(entity.id);
            spawn_packet->set_entity_type(entity.type);
            spawn_packet->set_location(entity.location);
            spawn_packet->set_name(entity.name);
            spawn_packet->set_level(entity.level);
            spawn_packet->set_health(entity.health);
            spawn_packet->set_max_health(entity.max_health);
            
            // Send packet to session
            if (session->connection())
            {
                session->connection()->send_packet(*spawn_packet);
            }
        }
        
        return {true, std::nullopt};
    }
    
    auto WorldSynchronizer::despawn_entity(uint64_t entity_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = entities_.find(entity_id);
        if (it == entities_.end())
        {
            return {false, "Entity not found"};
        }
        
        // Remove from grid
        auto grid_key = get_grid_key(it->second.location);
        grid_[grid_key].entity_ids.erase(entity_id);
        
        // Notify nearby sessions
        auto interested = get_interested_sessions(it->second.location, view_distance_);
        for (const auto& session : interested)
        {
            // Send despawn packet
            auto despawn_packet = std::make_unique<EntityDespawnPacket>();
            despawn_packet->set_entity_id(entity_id);
            despawn_packet->set_reason(DespawnReason::OutOfRange);
            
            // Send packet to session
            if (session->connection())
            {
                session->connection()->send_packet(*despawn_packet);
            }
        }
        
        // Remove entity
        entities_.erase(it);
        stats_.entities_tracked--;
        
        return {true, std::nullopt};
    }
    
    auto WorldSynchronizer::get_interested_sessions(const Location& location, float radius) const 
        -> std::vector<std::shared_ptr<GameSession>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::shared_ptr<GameSession>> result;
        
        float search_radius = (radius == 0.0f) ? view_distance_ : radius;
        auto nearby_cells = get_nearby_cells(location, search_radius);
        
        for (const auto& cell_key : nearby_cells)
        {
            auto grid_it = grid_.find(cell_key);
            if (grid_it != grid_.end())
            {
                for (const auto& session_id : grid_it->second.session_ids)
                {
                    auto session_it = sessions_.find(session_id);
                    if (session_it != sessions_.end())
                    {
                        // Check actual distance
                        float dx = session_it->second.location.x - location.x;
                        float dy = session_it->second.location.y - location.y;
                        float dz = session_it->second.location.z - location.z;
                        float dist_sq = dx*dx + dy*dy + dz*dz;
                        
                        if (dist_sq <= search_radius * search_radius)
                        {
                            result.push_back(session_it->second.session);
                        }
                    }
                }
            }
        }
        
        return result;
    }
    
    auto WorldSynchronizer::get_visible_entities(std::shared_ptr<GameSession> session) const 
        -> std::vector<Entity>
    {
        if (!session)
        {
            return {};
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Entity> result;
        
        auto session_it = sessions_.find(session->session_id());
        if (session_it == sessions_.end())
        {
            return result;
        }
        
        const auto& session_location = session_it->second.location;
        auto nearby_cells = get_nearby_cells(session_location, view_distance_);
        
        for (const auto& cell_key : nearby_cells)
        {
            auto grid_it = grid_.find(cell_key);
            if (grid_it != grid_.end())
            {
                for (const auto& entity_id : grid_it->second.entity_ids)
                {
                    auto entity_it = entities_.find(entity_id);
                    if (entity_it != entities_.end())
                    {
                        // Check actual distance
                        float dx = entity_it->second.location.x - session_location.x;
                        float dy = entity_it->second.location.y - session_location.y;
                        float dz = entity_it->second.location.z - session_location.z;
                        float dist_sq = dx*dx + dy*dy + dz*dz;
                        
                        if (dist_sq <= view_distance_ * view_distance_)
                        {
                            result.push_back(entity_it->second);
                        }
                    }
                }
            }
        }
        
        return result;
    }
    
    auto WorldSynchronizer::broadcast_to_area(const Location& center, 
                                               float radius, 
                                               const GamePacket& packet, 
                                               std::shared_ptr<GameSession> exclude) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto interested = get_interested_sessions(center, radius);
        
        for (const auto& session : interested)
        {
            if (session != exclude)
            {
                auto connection = session->current_connection();
                if (connection)
                {
                    connection->send_packet(packet);
                    stats_.packets_broadcasted++;
                }
            }
        }
        
        return {true, std::nullopt};
    }
    
    auto WorldSynchronizer::get_channel_sessions(uint32_t channel_id) const 
        -> std::vector<std::shared_ptr<GameSession>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::shared_ptr<GameSession>> result;
        
        for (const auto& [id, info] : sessions_)
        {
            if (info.location.channel_id == channel_id)
            {
                result.push_back(info.session);
            }
        }
        
        return result;
    }
    
    auto WorldSynchronizer::broadcast_to_channel(uint32_t channel_id, 
                                                  const GamePacket& packet, 
                                                  std::shared_ptr<GameSession> exclude) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto channel_sessions = get_channel_sessions(channel_id);
        
        for (const auto& session : channel_sessions)
        {
            if (session != exclude)
            {
                auto connection = session->current_connection();
                if (connection)
                {
                    connection->send_packet(packet);
                    stats_.packets_broadcasted++;
                }
            }
        }
        
        return {true, std::nullopt};
    }
    
    auto WorldSynchronizer::start_sync_timer() -> void
    {
        sync_running_ = true;
        
        sync_timer_ = std::async(std::launch::async, [this]()
        {
            while (sync_running_)
            {
                std::this_thread::sleep_for(sync_interval_);
                
                auto start_time = std::chrono::steady_clock::now();
                sync_all_sessions();
                auto end_time = std::chrono::steady_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time);
                
                // Update average sync time
                if (stats_.total_syncs > 0)
                {
                    stats_.average_sync_time = std::chrono::microseconds(
                        (stats_.average_sync_time.count() * (stats_.total_syncs - 1) + 
                         duration.count()) / stats_.total_syncs);
                }
                else
                {
                    stats_.average_sync_time = duration;
                }
            }
        });
    }
    
    auto WorldSynchronizer::stop_sync_timer() -> void
    {
        sync_running_ = false;
        
        if (sync_timer_.valid())
        {
            sync_timer_.wait();
        }
    }
    
    auto WorldSynchronizer::force_sync() -> void
    {
        sync_all_sessions();
    }
    
    auto WorldSynchronizer::calculate_detail_level(float distance) const -> DetailLevel
    {
        if (distance <= lod_high_distance_)
        {
            return DetailLevel::Full;
        }
        else if (distance <= lod_medium_distance_)
        {
            return DetailLevel::High;
        }
        else if (distance <= lod_low_distance_)
        {
            return DetailLevel::Medium;
        }
        else
        {
            return DetailLevel::Low;
        }
    }
    
    auto WorldSynchronizer::set_lod_distances(float high, float medium, float low) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lod_high_distance_ = high;
        lod_medium_distance_ = medium;
        lod_low_distance_ = low;
    }
    
    auto WorldSynchronizer::get_stats() const -> SyncStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    auto WorldSynchronizer::reset_stats() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = {};
    }
    
    auto WorldSynchronizer::get_grid_key(const Location& location) const -> std::pair<int32_t, int32_t>
    {
        int32_t grid_x = static_cast<int32_t>(std::floor(location.x / grid_cell_size_));
        int32_t grid_z = static_cast<int32_t>(std::floor(location.z / grid_cell_size_));
        return {grid_x, grid_z};
    }
    
    auto WorldSynchronizer::get_nearby_cells(const Location& location, float radius) const 
        -> std::vector<std::pair<int32_t, int32_t>>
    {
        std::vector<std::pair<int32_t, int32_t>> result;
        
        int32_t cell_radius = static_cast<int32_t>(std::ceil(radius / grid_cell_size_));
        auto center_key = get_grid_key(location);
        
        for (int32_t dx = -cell_radius; dx <= cell_radius; ++dx)
        {
            for (int32_t dz = -cell_radius; dz <= cell_radius; ++dz)
            {
                result.push_back({center_key.first + dx, center_key.second + dz});
            }
        }
        
        return result;
    }
    
    auto WorldSynchronizer::sync_session(SessionInfo& info) -> void
    {
        // Get current visible entities
        auto current_visible = get_visible_entities(info.session);
        std::unordered_set<uint64_t> current_ids;
        
        for (const auto& entity : current_visible)
        {
            current_ids.insert(entity.id);
            
            // Check if newly visible
            if (info.visible_entities.find(entity.id) == info.visible_entities.end())
            {
                // Send spawn packet for this entity
                auto spawn_packet = std::make_unique<EntitySpawnPacket>();
                spawn_packet->set_entity_id(entity.id);
                spawn_packet->set_entity_type(entity.type);
                spawn_packet->set_location(entity.location);
                spawn_packet->set_name(entity.name);
                spawn_packet->set_level(entity.level);
                spawn_packet->set_health(entity.health);
                spawn_packet->set_max_health(entity.max_health);
                
                if (info.session->connection())
                {
                    info.session->connection()->send_packet(*spawn_packet);
                }
            }
        }
        
        // Check for entities that are no longer visible
        std::vector<uint64_t> to_remove;
        for (const auto& entity_id : info.visible_entities)
        {
            if (current_ids.find(entity_id) == current_ids.end())
            {
                to_remove.push_back(entity_id);
                // Send despawn packet for this entity
                auto despawn_packet = std::make_unique<EntityDespawnPacket>();
                despawn_packet->set_entity_id(entity_id);
                despawn_packet->set_reason(DespawnReason::OutOfRange);
                
                if (info.session->connection())
                {
                    info.session->connection()->send_packet(*despawn_packet);
                }
            }
        }
        
        // Update visible entities
        info.visible_entities = current_ids;
        
        // Send update packets for visible entities based on LOD
        for (const auto& entity : current_visible)
        {
            // Calculate distance for LOD
            float distance = calculate_distance(info.session->location(), entity.location);
            
            // Create update packet with appropriate detail level
            auto update_packet = std::make_unique<EntityUpdatePacket>();
            update_packet->set_entity_id(entity.id);
            
            // Always send location for visible entities
            update_packet->set_location(entity.location);
            
            // Send health if close enough
            if (distance < view_distance_ * 0.5f)
            {
                update_packet->set_health(entity.health);
                update_packet->set_state(entity.state);
            }
            
            // Send velocity for moving entities if very close
            if (distance < view_distance_ * 0.25f && entity.velocity.has_value())
            {
                update_packet->set_velocity(entity.velocity.value());
            }
            
            if (info.session->connection())
            {
                info.session->connection()->send_packet(*update_packet);
            }
        }
        
        info.last_sync = std::chrono::steady_clock::now();
    }
    
    auto WorldSynchronizer::sync_all_sessions() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [id, info] : sessions_)
        {
            sync_session(info);
        }
        
        stats_.total_syncs++;
    }
    
    auto WorldSynchronizer::calculate_distance(const Location& loc1, const Location& loc2) -> float
    {
        float dx = loc1.x - loc2.x;
        float dy = loc1.y - loc2.y;
        float dz = loc1.z - loc2.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
}
