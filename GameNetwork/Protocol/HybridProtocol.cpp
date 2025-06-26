#include "HybridProtocol.h"
#include <Logger.h>

using namespace Utilities;

namespace GameNetwork
{
    namespace Protocol
    {
        auto HybridProtocol::route_by_packet_priority(const GamePacket& packet) -> ProtocolType
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            stats_.routing_decisions++;
            
            // Use custom router if set
            if (custom_router_)
            {
                auto protocol = custom_router_(packet);
                
                // Update statistics
                switch (protocol)
                {
                    case ProtocolType::TCP:
                        stats_.tcp_packets++;
                        break;
                    case ProtocolType::UDP:
                        stats_.udp_packets++;
                        break;
                    case ProtocolType::ReliableUDP:
                        stats_.reliable_udp_packets++;
                        break;
                }
                
                return protocol;
            }
            
            // Check type-based routing
            auto type_it = type_routing_.find(packet.packet_type);
            if (type_it != type_routing_.end())
            {
                auto protocol = type_it->second;
                
                // Update statistics
                switch (protocol)
                {
                    case ProtocolType::TCP:
                        stats_.tcp_packets++;
                        break;
                    case ProtocolType::UDP:
                        stats_.udp_packets++;
                        break;
                    case ProtocolType::ReliableUDP:
                        stats_.reliable_udp_packets++;
                        break;
                }
                
                return protocol;
            }

            // Check priority-based routing
            auto priority = get_packet_priority(packet);
            auto priority_it = priority_routing_.find(priority);
            if (priority_it != priority_routing_.end())
            {
                auto protocol = priority_it->second;
                
                // Update statistics
                switch (protocol)
                {
                    case ProtocolType::TCP:
                        stats_.tcp_packets++;
                        break;
                    case ProtocolType::UDP:
                        stats_.udp_packets++;
                        break;
                    case ProtocolType::ReliableUDP:
                        stats_.reliable_udp_packets++;
                        break;
                }
                
                return protocol;
            }
            
            // Use default routing
            return default_routing(packet);
        }

        auto HybridProtocol::set_routing_rule(PacketType type, ProtocolType protocol) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            type_routing_[type] = protocol;
        }

        auto HybridProtocol::set_priority_rule(PacketPriority priority, ProtocolType protocol) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            priority_routing_[priority] = protocol;
        }

        auto HybridProtocol::set_custom_router(RoutingFunction router) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            custom_router_ = router;
        }

        auto HybridProtocol::get_packet_priority(const GamePacket& packet) const -> PacketPriority
        {
            // Determine priority based on packet type
            switch (packet.packet_type)
            {
                // Critical packets - must be reliable
                case static_cast<uint16_t>(PacketType::Authentication):
                case static_cast<uint16_t>(PacketType::SessionCreate):
                case static_cast<uint16_t>(PacketType::SessionDestroy):
                case static_cast<uint16_t>(PacketType::Transaction):
                    return PacketPriority::Critical;
                
                // High priority packets
                case static_cast<uint16_t>(PacketType::ChatMessage):
                case static_cast<uint16_t>(PacketType::ItemPickup):
                case static_cast<uint16_t>(PacketType::DamageEvent):
                case static_cast<uint16_t>(PacketType::StateChange):
                    return PacketPriority::High;
                
                // Low priority packets
                case static_cast<uint16_t>(PacketType::PositionUpdate):
                case static_cast<uint16_t>(PacketType::RotationUpdate):
                case static_cast<uint16_t>(PacketType::AnimationUpdate):
                    return PacketPriority::Low;
                
                // Default to normal priority
                default:
                    return PacketPriority::Normal;
            }
        }

        auto HybridProtocol::should_use_reliable_transport(const GamePacket& packet) const -> bool
        {
            auto priority = get_packet_priority(packet);
            return priority == PacketPriority::Critical || priority == PacketPriority::High;
        }

        auto HybridProtocol::get_statistics() const -> RoutingStats
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return stats_;
        }

        auto HybridProtocol::default_routing(const GamePacket& packet) const -> ProtocolType
        {
            auto priority = get_packet_priority(packet);
            
            ProtocolType protocol;
            
            switch (priority)
            {
                case PacketPriority::Critical:
                    // Critical packets always use TCP for guaranteed delivery
                    protocol = ProtocolType::TCP;
                    stats_.tcp_packets++;
                    break;
                    
                case PacketPriority::High:
                    // High priority packets use ReliableUDP for lower latency with reliability
                    protocol = ProtocolType::ReliableUDP;
                    stats_.reliable_udp_packets++;
                    break;
                    
                case PacketPriority::Low:
                    // Low priority packets use UDP for minimum latency
                    protocol = ProtocolType::UDP;
                    stats_.udp_packets++;
                    break;
                    
                case PacketPriority::Normal:
                default:
                    // Normal packets use ReliableUDP as a balance
                    protocol = ProtocolType::ReliableUDP;
                    stats_.reliable_udp_packets++;
                    break;
            }
            
            Logger::handle().write(LogTypes::Debug,
                "Routing packet type " + std::to_string(static_cast<uint16_t>(packet.packet_type)) + 
                " via " + std::to_string(static_cast<int>(protocol)));
            
            return protocol;
        }
    }
}
