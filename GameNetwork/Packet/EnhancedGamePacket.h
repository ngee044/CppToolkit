#pragma once

#include "GamePacket.h"
#include "GamePacket_Binary.h"

namespace GameNetwork
{
    // Adapter to integrate new binary/fragmentation features with existing GamePacket system
    class EnhancedGamePacket : public GamePacket
    {
    public:
        EnhancedGamePacket();
        explicit EnhancedGamePacket(PacketType type);
        virtual ~EnhancedGamePacket();

        // Override to add binary serialization support
        auto serialize() const -> std::vector<uint8_t> override;
        auto deserialize(const std::vector<uint8_t>& data) -> bool override;

        // Configuration
        auto use_binary_format(bool use_binary = true) -> void;
        auto is_binary_format() const -> bool;

    protected:
        // Override these in derived classes for custom binary serialization
        virtual auto write_binary_data(Serialization::BinarySerializer& serializer) const -> void {}
        virtual auto read_binary_data(Serialization::BinaryDeserializer& deserializer) -> bool { return true; }

    private:
        bool use_binary_;
        static PacketTransmitter* global_transmitter_;  // Shared transmitter for all packets
    };

    // Helper macro for easy packet definition
    #define DEFINE_BINARY_PACKET(ClassName, PacketTypeValue) \
        class ClassName : public EnhancedGamePacket { \
        public: \
            ClassName() : EnhancedGamePacket(PacketTypeValue) { use_binary_format(true); } \
            static constexpr PacketType TYPE = PacketTypeValue;

    #define END_BINARY_PACKET };

    // Example usage:
    /*
    DEFINE_BINARY_PACKET(LoginPacket, PacketType::Login)
        std::string username;
        std::string password_hash;
        uint32_t client_version;
        
    protected:
        auto write_binary_data(Serialization::BinarySerializer& s) const -> void override
        {
            s.write_string(username);
            s.write_string(password_hash);
            s.write_uint32(client_version);
        }
        
        auto read_binary_data(Serialization::BinaryDeserializer& d) -> bool override
        {
            auto [u, u_ok] = d.read_string();
            auto [p, p_ok] = d.read_string();
            auto [v, v_ok] = d.read_uint32();
            
            if (!u_ok || !p_ok || !v_ok) return false;
            
            username = u;
            password_hash = p;
            client_version = v;
            return true;
        }
    END_BINARY_PACKET
    */
}