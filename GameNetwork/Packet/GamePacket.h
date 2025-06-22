#pragma once

#include "GameNetworkConstants.h"

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <variant>
#include <optional>

namespace GameNetwork
{
    // Packet types
    enum class PacketType : uint16_t
    {
        // System packets (0-999)
        Heartbeat = 1,
        Authentication = 2,
        Disconnect = 3,
        ServerInfo = 4,
        
        // Movement packets (1000-1999)
        MoveTo = 1000,
        MoveStop = 1001,
        Teleport = 1002,
        ChangeDirection = 1003,
        
        // Combat packets (2000-2999)
        Attack = 2000,
        SkillUse = 2001,
        Damage = 2002,
        Death = 2003,
        Resurrect = 2004,
        
        // Entity packets (3000-3999)
        EntitySpawn = 3000,
        EntityDespawn = 3001,
        EntityUpdate = 3002,
        
        // Chat packets (4000-4999)
        ChatMessage = 4000,
        ChatWhisper = 4001,
        ChatGuild = 4002,
        ChatParty = 4003,
        
        // Item packets (5000-5999)
        ItemPickup = 5000,
        ItemDrop = 5001,
        ItemUse = 5002,
        ItemEquip = 5003,
        ItemUnequip = 5004,
        
        // Custom packets (10000+)
        Custom = 10000
    };
    
    // Packet header
    struct PacketHeader
    {
        uint16_t type;
        uint16_t flags;
        uint32_t sequence;
        uint32_t size;
        uint32_t checksum;
    };
    
    // Base packet class
    class GamePacket
    {
    public:
        GamePacket(PacketType type);
        virtual ~GamePacket() = default;
        
        // Type and metadata
        auto type() const -> PacketType;
        auto sequence() const -> uint32_t;
        auto set_sequence(uint32_t seq) -> void;
        
        // Serialization
        virtual auto serialize() const -> std::vector<uint8_t> = 0;
        static auto deserialize(const std::vector<uint8_t>& data) 
            -> std::tuple<std::unique_ptr<GamePacket>, std::optional<std::string>>;
        
        // Validation
        virtual auto validate() const -> std::tuple<bool, std::optional<std::string>>;
        
    protected:
        PacketType type_;
        uint32_t sequence_;
    };
    
    // Heartbeat packet
    class HeartbeatPacket : public GamePacket
    {
    public:
        HeartbeatPacket();
        
        auto timestamp() const -> uint64_t;
        auto set_timestamp(uint64_t ts) -> void;
        
        auto serialize() const -> std::vector<uint8_t> override;
        static auto from_data(const std::vector<uint8_t>& data) 
            -> std::tuple<std::unique_ptr<HeartbeatPacket>, std::optional<std::string>>;
        
    private:
        uint64_t timestamp_;
    };
    
    // Authentication packet
    class AuthenticationPacket : public GamePacket
    {
    public:
        AuthenticationPacket();
        
        auto account_id() const -> std::string;
        auto session_token() const -> std::string;
        auto client_version() const -> uint32_t;
        
        auto set_account_id(const std::string& id) -> void;
        auto set_session_token(const std::string& token) -> void;
        auto set_client_version(uint32_t version) -> void;
        
        auto serialize() const -> std::vector<uint8_t> override;
        auto deserialize(const std::vector<uint8_t>& data) 
            -> std::tuple<std::unique_ptr<GamePacket>, std::optional<std::string>>;
        static auto from_data(const std::vector<uint8_t>& data)
            -> std::tuple<std::unique_ptr<AuthenticationPacket>, std::optional<std::string>>;
        
    private:
        std::string account_id_;
        std::string session_token_;
        uint32_t client_version_;
    };
    
    // Entity spawn packet
    class EntitySpawnPacket : public GamePacket
    {
    public:
        EntitySpawnPacket();
        
        auto entity_id() const -> uint64_t;
        auto entity_type() const -> EntityType;
        auto location() const -> Location;
        auto name() const -> std::string;
        auto level() const -> uint32_t;
        auto health() const -> uint32_t;
        auto max_health() const -> uint32_t;
        
        auto set_entity_id(uint64_t id) -> void;
        auto set_entity_type(EntityType type) -> void;
        auto set_location(const Location& loc) -> void;
        auto set_name(const std::string& name) -> void;
        auto set_level(uint32_t level) -> void;
        auto set_health(uint32_t health) -> void;
        auto set_max_health(uint32_t max_health) -> void;
        
        auto serialize() const -> std::vector<uint8_t> override;
        static auto from_data(const std::vector<uint8_t>& data)
            -> std::tuple<std::unique_ptr<EntitySpawnPacket>, std::optional<std::string>>;
        
    private:
        uint64_t entity_id_;
        EntityType entity_type_;
        Location location_;
        std::string name_;
        uint32_t level_;
        uint32_t health_;
        uint32_t max_health_;
    };
    
    // Entity despawn packet
    class EntityDespawnPacket : public GamePacket
    {
    public:
        EntityDespawnPacket();
        
        auto entity_id() const -> uint64_t;
        auto reason() const -> DespawnReason;
        
        auto set_entity_id(uint64_t id) -> void;
        auto set_reason(DespawnReason reason) -> void;
        
        auto serialize() const -> std::vector<uint8_t> override;
        static auto from_data(const std::vector<uint8_t>& data)
            -> std::tuple<std::unique_ptr<EntityDespawnPacket>, std::optional<std::string>>;
        
    private:
        uint64_t entity_id_;
        DespawnReason reason_;
    };
    
    // Entity update packet
    class EntityUpdatePacket : public GamePacket
    {
    public:
        EntityUpdatePacket();
        
        auto entity_id() const -> uint64_t;
        auto location() const -> std::optional<Location>;
        auto health() const -> std::optional<uint32_t>;
        auto state() const -> std::optional<EntityState>;
        auto velocity() const -> std::optional<Vector3>;
        
        auto set_entity_id(uint64_t id) -> void;
        auto set_location(const Location& loc) -> void;
        auto set_health(uint32_t health) -> void;
        auto set_state(EntityState state) -> void;
        auto set_velocity(const Vector3& vel) -> void;
        
        auto serialize() const -> std::vector<uint8_t> override;
        static auto from_data(const std::vector<uint8_t>& data)
            -> std::tuple<std::unique_ptr<EntityUpdatePacket>, std::optional<std::string>>;
        
    private:
        uint64_t entity_id_;
        std::optional<Location> location_;
        std::optional<uint32_t> health_;
        std::optional<EntityState> state_;
        std::optional<Vector3> velocity_;
    };
}
