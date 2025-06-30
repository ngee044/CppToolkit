#include "EntityReplicator.h"
#include "BinaryGamePacket.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Utilities;

namespace GameNetwork
{
    EntityReplicator::EntityReplicator()
        : delta_compression_enabled_(true)
        , stats_{0, 0, 0, 0, {}}
    {
    }

    auto EntityReplicator::register_entity(uint64_t entity_id, uint32_t entity_type) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        EntityState state;
        state.entity_id = entity_id;
        state.entity_type = entity_type;
        state.replication_rate = 10; // Default 10 Hz
        state.relevancy_distance = 100.0f; // Default 100 units
        state.last_replication = std::chrono::steady_clock::now();
        
        entities_[entity_id] = state;
        
        Logger::handle().write(LogTypes::Information, fmt::format("Registered entity {} for replication", entity_id));
    }

    auto EntityReplicator::unregister_entity(uint64_t entity_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entities_.erase(entity_id);
    }

    auto EntityReplicator::is_entity_registered(uint64_t entity_id) const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return entities_.find(entity_id) != entities_.end();
    }

    auto EntityReplicator::register_property(uint64_t entity_id,
                                            const std::string& property_name,
                                            PropertyType type,
                                            ReplicationMode mode,
                                            uint32_t priority) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it == entities_.end())
        {
            Logger::handle().write(LogTypes::Error, fmt::format("Cannot register property for unregistered entity {}", entity_id));
            return;
        }
        
        ReplicatedProperty prop;
        prop.name = property_name;
        prop.type = type;
        prop.mode = mode;
        prop.priority = priority;
        prop.dirty = false;
        prop.last_replicated = std::chrono::steady_clock::now();
        
        // Initialize with default value based on type
        switch (type)
        {
            case PropertyType::Int32:
                prop.value = int32_t(0);
                break;
            case PropertyType::Float:
                prop.value = 0.0f;
                break;
            case PropertyType::Vector3:
                prop.value = glm::vec3(0.0f);
                break;
            case PropertyType::Quaternion:
                prop.value = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                break;
            case PropertyType::String:
                prop.value = std::string("");
                break;
            default:
                break;
        }
        
        entity_it->second.properties[property_name] = prop;
    }

    template<typename T>
    auto EntityReplicator::update_property(uint64_t entity_id, const std::string& property_name, const T& value) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it == entities_.end())
        {
            return;
        }
        
        auto prop_it = entity_it->second.properties.find(property_name);
        if (prop_it == entity_it->second.properties.end())
        {
            return;
        }
        
        // Store previous value for delta compression
        prop_it->second.previous_value = prop_it->second.value;
        prop_it->second.value = value;
        
        mark_property_dirty(entity_id, property_name);
    }

    // Explicit template instantiations
    template auto EntityReplicator::update_property<int32_t>(uint64_t, const std::string&, const int32_t&) -> void;
    template auto EntityReplicator::update_property<float>(uint64_t, const std::string&, const float&) -> void;
    template auto EntityReplicator::update_property<glm::vec3>(uint64_t, const std::string&, const glm::vec3&) -> void;
    template auto EntityReplicator::update_property<glm::quat>(uint64_t, const std::string&, const glm::quat&) -> void;
    template auto EntityReplicator::update_property<std::string>(uint64_t, const std::string&, const std::string&) -> void;

    template<typename T>
    auto EntityReplicator::get_property(uint64_t entity_id, const std::string& property_name) const -> std::optional<T>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it == entities_.end())
        {
            return std::nullopt;
        }
        
        auto prop_it = entity_it->second.properties.find(property_name);
        if (prop_it == entity_it->second.properties.end())
        {
            return std::nullopt;
        }
        
        try
        {
            return std::any_cast<T>(prop_it->second.value);
        }
        catch (const std::bad_any_cast&)
        {
            return std::nullopt;
        }
    }

    // Explicit template instantiations
    template auto EntityReplicator::get_property<int32_t>(uint64_t, const std::string&) const -> std::optional<int32_t>;
    template auto EntityReplicator::get_property<float>(uint64_t, const std::string&) const -> std::optional<float>;
    template auto EntityReplicator::get_property<glm::vec3>(uint64_t, const std::string&) const -> std::optional<glm::vec3>;
    template auto EntityReplicator::get_property<glm::quat>(uint64_t, const std::string&) const -> std::optional<glm::quat>;
    template auto EntityReplicator::get_property<std::string>(uint64_t, const std::string&) const -> std::optional<std::string>;

    auto EntityReplicator::set_replication_rate(uint64_t entity_id, uint32_t updates_per_second) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it != entities_.end())
        {
            entity_it->second.replication_rate = updates_per_second;
        }
    }

    auto EntityReplicator::set_relevancy_distance(uint64_t entity_id, float distance) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it != entities_.end())
        {
            entity_it->second.relevancy_distance = distance;
        }
    }

    auto EntityReplicator::force_replication(uint64_t entity_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it != entities_.end())
        {
            // Mark all properties as dirty
            for (auto& [prop_name, prop] : entity_it->second.properties)
            {
                prop.dirty = true;
            }
        }
    }

    auto EntityReplicator::enable_delta_compression(bool enable) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        delta_compression_enabled_ = enable;
    }

    auto EntityReplicator::create_delta_snapshot(uint64_t entity_id) -> std::vector<uint8_t>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<uint8_t> snapshot;
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it == entities_.end())
        {
            return snapshot;
        }
        
        // Write entity ID
        snapshot.resize(sizeof(uint64_t));
        std::memcpy(snapshot.data(), &entity_id, sizeof(uint64_t));
        
        // Write dirty property count
        uint32_t dirty_count = 0;
        for (const auto& [name, prop] : entity_it->second.properties)
        {
            if (prop.dirty)
            {
                dirty_count++;
            }
        }
        
        size_t count_offset = snapshot.size();
        snapshot.resize(snapshot.size() + sizeof(uint32_t));
        std::memcpy(snapshot.data() + count_offset, &dirty_count, sizeof(uint32_t));
        
        // Write dirty properties
        for (const auto& [name, prop] : entity_it->second.properties)
        {
            if (!prop.dirty)
            {
                continue;
            }
            
            // Property name length and name
            uint32_t name_len = static_cast<uint32_t>(name.length());
            size_t name_len_offset = snapshot.size();
            snapshot.resize(snapshot.size() + sizeof(uint32_t) + name_len);
            std::memcpy(snapshot.data() + name_len_offset, &name_len, sizeof(uint32_t));
            std::memcpy(snapshot.data() + name_len_offset + sizeof(uint32_t), name.data(), name_len);
            
            // Property data
            auto prop_data = serialize_property(prop);
            snapshot.insert(snapshot.end(), prop_data.begin(), prop_data.end());
            
            stats_.properties_replicated++;
        }
        
        if (delta_compression_enabled_)
        {
            stats_.delta_compressions++;
        }
        
        return snapshot;
    }

    auto EntityReplicator::apply_delta_snapshot(uint64_t entity_id, const std::vector<uint8_t>& delta) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (delta.size() < sizeof(uint64_t) + sizeof(uint32_t))
        {
            return;
        }
        
        size_t offset = 0;
        
        // Read entity ID
        uint64_t snapshot_entity_id;
        std::memcpy(&snapshot_entity_id, delta.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        if (snapshot_entity_id != entity_id)
        {
            Logger::handle().write(LogTypes::Error,"Entity ID mismatch in delta snapshot");
            return;
        }
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it == entities_.end())
        {
            return;
        }
        
        // Read property count
        uint32_t prop_count;
        std::memcpy(&prop_count, delta.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Read properties
        for (uint32_t i = 0; i < prop_count; ++i)
        {
            if (offset + sizeof(uint32_t) > delta.size())
            {
                break;
            }
            
            // Read property name
            uint32_t name_len;
            std::memcpy(&name_len, delta.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            if (offset + name_len > delta.size())
            {
                break;
            }
            
            std::string prop_name(reinterpret_cast<const char*>(delta.data() + offset), name_len);
            offset += name_len;
            
            // Find property
            auto prop_it = entity_it->second.properties.find(prop_name);
            if (prop_it != entity_it->second.properties.end())
            {
                // Extract property data based on type
                size_t data_size = 0;
                switch (prop_it->second.type)
                {
                    case PropertyType::Int32:
                        data_size = sizeof(int32_t);
                        break;
                    case PropertyType::Float:
                        data_size = sizeof(float);
                        break;
                    case PropertyType::Vector3:
                        data_size = sizeof(float) * 3;
                        break;
                    case PropertyType::Quaternion:
                        data_size = sizeof(float) * 4;
                        break;
                    case PropertyType::String:
                        {
                            if (offset + sizeof(uint32_t) <= delta.size())
                            {
                                uint32_t str_len;
                                std::memcpy(&str_len, delta.data() + offset, sizeof(uint32_t));
                                data_size = sizeof(uint32_t) + str_len;
                            }
                        }
                        break;
                    default:
                        continue;
                }
                
                if (offset + data_size <= delta.size())
                {
                    std::vector<uint8_t> prop_data(delta.begin() + offset, delta.begin() + offset + data_size);
                    prop_it->second.value = deserialize_property(prop_data, prop_it->second.type);
                    offset += data_size;
                }
            }
        }
    }

    auto EntityReplicator::gather_dirty_entities() const -> std::vector<uint64_t>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<uint64_t> dirty_entities;
        
        for (const auto& [entity_id, entity_state] : entities_)
        {
            bool has_dirty_props = false;
            for (const auto& [name, prop] : entity_state.properties)
            {
                if (prop.dirty)
                {
                    has_dirty_props = true;
                    break;
                }
            }
            
            if (has_dirty_props)
            {
                dirty_entities.push_back(entity_id);
            }
        }
        
        return dirty_entities;
    }

    auto EntityReplicator::create_replication_packet(uint64_t entity_id) -> std::unique_ptr<GamePacket>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it == entities_.end())
        {
            return nullptr;
        }

        auto packet = std::make_unique<BinaryGamePacket>();
        packet->packet_type = PacketType::EntityUpdate;
        
        // Create delta snapshot
        auto snapshot = create_delta_snapshot(entity_id);
        packet->data.insert(packet->data.end(), snapshot.begin(), snapshot.end());
        
        // Clear dirty flags after creating packet
        clear_dirty_flags(entity_id);
        
        // Update stats
        stats_.total_replications++;
        stats_.replications_per_entity[entity_id]++;
        
        entity_it->second.last_replication = std::chrono::steady_clock::now();
        
        return packet;
    }

    auto EntityReplicator::process_replication_packet(const GamePacket& packet) -> void
    {
        if (packet.data.size() < sizeof(uint64_t))
        {
            return;
        }
        
        // Extract entity ID
        uint64_t entity_id;
        std::memcpy(&entity_id, packet.data.data(), sizeof(uint64_t));
        
        // Apply delta snapshot
        apply_delta_snapshot(entity_id, packet.data);
    }

    auto EntityReplicator::calculate_priority(uint64_t entity_id, const Location& viewer_location) const -> float
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it == entities_.end())
        {
            return 0.0f;
        }
        
        // Get entity position if available
        auto position_opt = get_property<glm::vec3>(entity_id, "position");
        if (!position_opt.has_value())
        {
            return 100.0f; // Default priority if no position
        }
        
        // Calculate distance-based priority
        float distance = glm::length(position_opt.value() - viewer_location.position);
        float distance_priority = 1.0f / (1.0f + distance * 0.01f); // Inverse distance
        
        // Factor in property priorities
        float property_priority = 0.0f;
        for (const auto& [name, prop] : entity_it->second.properties)
        {
            if (prop.dirty)
            {
                property_priority += prop.priority;
            }
        }
        property_priority = std::min(property_priority / 1000.0f, 1.0f); // Normalize
        
        // Combine priorities
        return distance_priority * 0.7f + property_priority * 0.3f;
    }

    auto EntityReplicator::should_replicate_to(uint64_t entity_id, const Location& viewer_location) const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto entity_it = entities_.find(entity_id);
        if (entity_it == entities_.end())
        {
            return false;
        }
        
        // Get entity position
        auto position_opt = get_property<glm::vec3>(entity_id, "position");
        if (!position_opt.has_value())
        {
            return true; // Replicate if no position (might be important)
        }
        
        // Check relevancy distance
        float distance = glm::length(position_opt.value() - viewer_location.position);
        return distance <= entity_it->second.relevancy_distance;
    }

    auto EntityReplicator::create_full_snapshot() const -> std::vector<uint8_t>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<uint8_t> snapshot;
        
        // Write entity count
        uint32_t entity_count = static_cast<uint32_t>(entities_.size());
        snapshot.resize(sizeof(uint32_t));
        std::memcpy(snapshot.data(), &entity_count, sizeof(uint32_t));

        // Write each entity
        for (const auto& [entity_id, entity_state] : entities_)
        {
            // Entity ID and type
            size_t id_offset = snapshot.size();
            snapshot.resize(snapshot.size() + sizeof(uint64_t) + sizeof(uint32_t));
            std::memcpy(snapshot.data() + id_offset, &entity_id, sizeof(uint64_t));
            std::memcpy(snapshot.data() + id_offset + sizeof(uint64_t), &entity_state.entity_type, sizeof(uint32_t));
            
            // Property count
            uint32_t prop_count = static_cast<uint32_t>(entity_state.properties.size());
            size_t prop_count_offset = snapshot.size();
            snapshot.resize(snapshot.size() + sizeof(uint32_t));
            std::memcpy(snapshot.data() + prop_count_offset, &prop_count, sizeof(uint32_t));
            
            // Each property
            for (const auto& [prop_name, prop] : entity_state.properties)
            {
                // Property name
                uint32_t name_len = static_cast<uint32_t>(prop_name.length());
                size_t name_offset = snapshot.size();
                snapshot.resize(snapshot.size() + sizeof(uint32_t) + name_len);
                std::memcpy(snapshot.data() + name_offset, &name_len, sizeof(uint32_t));
                std::memcpy(snapshot.data() + name_offset + sizeof(uint32_t), prop_name.data(), name_len);
                
                // Property type
                snapshot.push_back(static_cast<uint8_t>(prop.type));
                
                // Property data
                auto prop_data = serialize_property(prop);
                snapshot.insert(snapshot.end(), prop_data.begin(), prop_data.end());
            }
        }
        
        return snapshot;
    }

    auto EntityReplicator::restore_from_snapshot(const std::vector<uint8_t>& snapshot) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (snapshot.size() < sizeof(uint32_t))
        {
            return;
        }
        
        entities_.clear();
        size_t offset = 0;
        
        // Read entity count
        uint32_t entity_count;
        std::memcpy(&entity_count, snapshot.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Read each entity
        for (uint32_t i = 0; i < entity_count; ++i)
        {
            if (offset + sizeof(uint64_t) + sizeof(uint32_t) > snapshot.size())
            {
                break;
            }
            
            // Read entity ID and type
            uint64_t entity_id;
            uint32_t entity_type;
            std::memcpy(&entity_id, snapshot.data() + offset, sizeof(uint64_t));
            offset += sizeof(uint64_t);
            std::memcpy(&entity_type, snapshot.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            // Create entity
            register_entity(entity_id, entity_type);
            
            // Read property count
            uint32_t prop_count;
            std::memcpy(&prop_count, snapshot.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            // Read properties
            for (uint32_t j = 0; j < prop_count; ++j)
            {
                if (offset + sizeof(uint32_t) > snapshot.size())
                {
                    break;
                }
                
                // Read property name
                uint32_t name_len;
                std::memcpy(&name_len, snapshot.data() + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                
                if (offset + name_len + 1 > snapshot.size())
                {
                    break;
                }
                
                std::string prop_name(reinterpret_cast<const char*>(snapshot.data() + offset), name_len);
                offset += name_len;
                
                // Read property type
                PropertyType prop_type = static_cast<PropertyType>(snapshot[offset]);
                offset += 1;
                
                // Register property
                register_property(entity_id, prop_name, prop_type);
                
                // Read property value
                // Size depends on type
                size_t value_size = 0;
                switch (prop_type)
                {
                    case PropertyType::Int32:
                        value_size = sizeof(int32_t);
                        break;
                    case PropertyType::Float:
                        value_size = sizeof(float);
                        break;
                    case PropertyType::Vector3:
                        value_size = sizeof(float) * 3;
                        break;
                    case PropertyType::Quaternion:
                        value_size = sizeof(float) * 4;
                        break;
                    case PropertyType::String:
                        if (offset + sizeof(uint32_t) <= snapshot.size())
                        {
                            uint32_t str_len;
                            std::memcpy(&str_len, snapshot.data() + offset, sizeof(uint32_t));
                            value_size = sizeof(uint32_t) + str_len;
                        }
                        break;
                    default:
                        continue;
                }
                
                if (offset + value_size <= snapshot.size())
                {
                    std::vector<uint8_t> value_data(snapshot.begin() + offset, snapshot.begin() + offset + value_size);
                    auto value = deserialize_property(value_data, prop_type);
                    
                    // Update property value directly
                    auto entity_it = entities_.find(entity_id);
                    if (entity_it != entities_.end())
                    {
                        auto prop_it = entity_it->second.properties.find(prop_name);
                        if (prop_it != entity_it->second.properties.end())
                        {
                            prop_it->second.value = value;
                        }
                    }
                    
                    offset += value_size;
                }
            }
        }
    }

    auto EntityReplicator::get_stats() const -> ReplicationStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    auto EntityReplicator::reset_stats() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = ReplicationStats{0, 0, 0, 0, {}};
    }

    auto EntityReplicator::mark_property_dirty(uint64_t entity_id, const std::string& property_name) -> void
    {
        auto entity_it = entities_.find(entity_id);
        if (entity_it != entities_.end())
        {
            auto prop_it = entity_it->second.properties.find(property_name);
            if (prop_it != entity_it->second.properties.end())
            {
                prop_it->second.dirty = true;
            }
        }
    }

    auto EntityReplicator::clear_dirty_flags(uint64_t entity_id) -> void
    {
        auto entity_it = entities_.find(entity_id);
        if (entity_it != entities_.end())
        {
            for (auto& [name, prop] : entity_it->second.properties)
            {
                prop.dirty = false;
            }
        }
    }

    auto EntityReplicator::serialize_property(const ReplicatedProperty& prop) const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        
        switch (prop.type)
        {
            case PropertyType::Int32:
                {
                    data.resize(sizeof(int32_t));
                    auto value = std::any_cast<int32_t>(prop.value);
                    std::memcpy(data.data(), &value, sizeof(int32_t));
                }
                break;
                
            case PropertyType::Float:
                {
                    data.resize(sizeof(float));
                    auto value = std::any_cast<float>(prop.value);
                    std::memcpy(data.data(), &value, sizeof(float));
                }
                break;
                
            case PropertyType::Vector3:
                {
                    data.resize(sizeof(float) * 3);
                    auto value = std::any_cast<glm::vec3>(prop.value);
                    std::memcpy(data.data(), &value.x, sizeof(float));
                    std::memcpy(data.data() + sizeof(float), &value.y, sizeof(float));
                    std::memcpy(data.data() + sizeof(float) * 2, &value.z, sizeof(float));
                }
                break;
                
            case PropertyType::Quaternion:
                {
                    data.resize(sizeof(float) * 4);
                    auto value = std::any_cast<glm::quat>(prop.value);
                    std::memcpy(data.data(), &value.w, sizeof(float));
                    std::memcpy(data.data() + sizeof(float), &value.x, sizeof(float));
                    std::memcpy(data.data() + sizeof(float) * 2, &value.y, sizeof(float));
                    std::memcpy(data.data() + sizeof(float) * 3, &value.z, sizeof(float));
                }
                break;
                
            case PropertyType::String:
                {
                    auto value = std::any_cast<std::string>(prop.value);
                    uint32_t str_len = static_cast<uint32_t>(value.length());
                    data.resize(sizeof(uint32_t) + str_len);
                    std::memcpy(data.data(), &str_len, sizeof(uint32_t));
                    std::memcpy(data.data() + sizeof(uint32_t), value.data(), str_len);
                }
                break;
                
            default:
                break;
        }
        
        return data;
    }

    auto EntityReplicator::deserialize_property(const std::vector<uint8_t>& data, PropertyType type) -> std::any
    {
        switch (type)
        {
            case PropertyType::Int32:
                {
                    int32_t value;
                    std::memcpy(&value, data.data(), sizeof(int32_t));
                    return value;
                }
                
            case PropertyType::Float:
                {
                    float value;
                    std::memcpy(&value, data.data(), sizeof(float));
                    return value;
                }
                
            case PropertyType::Vector3:
                {
                    glm::vec3 value;
                    std::memcpy(&value.x, data.data(), sizeof(float));
                    std::memcpy(&value.y, data.data() + sizeof(float), sizeof(float));
                    std::memcpy(&value.z, data.data() + sizeof(float) * 2, sizeof(float));
                    return value;
                }
                
            case PropertyType::Quaternion:
                {
                    glm::quat value;
                    std::memcpy(&value.w, data.data(), sizeof(float));
                    std::memcpy(&value.x, data.data() + sizeof(float), sizeof(float));
                    std::memcpy(&value.y, data.data() + sizeof(float) * 2, sizeof(float));
                    std::memcpy(&value.z, data.data() + sizeof(float) * 3, sizeof(float));
                    return value;
                }
                
            case PropertyType::String:
                {
                    uint32_t str_len;
                    std::memcpy(&str_len, data.data(), sizeof(uint32_t));
                    std::string value(reinterpret_cast<const char*>(data.data() + sizeof(uint32_t)), str_len);
                    return value;
                }
                
            default:
                return std::any{};
        }
    }
}
