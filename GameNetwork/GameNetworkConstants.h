#pragma once

#include <cstdint>
#include <chrono>

namespace GameNetwork
{
    // Network configuration
    constexpr uint16_t DEFAULT_HIGH_PRIORITY_THREADS = 4;
    constexpr uint16_t DEFAULT_NORMAL_PRIORITY_THREADS = 8;
    constexpr uint16_t DEFAULT_LOW_PRIORITY_THREADS = 4;
    
    constexpr size_t DEFAULT_SOCKET_BUFFER_SIZE = 65536;
    constexpr size_t MAX_PACKET_SIZE = 65536;
    constexpr size_t PACKET_HEADER_SIZE = 16;
    
    // Timeouts
    constexpr auto HEARTBEAT_INTERVAL = std::chrono::seconds(30);
    constexpr auto SESSION_TIMEOUT = std::chrono::seconds(300);
    constexpr auto RECONNECT_GRACE_PERIOD = std::chrono::seconds(30);
    
    // Game specific
    constexpr uint32_t INVALID_SESSION_ID = 0;
    constexpr uint32_t INVALID_CHANNEL_ID = 0;
    constexpr uint32_t MAX_CHANNELS_PER_SERVER = 10;
    
    // Packet priorities
    enum class PacketPriority : uint8_t
    {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };
    
    // Connection states
    enum class ConnectionState : uint8_t
    {
        Disconnected = 0,
        Connecting = 1,
        Connected = 2,
        Authenticating = 3,
        Authenticated = 4,
        InGame = 5,
        Disconnecting = 6,
        Reconnecting = 7,
        Failed = 8
    };
    
    // Reconnection configuration
    constexpr uint32_t MAX_RECONNECT_ATTEMPTS = 5;
    constexpr auto INITIAL_RECONNECT_DELAY = std::chrono::seconds(1);
    constexpr auto MAX_RECONNECT_DELAY = std::chrono::seconds(30);
    constexpr float RECONNECT_BACKOFF_MULTIPLIER = 2.0f;
    
    // Session states
    enum class SessionState : uint8_t
    {
        Inactive = 0,
        Active = 1,
        Suspended = 2,
        Migrating = 3,
        Terminating = 4
    };
    
    // Game structures
    struct Location
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        uint32_t map_id = 0;
        uint32_t channel_id = 0;
    };
    
    struct Vector3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };
    
    // Entity types
    enum class EntityType : uint8_t
    {
        Player = 0,
        NPC = 1,
        Monster = 2,
        Item = 3,
        Object = 4
    };
    
    // Entity states
    enum class EntityState : uint8_t
    {
        Idle = 0,
        Moving = 1,
        Attacking = 2,
        Casting = 3,
        Dead = 4,
        Interacting = 5
    };
    
    // Despawn reasons
    enum class DespawnReason : uint8_t
    {
        OutOfRange = 0,
        Death = 1,
        Disconnect = 2,
        Teleport = 3,
        ChannelChange = 4
    };
}
