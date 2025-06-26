#include "PacketManager.h"
#include "GamePacketExamples.h"
#include "../../Utilities/Logger.h"

using namespace Utilities;

namespace GameNetwork
{
    PacketManager::PacketManager(size_t mtu)
        : fragmenter_(std::make_unique<PacketFragmenter>(mtu))
        , reassembler_(std::make_unique<PacketReassembler>())
    {
        std::memset(&stats_, 0, sizeof(stats_));
    }
    
    PacketManager::~PacketManager() = default;
    
    auto PacketManager::prepare_packet_for_send(const BinaryGamePacket& packet) 
        -> std::tuple<bool, std::vector<std::vector<uint8_t>>>
    {
        // Serialize packet
        std::vector<uint8_t> serialized_data = packet.serialize();
        
        // Fragment if necessary
        auto [success, fragments] = fragmenter_->fragment_packet(serialized_data);
        
        if (success)
        {
            stats_.packets_sent++;
            if (fragments.size() > 1)
            {
                stats_.packets_fragmented++;
                stats_.fragments_sent += fragments.size();
            }
            else
            {
                stats_.fragments_sent++;
            }
        }
        
        return {success, fragments};
    }
    
    auto PacketManager::process_received_data(const std::vector<uint8_t>& data) 
        -> std::tuple<bool, std::unique_ptr<BinaryGamePacket>>
    {
        stats_.fragments_received++;
        
        // Process through reassembler
        auto [is_complete, complete_data] = reassembler_->process_fragment(data);
        
        if (!is_complete || !complete_data.has_value())
        {
            return {false, nullptr};
        }
        
        // Deserialize packet
        BinaryBuffer buffer;
        buffer.write_bytes(complete_data->data(), complete_data->size());
        buffer.reset_read_position();
        
        // Read packet type from header (skip serialization type)
        buffer.read_uint8();  // Skip serialization type
        auto [type_success, packet_type_value] = buffer.read_uint16();
        
        if (!type_success)
        {
            Logger::handle().write(LogTypes::Error, "PacketManager: Failed to read packet type");
            return {false, nullptr};
        }
        
        PacketType packet_type = static_cast<PacketType>(packet_type_value);
        
        // Create appropriate packet type
        auto packet = create_packet_from_type(packet_type);
        if (!packet)
        {
            Logger::handle().write(LogTypes::Error, "PacketManager: Unknown packet type " + 
                         std::to_string(static_cast<uint16_t>(packet_type)));
            return {false, nullptr};
        }
        
        // Deserialize packet data
        if (!packet->deserialize(*complete_data))
        {
            Logger::handle().write(LogTypes::Error, "PacketManager: Failed to deserialize packet");
            return {false, nullptr};
        }
        
        stats_.packets_received++;
        return {true, std::move(packet)};
    }
    
    auto PacketManager::set_mtu(size_t mtu) -> void
    {
        fragmenter_->set_mtu(mtu);
    }
    
    auto PacketManager::get_mtu() const -> size_t
    {
        return fragmenter_->get_mtu();
    }
    
    auto PacketManager::cleanup_timeout_fragments() -> size_t
    {
        return reassembler_->cleanup_timeout_messages();
    }
    
    auto PacketManager::get_statistics() const -> Statistics
    {
        Statistics current_stats = stats_;
        current_stats.pending_messages = reassembler_->get_pending_count();
        return current_stats;
    }
    
    auto PacketManager::create_packet_from_type(PacketType type) -> std::unique_ptr<BinaryGamePacket>
    {
        // Create appropriate packet based on type
        switch (type)
        {
            case PacketType::MoveTo:
                return std::make_unique<MovePacket>();
                
            case PacketType::ChatMessage:
                return std::make_unique<ChatPacket>();
                
            default:
                return std::make_unique<BinaryGamePacket>(type);
        }
    }
}