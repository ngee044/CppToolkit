#pragma once

#include <GameNetworkConstants.h>
#include <GamePacket.h>

#include <memory>
#include <tuple>
#include <optional>
#include <functional>
#include <mutex>

namespace GameNetwork
{
    namespace Protocol
    {
        enum class ProtocolType
        {
            TCP,
            UDP,
            ReliableUDP
        };

        enum class PacketPriority
        {
            Low,      // Non-critical updates (e.g., ambient sounds)
            Normal,   // Regular game updates
            High,     // Important game events (e.g., damage, items)
            Critical  // Must be reliable (e.g., login, transactions)
        };

        class HybridProtocol
        {
        public:
            HybridProtocol() = default;
            virtual ~HybridProtocol() = default;

            // Routing decision based on packet type and priority
            auto route_by_packet_priority(const GamePacket& packet) -> ProtocolType;
            
            // Set routing rules
            auto set_routing_rule(PacketType type, ProtocolType protocol) -> void;
            auto set_priority_rule(PacketPriority priority, ProtocolType protocol) -> void;
            
            // Override default routing
            using RoutingFunction = std::function<ProtocolType(const GamePacket&)>;
            auto set_custom_router(RoutingFunction router) -> void;            
            // Get packet info
            auto get_packet_priority(const GamePacket& packet) const -> PacketPriority;
            auto should_use_reliable_transport(const GamePacket& packet) const -> bool;
            
            // Statistics
            struct RoutingStats
            {
                uint64_t tcp_packets;
                uint64_t udp_packets;
                uint64_t reliable_udp_packets;
                uint64_t routing_decisions;
            };
            
            auto get_statistics() const -> RoutingStats;
            
        private:
            // Default routing logic
            auto default_routing(const GamePacket& packet) const -> ProtocolType;
            
        private:
            std::unordered_map<PacketType, ProtocolType> type_routing_;
            std::unordered_map<PacketPriority, ProtocolType> priority_routing_;
            RoutingFunction custom_router_;
            
            mutable RoutingStats stats_;
            mutable std::mutex mutex_;
        };
    }
}