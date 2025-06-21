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
}