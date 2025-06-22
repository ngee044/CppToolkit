#include "GamePacket.h"
#include "../GameNetworkConstants.h"
#include "../../Utilities/Logger.h"

#include <cstring>
#include <algorithm>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>
#include <string>

using namespace Utilities;

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
                
            case PacketType::MoveTo:
                return MoveToPacket::from_data(data);
                
            case PacketType::MoveStop:
                return MoveStopPacket::from_data(data);
                
            case PacketType::Attack:
                return AttackPacket::from_data(data);
                
            case PacketType::Damage:
                return DamagePacket::from_data(data);
                
            case PacketType::ChatMessage:
                return ChatMessagePacket::from_data(data);
                
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
    
    // MoveToPacket implementation
    MoveToPacket::MoveToPacket()
        : GamePacket(PacketType::MoveTo)
        , entity_id_(0)
        , movement_speed_(1.0f)
        , movement_type_(0)
    {
    }
    
    auto MoveToPacket::entity_id() const -> uint64_t
    {
        return entity_id_;
    }
    
    auto MoveToPacket::destination() const -> Location
    {
        return destination_;
    }
    
    auto MoveToPacket::movement_speed() const -> float
    {
        return movement_speed_;
    }
    
    auto MoveToPacket::movement_type() const -> uint8_t
    {
        return movement_type_;
    }
    
    auto MoveToPacket::set_entity_id(uint64_t id) -> void
    {
        entity_id_ = id;
    }    
    auto MoveToPacket::set_destination(const Location& dest) -> void
    {
        destination_ = dest;
    }
    
    auto MoveToPacket::set_movement_speed(float speed) -> void
    {
        movement_speed_ = speed;
    }
    
    auto MoveToPacket::set_movement_type(uint8_t type) -> void
    {
        movement_type_ = type;
    }
    
    auto MoveToPacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        size_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) + 
                          sizeof(float) * 3 + sizeof(uint32_t) * 2 + // Location
                          sizeof(float) + sizeof(uint8_t); // speed + type
        
        data.resize(total_size);
        
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = static_cast<uint32_t>(total_size - sizeof(PacketHeader));
        header.checksum = 0;        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        std::memcpy(data.data() + offset, &entity_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Write destination
        std::memcpy(data.data() + offset, &destination_.x, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &destination_.y, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &destination_.z, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &destination_.map_id, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, &destination_.channel_id, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        std::memcpy(data.data() + offset, &movement_speed_, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &movement_type_, sizeof(uint8_t));
        
        return data;
    }
    
    auto MoveToPacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<MoveToPacket>, std::optional<std::string>>
    {        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(float) * 4 + sizeof(uint32_t) * 2 + sizeof(uint8_t))
        {
            return {nullptr, "Data too small for MoveToPacket"};
        }
        
        auto packet = std::make_unique<MoveToPacket>();
        size_t offset = sizeof(PacketHeader);
        
        std::memcpy(&packet->entity_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Read destination
        std::memcpy(&packet->destination_.x, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->destination_.y, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->destination_.z, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->destination_.map_id, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(&packet->destination_.channel_id, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        std::memcpy(&packet->movement_speed_, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->movement_type_, data.data() + offset, sizeof(uint8_t));
        
        return {std::move(packet), std::nullopt};
    }    
    // MoveStopPacket implementation
    MoveStopPacket::MoveStopPacket()
        : GamePacket(PacketType::MoveStop)
        , entity_id_(0)
    {
    }
    
    auto MoveStopPacket::entity_id() const -> uint64_t
    {
        return entity_id_;
    }
    
    auto MoveStopPacket::stop_location() const -> Location
    {
        return stop_location_;
    }
    
    auto MoveStopPacket::set_entity_id(uint64_t id) -> void
    {
        entity_id_ = id;
    }
    
    auto MoveStopPacket::set_stop_location(const Location& loc) -> void
    {
        stop_location_ = loc;
    }
    
    auto MoveStopPacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;        size_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) + 
                          sizeof(float) * 3 + sizeof(uint32_t) * 2; // entity_id + Location
        
        data.resize(total_size);
        
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = static_cast<uint32_t>(total_size - sizeof(PacketHeader));
        header.checksum = 0;
        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        std::memcpy(data.data() + offset, &entity_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Write stop location
        std::memcpy(data.data() + offset, &stop_location_.x, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &stop_location_.y, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &stop_location_.z, sizeof(float));
        offset += sizeof(float);
        std::memcpy(data.data() + offset, &stop_location_.map_id, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, &stop_location_.channel_id, sizeof(uint32_t));
        
        return data;    }
    
    auto MoveStopPacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<MoveStopPacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(float) * 3 + sizeof(uint32_t) * 2)
        {
            return {nullptr, "Data too small for MoveStopPacket"};
        }
        
        auto packet = std::make_unique<MoveStopPacket>();
        size_t offset = sizeof(PacketHeader);
        
        std::memcpy(&packet->entity_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Read stop location
        std::memcpy(&packet->stop_location_.x, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->stop_location_.y, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->stop_location_.z, data.data() + offset, sizeof(float));
        offset += sizeof(float);
        std::memcpy(&packet->stop_location_.map_id, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(&packet->stop_location_.channel_id, data.data() + offset, sizeof(uint32_t));
        
        return {std::move(packet), std::nullopt};
    }    
    // AttackPacket implementation
    AttackPacket::AttackPacket()
        : GamePacket(PacketType::Attack)
        , attacker_id_(0)
        , target_id_(0)
        , attack_type_(0)
        , damage_(0)
        , is_critical_(false)
    {
    }
    
    auto AttackPacket::attacker_id() const -> uint64_t
    {
        return attacker_id_;
    }
    
    auto AttackPacket::target_id() const -> uint64_t
    {
        return target_id_;
    }
    
    auto AttackPacket::attack_type() const -> uint16_t
    {
        return attack_type_;
    }
    
    auto AttackPacket::damage() const -> uint32_t
    {
        return damage_;
    }
    
    auto AttackPacket::is_critical() const -> bool    {
        return is_critical_;
    }
    
    auto AttackPacket::set_attacker_id(uint64_t id) -> void
    {
        attacker_id_ = id;
    }
    
    auto AttackPacket::set_target_id(uint64_t id) -> void
    {
        target_id_ = id;
    }
    
    auto AttackPacket::set_attack_type(uint16_t type) -> void
    {
        attack_type_ = type;
    }
    
    auto AttackPacket::set_damage(uint32_t dmg) -> void
    {
        damage_ = dmg;
    }
    
    auto AttackPacket::set_critical(bool crit) -> void
    {
        is_critical_ = crit;
    }
    
    auto AttackPacket::serialize() const -> std::vector<uint8_t>
    {        std::vector<uint8_t> data;
        size_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) * 2 + 
                          sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t);
        
        data.resize(total_size);
        
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = static_cast<uint32_t>(total_size - sizeof(PacketHeader));
        header.checksum = 0;
        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        std::memcpy(data.data() + offset, &attacker_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(data.data() + offset, &target_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(data.data() + offset, &attack_type_, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        std::memcpy(data.data() + offset, &damage_, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        uint8_t crit_flag = is_critical_ ? 1 : 0;
        std::memcpy(data.data() + offset, &crit_flag, sizeof(uint8_t));
        
        return data;    }
    
    auto AttackPacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<AttackPacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) * 2 + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t))
        {
            return {nullptr, "Data too small for AttackPacket"};
        }
        
        auto packet = std::make_unique<AttackPacket>();
        size_t offset = sizeof(PacketHeader);
        
        std::memcpy(&packet->attacker_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(&packet->target_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(&packet->attack_type_, data.data() + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        std::memcpy(&packet->damage_, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        uint8_t crit_flag;
        std::memcpy(&crit_flag, data.data() + offset, sizeof(uint8_t));
        packet->is_critical_ = (crit_flag != 0);
        
        return {std::move(packet), std::nullopt};
    }
    
    // DamagePacket implementation
    DamagePacket::DamagePacket()
        : GamePacket(PacketType::Damage)
        , target_id_(0)
        , damage_amount_(0)
        , damage_type_(0)
        , source_id_(0)
        , remaining_hp_(0)
    {
    }
    
    auto DamagePacket::target_id() const -> uint64_t
    {
        return target_id_;
    }
    
    auto DamagePacket::damage_amount() const -> uint32_t
    {
        return damage_amount_;
    }
    
    auto DamagePacket::damage_type() const -> uint16_t
    {
        return damage_type_;
    }
    
    auto DamagePacket::source_id() const -> uint64_t
    {
        return source_id_;
    }
    
    auto DamagePacket::remaining_hp() const -> uint32_t
    {
        return remaining_hp_;
    }
    
    auto DamagePacket::set_target_id(uint64_t id) -> void
    {
        target_id_ = id;
    }
    
    auto DamagePacket::set_damage_amount(uint32_t amount) -> void
    {
        damage_amount_ = amount;
    }
    
    auto DamagePacket::set_damage_type(uint16_t type) -> void
    {
        damage_type_ = type;
    }
    
    auto DamagePacket::set_source_id(uint64_t id) -> void
    {
        source_id_ = id;
    }
    
    auto DamagePacket::set_remaining_hp(uint32_t hp) -> void
    {
        remaining_hp_ = hp;
    }
    
    auto DamagePacket::serialize() const -> std::vector<uint8_t>
    {
        size_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) * 2 + sizeof(uint32_t) * 2 + sizeof(uint16_t);
        std::vector<uint8_t> data(total_size);
        
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = static_cast<uint32_t>(total_size);
        header.checksum = 0;
        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        std::memcpy(data.data() + offset, &target_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(data.data() + offset, &damage_amount_, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, &damage_type_, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        std::memcpy(data.data() + offset, &source_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(data.data() + offset, &remaining_hp_, sizeof(uint32_t));
        
        return data;
    }
    
    auto DamagePacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<DamagePacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) * 2 + sizeof(uint32_t) * 2 + sizeof(uint16_t))
        {
            return {nullptr, "Data too small for DamagePacket"};
        }
        
        auto packet = std::make_unique<DamagePacket>();
        size_t offset = sizeof(PacketHeader);
        
        std::memcpy(&packet->target_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(&packet->damage_amount_, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(&packet->damage_type_, data.data() + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        std::memcpy(&packet->source_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(&packet->remaining_hp_, data.data() + offset, sizeof(uint32_t));
        
        return {std::move(packet), std::nullopt};
    }
        
    // ChatMessagePacket implementation
    ChatMessagePacket::ChatMessagePacket()
        : GamePacket(PacketType::ChatMessage)
        , sender_id_(0)
        , chat_type_(0)
        , channel_id_(0)
    {
    }
    
    auto ChatMessagePacket::sender_id() const -> uint64_t
    {
        return sender_id_;
    }
    
    auto ChatMessagePacket::sender_name() const -> std::string
    {
        return sender_name_;
    }
    
    auto ChatMessagePacket::message() const -> std::string
    {
        return message_;
    }
    
    auto ChatMessagePacket::chat_type() const -> uint8_t
    {
        return chat_type_;
    }
    
    auto ChatMessagePacket::channel_id() const -> uint32_t
    {        return channel_id_;
    }
    
    auto ChatMessagePacket::set_sender_id(uint64_t id) -> void
    {
        sender_id_ = id;
    }
    
    auto ChatMessagePacket::set_sender_name(const std::string& name) -> void
    {
        sender_name_ = name;
    }
    
    auto ChatMessagePacket::set_message(const std::string& msg) -> void
    {
        message_ = msg;
    }
    
    auto ChatMessagePacket::set_chat_type(uint8_t type) -> void
    {
        chat_type_ = type;
    }
    
    auto ChatMessagePacket::set_channel_id(uint32_t id) -> void
    {
        channel_id_ = id;
    }
    
    auto ChatMessagePacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;        size_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) + 
                          sizeof(uint32_t) * 2 + // sender_name size + message size
                          sender_name_.size() + message_.size() +
                          sizeof(uint8_t) + sizeof(uint32_t); // chat_type + channel_id
        
        data.resize(total_size);
        
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = static_cast<uint32_t>(total_size - sizeof(PacketHeader));
        header.checksum = 0;
        
        size_t offset = 0;
        std::memcpy(data.data() + offset, &header, sizeof(PacketHeader));
        offset += sizeof(PacketHeader);
        
        std::memcpy(data.data() + offset, &sender_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Write sender name
        uint32_t name_size = static_cast<uint32_t>(sender_name_.size());
        std::memcpy(data.data() + offset, &name_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, sender_name_.data(), name_size);
        offset += name_size;
        
        // Write message
        uint32_t msg_size = static_cast<uint32_t>(message_.size());        std::memcpy(data.data() + offset, &msg_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, message_.data(), msg_size);
        offset += msg_size;
        
        std::memcpy(data.data() + offset, &chat_type_, sizeof(uint8_t));
        offset += sizeof(uint8_t);
        std::memcpy(data.data() + offset, &channel_id_, sizeof(uint32_t));
        
        return data;
    }
    
    auto ChatMessagePacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<ChatMessagePacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint32_t) * 2 + sizeof(uint8_t) + sizeof(uint32_t))
        {
            return {nullptr, "Data too small for ChatMessagePacket"};
        }
        
        auto packet = std::make_unique<ChatMessagePacket>();
        size_t offset = sizeof(PacketHeader);
        
        std::memcpy(&packet->sender_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Read sender name
        uint32_t name_size;
        std::memcpy(&name_size, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);        
        if (offset + name_size > data.size())
        {
            return {nullptr, "Invalid sender name size"};
        }
        packet->sender_name_ = std::string(reinterpret_cast<const char*>(data.data() + offset), name_size);
        offset += name_size;
        
        // Read message
        uint32_t msg_size;
        std::memcpy(&msg_size, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (offset + msg_size > data.size())
        {
            return {nullptr, "Invalid message size"};
        }
        packet->message_ = std::string(reinterpret_cast<const char*>(data.data() + offset), msg_size);
        offset += msg_size;
        
        std::memcpy(&packet->chat_type_, data.data() + offset, sizeof(uint8_t));
        offset += sizeof(uint8_t);
        std::memcpy(&packet->channel_id_, data.data() + offset, sizeof(uint32_t));
        
        return {std::move(packet), std::nullopt};
    }
    
    // PartyInvitePacket implementation
    PartyInvitePacket::PartyInvitePacket()
        : GamePacket(PacketType::PartyInvite)
        , inviter_id_(0)
        , invited_player_id_(0)
        , party_id_(0)
    {
    }
    
    auto PartyInvitePacket::inviter_id() const -> uint64_t
    {
        return inviter_id_;
    }
    
    auto PartyInvitePacket::inviter_name() const -> std::string
    {
        return inviter_name_;
    }
    
    auto PartyInvitePacket::invited_player_id() const -> uint64_t
    {
        return invited_player_id_;
    }
    
    auto PartyInvitePacket::party_id() const -> uint64_t
    {
        return party_id_;
    }
    
    auto PartyInvitePacket::set_inviter_id(uint64_t id) -> void
    {
        inviter_id_ = id;
    }    
    auto PartyInvitePacket::set_inviter_name(const std::string& name) -> void
    {
        inviter_name_ = name;
    }
    
    auto PartyInvitePacket::set_invited_player_id(uint64_t id) -> void
    {
        invited_player_id_ = id;
    }
    
    auto PartyInvitePacket::set_party_id(uint64_t id) -> void
    {
        party_id_ = id;
    }
    
    auto PartyInvitePacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        
        // Calculate total size
        uint32_t name_size = static_cast<uint32_t>(inviter_name_.size());
        uint32_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) * 3 + 
                             sizeof(uint32_t) + name_size;
        
        data.resize(total_size);
        
        // Write header
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = total_size;
        header.checksum = 0;
        
        std::memcpy(data.data(), &header, sizeof(PacketHeader));
        
        // Write data
        size_t offset = sizeof(PacketHeader);
        std::memcpy(data.data() + offset, &inviter_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Write inviter name
        std::memcpy(data.data() + offset, &name_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, inviter_name_.c_str(), name_size);
        offset += name_size;
        
        std::memcpy(data.data() + offset, &invited_player_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(data.data() + offset, &party_id_, sizeof(uint64_t));
        
        return data;
    }    
    auto PartyInvitePacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<PartyInvitePacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) * 3 + sizeof(uint32_t))
        {
            return std::make_tuple(nullptr, std::optional<std::string>("Data too small for PartyInvitePacket"));
        }
        
        auto packet = std::make_unique<PartyInvitePacket>();
        size_t offset = sizeof(PacketHeader);
        
        std::memcpy(&packet->inviter_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Read inviter name
        uint32_t name_size;
        std::memcpy(&name_size, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (offset + name_size > data.size())
        {
            return std::make_tuple(nullptr, std::optional<std::string>("Invalid inviter name size"));
        }
        packet->inviter_name_ = std::string(reinterpret_cast<const char*>(data.data() + offset), name_size);
        offset += name_size;
        
        std::memcpy(&packet->invited_player_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(&packet->party_id_, data.data() + offset, sizeof(uint64_t));
        
        return std::make_tuple(std::move(packet), std::nullopt);
    }    
    // TradeRequestPacket implementation
    TradeRequestPacket::TradeRequestPacket()
        : GamePacket(PacketType::TradeRequest)
        , requester_id_(0)
        , target_id_(0)
    {
    }
    
    auto TradeRequestPacket::requester_id() const -> uint64_t
    {
        return requester_id_;
    }
    
    auto TradeRequestPacket::requester_name() const -> std::string
    {
        return requester_name_;
    }
    
    auto TradeRequestPacket::target_id() const -> uint64_t
    {
        return target_id_;
    }
    
    auto TradeRequestPacket::set_requester_id(uint64_t id) -> void
    {
        requester_id_ = id;
    }
    
    auto TradeRequestPacket::set_requester_name(const std::string& name) -> void
    {
        requester_name_ = name;
    }    
    auto TradeRequestPacket::set_target_id(uint64_t id) -> void
    {
        target_id_ = id;
    }
    
    auto TradeRequestPacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        
        // Calculate total size
        uint32_t name_size = static_cast<uint32_t>(requester_name_.size());
        uint32_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) * 2 + 
                             sizeof(uint32_t) + name_size;
        
        data.resize(total_size);
        
        // Write header
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = total_size;
        header.checksum = 0;
        
        std::memcpy(data.data(), &header, sizeof(PacketHeader));
        
        // Write data
        size_t offset = sizeof(PacketHeader);
        std::memcpy(data.data() + offset, &requester_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Write requester name
        std::memcpy(data.data() + offset, &name_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, requester_name_.c_str(), name_size);
        offset += name_size;
        
        std::memcpy(data.data() + offset, &target_id_, sizeof(uint64_t));
        
        return data;
    }    
    auto TradeRequestPacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<TradeRequestPacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) * 2 + sizeof(uint32_t))
        {
            return std::make_tuple(nullptr, std::optional<std::string>("Data too small for TradeRequestPacket"));
        }
        
        auto packet = std::make_unique<TradeRequestPacket>();
        size_t offset = sizeof(PacketHeader);
        
        std::memcpy(&packet->requester_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Read requester name
        uint32_t name_size;
        std::memcpy(&name_size, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (offset + name_size > data.size())
        {
            return std::make_tuple(nullptr, std::optional<std::string>("Invalid requester name size"));
        }
        packet->requester_name_ = std::string(reinterpret_cast<const char*>(data.data() + offset), name_size);
        offset += name_size;
        
        std::memcpy(&packet->target_id_, data.data() + offset, sizeof(uint64_t));
        
        return std::make_tuple(std::move(packet), std::nullopt);
    }    
    // QuestUpdatePacket implementation
    QuestUpdatePacket::QuestUpdatePacket()
        : GamePacket(PacketType::QuestUpdate)
        , player_id_(0)
        , quest_id_(0)
        , quest_state_(0)
    {
    }
    
    auto QuestUpdatePacket::player_id() const -> uint64_t
    {
        return player_id_;
    }
    
    auto QuestUpdatePacket::quest_id() const -> uint32_t
    {
        return quest_id_;
    }
    
    auto QuestUpdatePacket::quest_state() const -> uint8_t
    {
        return quest_state_;
    }
    
    auto QuestUpdatePacket::objectives() const -> std::vector<QuestObjective>
    {
        return objectives_;
    }
    
    auto QuestUpdatePacket::set_player_id(uint64_t id) -> void
    {
        player_id_ = id;
    }    
    auto QuestUpdatePacket::set_quest_id(uint32_t id) -> void
    {
        quest_id_ = id;
    }
    
    auto QuestUpdatePacket::set_quest_state(uint8_t state) -> void
    {
        quest_state_ = state;
    }
    
    auto QuestUpdatePacket::add_objective(const QuestObjective& objective) -> void
    {
        objectives_.push_back(objective);
    }
    
    auto QuestUpdatePacket::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        
        // Calculate total size
        uint32_t objectives_count = static_cast<uint32_t>(objectives_.size());
        uint32_t objectives_size = 0;
        for (const auto& obj : objectives_)
        {
            objectives_size += sizeof(uint32_t) + static_cast<uint32_t>(obj.description.size()) +
                             sizeof(uint32_t) * 3 + sizeof(bool);
        }
        
        uint32_t total_size = sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint32_t) + 
                             sizeof(uint8_t) + sizeof(uint32_t) + objectives_size;
        
        data.resize(total_size);
        
        // Write header
        PacketHeader header;
        header.type = static_cast<uint16_t>(type_);
        header.flags = 0;
        header.sequence = sequence_;
        header.size = total_size;
        header.checksum = 0;
        
        std::memcpy(data.data(), &header, sizeof(PacketHeader));
        
        // Write data
        size_t offset = sizeof(PacketHeader);
        std::memcpy(data.data() + offset, &player_id_, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(data.data() + offset, &quest_id_, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, &quest_state_, sizeof(uint8_t));
        offset += sizeof(uint8_t);        
        // Write objectives count
        std::memcpy(data.data() + offset, &objectives_count, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Write objectives
        for (const auto& obj : objectives_)
        {
            std::memcpy(data.data() + offset, &obj.objective_id, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            uint32_t desc_size = static_cast<uint32_t>(obj.description.size());
            std::memcpy(data.data() + offset, &desc_size, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            std::memcpy(data.data() + offset, obj.description.data(), desc_size);
            offset += desc_size;
            
            std::memcpy(data.data() + offset, &obj.current_progress, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            std::memcpy(data.data() + offset, &obj.required_progress, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            std::memcpy(data.data() + offset, &obj.is_completed, sizeof(bool));
            offset += sizeof(bool);
        }
        
        return data;
    }    
    auto QuestUpdatePacket::from_data(const std::vector<uint8_t>& data)
        -> std::tuple<std::unique_ptr<QuestUpdatePacket>, std::optional<std::string>>
    {
        if (data.size() < sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint32_t) + 
            sizeof(uint8_t) + sizeof(uint32_t))
        {
            return {nullptr, "Data too small for QuestUpdatePacket"};
        }
        
        auto packet = std::make_unique<QuestUpdatePacket>();
        size_t offset = sizeof(PacketHeader);
        
        std::memcpy(&packet->player_id_, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(&packet->quest_id_, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(&packet->quest_state_, data.data() + offset, sizeof(uint8_t));
        offset += sizeof(uint8_t);
        
        // Read objectives count
        uint32_t objectives_count;
        std::memcpy(&objectives_count, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Read objectives
        for (uint32_t i = 0; i < objectives_count; ++i)
        {
            if (offset + sizeof(uint32_t) > data.size())
            {
                return {nullptr, "Invalid objective data"};
            }            
            QuestObjective obj;
            std::memcpy(&obj.objective_id, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            uint32_t desc_size;
            std::memcpy(&desc_size, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            if (offset + desc_size > data.size())
            {
                return {nullptr, "Invalid objective description size"};
            }
            obj.description = std::string(reinterpret_cast<const char*>(data.data() + offset), desc_size);
            offset += desc_size;
            
            std::memcpy(&obj.current_progress, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            std::memcpy(&obj.required_progress, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            std::memcpy(&obj.is_completed, data.data() + offset, sizeof(bool));
            offset += sizeof(bool);
            
            packet->objectives_.push_back(obj);
        }
        
        return {std::move(packet), std::nullopt};
    }
    
} // namespace GameNetwork