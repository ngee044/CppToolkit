#pragma once

#include <GameNetworkConstants.h>
#include <GamePacket.h>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <queue>
#include <tuple>
#include <optional>
#include <atomic>
#include <mutex>
#include <vector>

namespace GameNetwork
{
    namespace Protocol
    {
        // Selective ACK information
        struct SelectiveAck
        {
            uint32_t base_sequence;
            std::vector<uint32_t> received_sequences; // Bitmap or explicit sequences
            std::chrono::steady_clock::time_point timestamp;
        };

        // Congestion control state
        enum class CongestionState
        {
            SlowStart,
            CongestionAvoidance,
            FastRecovery,
            FastRetransmit
        };

        // Packet tracking information
        struct PacketInfo
        {
            uint32_t sequence;
            std::unique_ptr<GamePacket> packet;
            std::chrono::steady_clock::time_point sent_time;
            std::chrono::steady_clock::time_point last_retry_time;
            uint32_t retry_count;
            bool is_acknowledged;
            bool is_fast_retransmitted;
        };

        class ReliableUDP
        {
        public:
            ReliableUDP();
            virtual ~ReliableUDP() = default;

            // Configuration
            struct Config
            {
                uint32_t max_window_size = 64;
                uint32_t initial_window_size = 4;
                std::chrono::milliseconds initial_rtt = std::chrono::milliseconds(100);
                std::chrono::milliseconds min_rto = std::chrono::milliseconds(200);
                std::chrono::milliseconds max_rto = std::chrono::milliseconds(60000);
                float rtt_alpha = 0.125f; // RTT smoothing factor
                uint32_t duplicate_ack_threshold = 3;
                uint32_t max_retries = 10;
            };

            auto configure(const Config& config) -> void;

            // Packet handling
            auto send_packet(std::unique_ptr<GamePacket> packet) 
                -> std::tuple<bool, std::optional<std::string>>;
            
            auto receive_packet(std::unique_ptr<GamePacket> packet) 
                -> std::tuple<bool, std::optional<std::string>>;

            // Out of order packet handling  
            auto handle_out_of_order_packets() -> void;
            
            // Selective acknowledgment
            auto implement_selective_ack() -> void;
            auto process_selective_ack(const SelectiveAck& sack) -> void;
            auto generate_selective_ack() -> SelectiveAck;

            // Congestion control
            auto congestion_control() -> void;
            auto on_packet_acknowledged(uint32_t sequence) -> void;
            auto on_packet_lost(uint32_t sequence) -> void;
            auto on_duplicate_ack(uint32_t sequence) -> void;
            
            // Retransmission
            auto process_retransmissions() -> std::vector<std::unique_ptr<GamePacket>>;
            
            // Statistics
            struct Statistics
            {
                uint64_t packets_sent;
                uint64_t packets_received;
                uint64_t packets_acknowledged;
                uint64_t packets_retransmitted;
                uint64_t packets_lost;
                uint64_t out_of_order_packets;
                uint64_t duplicate_acks;
                float average_rtt_ms;
                float packet_loss_rate;
                uint32_t current_window_size;
                CongestionState congestion_state;
            };

            auto get_statistics() const -> Statistics;
            auto reset_statistics() -> void;

        private:
            // Window management
            auto adjust_window_size(bool increase) -> void;
            auto calculate_rto() -> std::chrono::milliseconds;
            auto update_rtt(std::chrono::milliseconds measured_rtt) -> void;
            
            // Sequence number handling
            auto get_next_sequence() -> uint32_t;
            auto is_sequence_in_window(uint32_t sequence) const -> bool;
            auto advance_window() -> void;

            // Packet buffering
            auto buffer_out_of_order_packet(uint32_t sequence, std::unique_ptr<GamePacket> packet) -> void;
            auto deliver_buffered_packets() -> std::vector<std::unique_ptr<GamePacket>>;

        private:
            Config config_;
            mutable std::mutex mutex_;

            // Sending side
            std::atomic<uint32_t> next_send_sequence_;
            std::unordered_map<uint32_t, PacketInfo> sent_packets_;
            std::atomic<uint32_t> send_window_base_;
            std::atomic<uint32_t> send_window_size_;

            // Receiving side  
            std::atomic<uint32_t> next_expected_sequence_;
            std::unordered_map<uint32_t, std::unique_ptr<GamePacket>> received_buffer_;
            std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> 
                out_of_order_sequences_;

            // Congestion control
            CongestionState congestion_state_;
            std::atomic<uint32_t> congestion_window_;
            std::atomic<uint32_t> slow_start_threshold_;
            std::unordered_map<uint32_t, uint32_t> duplicate_ack_count_;

            // RTT estimation
            std::chrono::milliseconds smooth_rtt_;
            std::chrono::milliseconds rtt_variance_;
            std::chrono::milliseconds current_rto_;

            // Statistics
            Statistics stats_;
        };
    }
}