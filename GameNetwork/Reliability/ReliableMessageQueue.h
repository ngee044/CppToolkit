#pragma once

#include "../GameNetworkConstants.h"
#include "../Packet/GamePacket.h"

#include <memory>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <tuple>
#include <functional>
#include <mutex>
#include <atomic>

namespace GameNetwork
{
    enum class MessageReliability
    {
        Unreliable = 0,        // Fire and forget
        Reliable = 1,          // Must be delivered
        ReliableOrdered = 2,   // Must be delivered in order
        ReliableSequenced = 3  // Latest reliable message only
    };
    
    struct ReliableMessage
    {
        uint32_t sequence;
        uint32_t channel;
        MessageReliability reliability;
        std::unique_ptr<GamePacket> packet;
        std::chrono::steady_clock::time_point sent_time;
        uint32_t retry_count;
        std::chrono::milliseconds retry_delay;
    };
    
    class ReliableMessageQueue : public std::enable_shared_from_this<ReliableMessageQueue>
    {
    public:
        ReliableMessageQueue();
        virtual ~ReliableMessageQueue();
        
        // Message sending
        auto send_message(std::unique_ptr<GamePacket> packet,
                          MessageReliability reliability = MessageReliability::Reliable,
                          uint32_t channel = 0) -> uint32_t;
        
        // Acknowledgment handling
        auto acknowledge_message(uint32_t sequence) -> void;
        auto acknowledge_messages_up_to(uint32_t sequence) -> void;
        auto handle_nack(uint32_t sequence) -> void;
        
        // Receiving
        auto receive_message(uint32_t sequence,
                             std::unique_ptr<GamePacket> packet,
                             MessageReliability reliability,
                             uint32_t channel) -> bool;
        
        auto get_deliverable_messages() -> std::vector<std::unique_ptr<GamePacket>>;
        
        // Retransmission
        auto process_retransmissions() -> std::vector<std::unique_ptr<GamePacket>>;
        auto set_retry_policy(uint32_t max_retries,
                              std::chrono::milliseconds base_delay,
                              float backoff_multiplier) -> void;
        
        // Channel management
        auto reset_channel(uint32_t channel) -> void;
        auto get_channel_sequence(uint32_t channel) const -> uint32_t;
        
        // Connection management
        auto on_connection_lost() -> void;
        auto on_connection_restored() -> void;
        
        // Statistics
        struct QueueStats
        {
            uint64_t messages_sent;
            uint64_t messages_acknowledged;
            uint64_t messages_retransmitted;
            uint64_t messages_dropped;
            uint64_t duplicate_messages_received;
            uint64_t out_of_order_messages;
            float average_rtt_ms;
            uint32_t pending_acknowledgments;
        };
        
        auto get_stats() const -> QueueStats;
        auto reset_stats() -> void;
        
    private:
        struct ChannelState
        {
            uint32_t send_sequence;
            uint32_t receive_sequence;
            uint32_t expected_sequence;
            std::unordered_map<uint32_t, std::unique_ptr<GamePacket>> received_messages;
            std::unordered_map<uint32_t, ReliableMessage> pending_messages;
        };
        
        auto get_next_sequence(uint32_t channel) -> uint32_t;
        auto is_sequence_newer(uint32_t seq1, uint32_t seq2) const -> bool;
        auto calculate_retry_delay(uint32_t retry_count) const -> std::chrono::milliseconds;
        
    private:
        mutable std::mutex mutex_;
        
        // Channel states
        std::unordered_map<uint32_t, ChannelState> channels_;
        
        // Global sequence for unordered messages
        std::atomic<uint32_t> global_sequence_;
        
        // Retry policy
        uint32_t max_retries_;
        std::chrono::milliseconds base_retry_delay_;
        float backoff_multiplier_;
        
        // Connection state
        bool connection_active_;
        
        // Statistics
        QueueStats stats_;
        
        // Constants
        static constexpr uint32_t SEQUENCE_WINDOW = 65536;
        static constexpr uint32_t MAX_PENDING_PER_CHANNEL = 1000;
    };
}
