#pragma once

#include "BinaryGamePacket.h"
#include "PacketFragmentation.h"
#include <memory>
#include <vector>
#include <tuple>
#include <optional>

namespace GameNetwork
{
    class PacketManager
    {
    public:
        explicit PacketManager(size_t mtu = PacketFragmenter::kDefaultMTU);
        ~PacketManager();
        
        // Send side
        auto prepare_packet_for_send(const BinaryGamePacket& packet) 
            -> std::tuple<bool, std::vector<std::vector<uint8_t>>>;
        
        // Receive side
        auto process_received_data(const std::vector<uint8_t>& data) 
            -> std::tuple<bool, std::unique_ptr<BinaryGamePacket>>;
        
        // Configuration
        auto set_mtu(size_t mtu) -> void;
        auto get_mtu() const -> size_t;
        
        // Maintenance
        auto cleanup_timeout_fragments() -> size_t;
        
        // Statistics
        struct Statistics
        {
            uint64_t packets_sent;
            uint64_t packets_received;
            uint64_t packets_fragmented;
            uint64_t fragments_sent;
            uint64_t fragments_received;
            size_t pending_messages;
        };
        
        auto get_statistics() const -> Statistics;
        
    private:
        std::unique_ptr<PacketFragmenter> fragmenter_;
        std::unique_ptr<PacketReassembler> reassembler_;
        
        Statistics stats_;
        
        // Factory method - override this to support custom packet types
        virtual auto create_packet_from_type(PacketType type) -> std::unique_ptr<BinaryGamePacket>;
    };
}