#include "GamePacket.h"

#include <cstring>
#include <algorithm>

namespace GameNetwork
{
    // GamePacket base implementation
    GamePacket::GamePacket(PacketType type)
        : type_(type)
        , sequence_(0)
    {
    }
    
    auto GamePacket::type() const -> PacketType
    {
        return type_;
    }
    
    auto GamePacket::sequence() const -> uint32_t
    {
        return sequence_;
    }
    
    auto GamePacket::set_sequence(uint32_t seq) -> void
    {
        sequence_ = seq;
    }
    
    auto GamePacket::validate() const -> std::tuple<bool, std::optional<std::string>>
    {
        // Base validation - can be overridden
        return {true, std::nullopt};
    }    
    auto GamePacket::deserialize(const std::vector<uint8_t>& data) 
        -> std::tuple<std::unique_ptr<GamePacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader))
        {
            return {nullptr, "Packet too small for header"};
        }
        
        // Read header
        PacketHeader header;
        std::memcpy(&header, data.data(), sizeof(PacketHeader));
        
        // Create appropriate packet based on type
        PacketType type = static_cast<PacketType>(header.type);
        
        switch (type)
        {
            case PacketType::Heartbeat:
                return HeartbeatPacket::from_data(data);
                
            case PacketType::Authentication:
                return AuthenticationPacket::from_data(data);
                
            case PacketType::EntitySpawn:
                return EntitySpawnPacket::from_data(data);
                
            case PacketType::EntityDespawn:
                return EntityDespawnPacket::from_data(data);
                
            case PacketType::EntityUpdate:
                return EntityUpdatePacket::from_data(data);
                
            default:
                return {nullptr, "Unknown packet type: " + std::to_string(header.type)};
        }
    }
    
    // HeartbeatPacket implementation
    HeartbeatPacket::HeartbeatPacket()
        : GamePacket(PacketType::Heartbeat)
        , timestamp_(0)
    {
    }    
    auto HeartbeatPacket::timestamp() const -> uint64_t
    {
        return timestamp_;
    }
    
    auto HeartbeatPacket::set_timestamp(uint64_t ts) -> void
    {
        timestamp_ = ts;
    }
    
    auto HeartbeatPacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        data.resize(sizeof(PacketHeader) + sizeof(uint64_t));
        
        // Write header
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = sizeof(uint64_t);
        header.checksum = 0; // TODO: Calculate checksum
        
        std::memcpy(data.data(), &header, sizeof(PacketHeader));
        
        // Write timestamp
        std::memcpy(data.data() + sizeof(PacketHeader), &timestamp_, sizeof(uint64_t));
        
        return data;
    }    
    auto HeartbeatPacket::from_data(const std::vector<uint8_t>& data) 
        -> std::tuple<std::unique_ptr<HeartbeatPacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t))
        {
            return {nullptr, "Invalid heartbeat packet size"};
        }
        
        auto packet = std::make_unique<HeartbeatPacket>();
        
        // Read header
        PacketHeader header;
        std::memcpy(&header, data.data(), sizeof(PacketHeader));
        packet->set_sequence(header.sequence);
        
        // Read timestamp
        uint64_t timestamp;
        std::memcpy(&timestamp, data.data() + sizeof(PacketHeader), sizeof(uint64_t));
        packet->set_timestamp(timestamp);
        
        return {std::move(packet), std::nullopt};
    }
    
    // AuthenticationPacket implementation
    AuthenticationPacket::AuthenticationPacket()
        : GamePacket(PacketType::Authentication)
        , client_version_(0)
    {
    }    
    auto AuthenticationPacket::account_id() const -> std::string
    {
        return account_id_;
    }
    
    auto AuthenticationPacket::session_token() const -> std::string
    {
        return session_token_;
    }
    
    auto AuthenticationPacket::client_version() const -> uint32_t
    {
        return client_version_;
    }
    
    auto AuthenticationPacket::set_account_id(const std::string& id) -> void
    {
        account_id_ = id;
    }
    
    auto AuthenticationPacket::set_session_token(const std::string& token) -> void
    {
        session_token_ = token;
    }
    
    auto AuthenticationPacket::set_client_version(uint32_t version) -> void
    {
        client_version_ = version;
    }    
    auto AuthenticationPacket::serialize() const -> std::vector<uint8_t>
    {
        // Calculate sizes
        uint32_t account_id_size = static_cast<uint32_t>(account_id_.size());
        uint32_t token_size = static_cast<uint32_t>(session_token_.size());
        uint32_t payload_size = sizeof(uint32_t) * 3 + account_id_size + token_size;
        
        std::vector<uint8_t> data;
        data.resize(sizeof(PacketHeader) + payload_size);
        
        // Write header
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = payload_size;
        header.checksum = 0; // TODO: Calculate checksum
        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        // Write client version
        std::memcpy(data.data() + offset, &client_version_, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Write account ID
        std::memcpy(data.data() + offset, &account_id_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, account_id_.data(), account_id_size);
        offset += account_id_size;
        
        // Write session token
        std::memcpy(data.data() + offset, &token_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, session_token_.data(), token_size);
        
        return data;
    }
    
    auto AuthenticationPacket::deserialize(const std::vector<uint8_t>& data) 
        -> std::tuple<std::unique_ptr<GamePacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader))
        {
            return {nullptr, "Data too small for packet header"};
        }
        
        // Read header
        PacketHeader header;
        std::memcpy(&header, data.data(), sizeof(PacketHeader));
        
        if (data.size() < sizeof(PacketHeader) + header.size)
        {
            return {nullptr, "Data size mismatch"};
        }
        
        auto packet = std::make_unique<AuthenticationPacket>();
        packet->type_ = static_cast<PacketType>(header.type);
        packet->set_sequence(header.sequence);
        
        size_t offset = sizeof(PacketHeader);
        
        // Read client version
        if (offset + sizeof(uint32_t) > data.size())
        {
            return {nullptr, "Invalid client version data"};
        }
        
        uint32_t client_version;
        std::memcpy(&client_version, data.data() + offset, sizeof(uint32_t));
        packet->set_client_version(client_version);
        offset += sizeof(uint32_t);
        
        // Read account ID
        if (offset + sizeof(uint32_t) > data.size())
        {
            return {nullptr, "Invalid account ID size data"};
        }
        
        uint32_t account_id_size;
        std::memcpy(&account_id_size, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (offset + account_id_size > data.size())
        {
            return {nullptr, "Invalid account ID size"};
        }
        
        std::string account_id(reinterpret_cast<const char*>(data.data() + offset), account_id_size);
        packet->set_account_id(account_id);
        offset += account_id_size;
        
        // Read session token
        if (offset + sizeof(uint32_t) > data.size())
        {
            return {nullptr, "Invalid session token size data"};
        }
        
        uint32_t token_size;
        std::memcpy(&token_size, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (offset + token_size > data.size())
        {
            return {nullptr, "Invalid session token size"};
        }
        
        std::string session_token(reinterpret_cast<const char*>(data.data() + offset), token_size);
        packet->set_session_token(session_token);
        
        return {std::move(packet), std::nullopt};
    }
    
    auto AuthenticationPacket::from_data(const std::vector<uint8_t>& data) 
        -> std::tuple<std::unique_ptr<AuthenticationPacket>, std::optional<std::string>>
    {
        // Create a temporary instance to use the deserialize method
        auto temp_packet = std::make_unique<AuthenticationPacket>();
        auto [success, error] = temp_packet->deserialize(data);
        
        if (!success)
        {
            return {nullptr, error};
        }
        
        return {std::move(temp_packet), std::nullopt};
    }
    
    // EntitySpawnPacket implementation
    EntitySpawnPacket::EntitySpawnPacket() 
        : GamePacket(PacketType::EntitySpawn)
        , entity_id_(0)
        , entity_type_(EntityType::Player)
        , level_(1)
        , health_(100)
        , max_health_(100)
    {
    }
    
    auto EntitySpawnPacket::entity_id() const -> uint64_t
    {
        return entity_id_;
    }
    
    auto EntitySpawnPacket::entity_type() const -> EntityType
    {
        return entity_type_;
    }
    
    auto EntitySpawnPacket::location() const -> Location
    {
        return location_;
    }
    
    auto EntitySpawnPacket::name() const -> std::string
    {
        return name_;
    }
    
    auto EntitySpawnPacket::level() const -> uint32_t
    {
        return level_;
    }
    
    auto EntitySpawnPacket::health() const -> uint32_t
    {
        return health_;
    }
    
    auto EntitySpawnPacket::max_health() const -> uint32_t
    {
        return max_health_;
    }
    
    auto EntitySpawnPacket::set_entity_id(uint64_t id) -> void
    {
        entity_id_ = id;
    }
    
    auto EntitySpawnPacket::set_entity_type(EntityType type) -> void
    {
        entity_type_ = type;
    }
    
    auto EntitySpawnPacket::set_location(const Location& loc) -> void
    {
        location_ = loc;
    }
    
    auto EntitySpawnPacket::set_name(const std::string& name) -> void
    {
        name_ = name;
    }
    
    auto EntitySpawnPacket::set_level(uint32_t level) -> void
    {
        level_ = level;
    }
    
    auto EntitySpawnPacket::set_health(uint32_t health) -> void
    {
        health_ = health;
    }
    
    auto EntitySpawnPacket::set_max_health(uint32_t max_health) -> void
    {
        max_health_ = max_health;
    }
    
    auto EntitySpawnPacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        
        // Calculate total size
        size_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint8_t) + 
                          sizeof(float) * 3 + sizeof(uint32_t) * 2 + // location
                          sizeof(uint32_t) + name_.size() + // name with size
                          sizeof(uint32_t) * 3; // level, health, max_health
        
        data.resize(total_size);
        
        // Write header
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = static_cast<uint32_t>(total_size - sizeof(PacketHeader));
        header.checksum = 0; // TODO: Calculate checksum
        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        // Write entity ID
        std::memcpy(data.data() + offset, &entity_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Write entity type
        uint8_t type = static_cast<uint8_t>(entity_type_);
        std::memcpy(data.data() + offset, &type, sizeof(uint8_t));
        offset += sizeof(uint8_t);
        
        // Write location
        std::memcpy(data.data() + offset, &location_.x, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &location_.y, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &location_.z, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &location_.map_id, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, &location_.channel_id, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Write name
        uint32_t name_size = static_cast<uint32_t>(name_.size());
        std::memcpy(data.data() + offset, &name_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, name_.data(), name_size);
        offset += name_size;
        
        // Write level, health, max_health
        std::memcpy(data.data() + offset, &level_, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, &health_, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, &max_health_, sizeof(uint32_t));
        
        return data;
    }
    
    auto EntitySpawnPacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<EntitySpawnPacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint8_t))
        {
            return {nullptr, "Data too small for EntitySpawnPacket"};
        }
        
        auto packet = std::make_unique<EntitySpawnPacket>();
        size_t offset = sizeof(PacketHeader);
        
        // Read entity ID
        std::memcpy(&packet->entity_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Read entity type
        uint8_t type;
        std::memcpy(&type, data.data() + offset, sizeof(uint8_t));
        packet->entity_type_ = static_cast<EntityType>(type);
        offset += sizeof(uint8_t);
        
        // Read location
        std::memcpy(&packet->location_.x, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->location_.y, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->location_.z, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->location_.map_id, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(&packet->location_.channel_id, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Read name
        uint32_t name_size;
        std::memcpy(&name_size, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (offset + name_size > data.size())
        {
            return {nullptr, "Invalid name size"};
        }
        
        packet->name_ = std::string(reinterpret_cast<const char*>(data.data() + offset), name_size);
        offset += name_size;
        
        // Read level, health, max_health
        std::memcpy(&packet->level_, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(&packet->health_, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(&packet->max_health_, data.data() + offset, sizeof(uint32_t));
        
        return {std::move(packet), std::nullopt};
    }    
    // EntityDespawnPacket implementation
    EntityDespawnPacket::EntityDespawnPacket()
        : GamePacket(PacketType::EntityDespawn)
        , entity_id_(0)
        , reason_(DespawnReason::OutOfRange)
    {
    }
    
    auto EntityDespawnPacket::entity_id() const -> uint64_t
    {
        return entity_id_;
    }
    
    auto EntityDespawnPacket::reason() const -> DespawnReason
    {
        return reason_;
    }
    
    auto EntityDespawnPacket::set_entity_id(uint64_t id) -> void
    {
        entity_id_ = id;
    }
    
    auto EntityDespawnPacket::set_reason(DespawnReason reason) -> void
    {
        reason_ = reason;
    }
    
    auto EntityDespawnPacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        size_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint8_t);
        
        data.resize(total_size);
        
        // Write header
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = static_cast<uint32_t>(total_size - sizeof(PacketHeader));
        header.checksum = 0;
        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        // Write entity ID
        std::memcpy(data.data() + offset, &entity_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Write reason
        uint8_t reason = static_cast<uint8_t>(reason_);
        std::memcpy(data.data() + offset, &reason, sizeof(uint8_t));
        
        return data;
    }
    
    auto EntityDespawnPacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<EntityDespawnPacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint8_t))
        {
            return {nullptr, "Data too small for EntityDespawnPacket"};
        }        
        auto packet = std::make_unique<EntityDespawnPacket>();
        size_t offset = sizeof(PacketHeader);
        
        // Read entity ID
        std::memcpy(&packet->entity_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Read reason
        uint8_t reason;
        std::memcpy(&reason, data.data() + offset, sizeof(uint8_t));
        packet->reason_ = static_cast<DespawnReason>(reason);
        
        return {std::move(packet), std::nullopt};
    }
    
    // EntityUpdatePacket implementation
    EntityUpdatePacket::EntityUpdatePacket()
        : GamePacket(PacketType::EntityUpdate)
        , entity_id_(0)
    {
    }
    
    auto EntityUpdatePacket::entity_id() const -> uint64_t
    {
        return entity_id_;
    }
    
    auto EntityUpdatePacket::location() const -> std::optional<Location>
    {
        return location_;
    }    
    auto EntityUpdatePacket::health() const -> std::optional<uint32_t>
    {
        return health_;
    }
    
    auto EntityUpdatePacket::state() const -> std::optional<EntityState>
    {
        return state_;
    }
    
    auto EntityUpdatePacket::velocity() const -> std::optional<Vector3>
    {
        return velocity_;
    }
    
    auto EntityUpdatePacket::set_entity_id(uint64_t id) -> void
    {
        entity_id_ = id;
    }
    
    auto EntityUpdatePacket::set_location(const Location& loc) -> void
    {
        location_ = loc;
    }
    
    auto EntityUpdatePacket::set_health(uint32_t health) -> void
    {
        health_ = health;
    }
    
    auto EntityUpdatePacket::set_state(EntityState state) -> void
    {
        state_ = state;
    }    
    auto EntityUpdatePacket::set_velocity(const Vector3& vel) -> void
    {
        velocity_ = vel;
    }
    
    auto EntityUpdatePacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        
        // Calculate size based on which fields are set
        size_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint8_t); // entity_id + flags
        uint8_t flags = 0;
        
        if (location_.has_value())
        {
            flags |= 0x01;
            total_size += sizeof(float) * 3 + sizeof(uint32_t) * 2;
        }
        if (health_.has_value())
        {
            flags |= 0x02;
            total_size += sizeof(uint32_t);
        }
        if (state_.has_value())
        {
            flags |= 0x04;
            total_size += sizeof(uint8_t);
        }
        if (velocity_.has_value())
        {
            flags |= 0x08;
            total_size += sizeof(float) * 3;
        }        
        data.resize(total_size);
        
        // Write header
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = static_cast<uint32_t>(total_size - sizeof(PacketHeader));
        header.checksum = 0;
        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        // Write entity ID
        std::memcpy(data.data() + offset, &entity_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Write flags
        std::memcpy(data.data() + offset, &flags, sizeof(uint8_t));
        offset += sizeof(uint8_t);
        
        // Write optional fields
        if (location_.has_value())
        {
            const auto& loc = location_.value();
            std::memcpy(data.data() + offset, &loc.x, sizeof(float));
            offset += sizeof(float);
            std::memcpy(data.data() + offset, &loc.y, sizeof(float));
            offset += sizeof(float);            std::memcpy(data.data() + offset, &loc.z, sizeof(float));
            offset += sizeof(float);
            std::memcpy(data.data() + offset, &loc.map_id, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            std::memcpy(data.data() + offset, &loc.channel_id, sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }
        
        if (health_.has_value())
        {
            std::memcpy(data.data() + offset, &health_.value(), sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }
        
        if (state_.has_value())
        {
            uint8_t state = static_cast<uint8_t>(state_.value());
            std::memcpy(data.data() + offset, &state, sizeof(uint8_t));
            offset += sizeof(uint8_t);
        }
        
        if (velocity_.has_value())
        {
            const auto& vel = velocity_.value();
            std::memcpy(data.data() + offset, &vel.x, sizeof(float));
            offset += sizeof(float);
            std::memcpy(data.data() + offset, &vel.y, sizeof(float));
            offset += sizeof(float);
            std::memcpy(data.data() + offset, &vel.z, sizeof(float));
        }        
        return data;
    }
    
    auto EntityUpdatePacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<EntityUpdatePacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint8_t))
        {
            return {nullptr, "Data too small for EntityUpdatePacket"};
        }
        
        auto packet = std::make_unique<EntityUpdatePacket>();
        size_t offset = sizeof(PacketHeader);
        
        // Read entity ID
        std::memcpy(&packet->entity_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Read flags
        uint8_t flags;
        std::memcpy(&flags, data.data() + offset, sizeof(uint8_t));
        offset += sizeof(uint8_t);
        
        // Read optional fields based on flags
        if (flags & 0x01) // Location
        {
            Location loc;
            std::memcpy(&loc.x, data.data() + offset, sizeof(float));
            offset += sizeof(float);            std::memcpy(&loc.y, data.data() + offset, sizeof(float));
            offset += sizeof(float);
            std::memcpy(&loc.z, data.data() + offset, sizeof(float));
            offset += sizeof(float);
            std::memcpy(&loc.map_id, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            std::memcpy(&loc.channel_id, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            packet->location_ = loc;
        }
        
        if (flags & 0x02) // Health
        {
            uint32_t health;
            std::memcpy(&health, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            packet->health_ = health;
        }
        
        if (flags & 0x04) // State
        {
            uint8_t state;
            std::memcpy(&state, data.data() + offset, sizeof(uint8_t));
            offset += sizeof(uint8_t);
            packet->state_ = static_cast<EntityState>(state);
        }
        
        if (flags & 0x08) // Velocity
        {
            Vector3 vel;            std::memcpy(&vel.x, data.data() + offset, sizeof(float));
            offset += sizeof(float);
            std::memcpy(&vel.y, data.data() + offset, sizeof(float));
            offset += sizeof(float);
            std::memcpy(&vel.z, data.data() + offset, sizeof(float));
            packet->velocity_ = vel;
        }
        
        return {std::move(packet), std::nullopt};
    }
}