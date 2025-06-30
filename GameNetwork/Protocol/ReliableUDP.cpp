#include "ReliableUDP.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

using namespace Utilities;

namespace GameNetwork
{
    namespace Protocol
    {
        ReliableUDP::ReliableUDP()
            : next_send_sequence_(0)
            , send_window_base_(0)
            , send_window_size_(4)
            , next_expected_sequence_(0)
            , congestion_state_(CongestionState::SlowStart)
            , congestion_window_(4)
            , slow_start_threshold_(64)
            , smooth_rtt_(std::chrono::milliseconds(100))
            , rtt_variance_(std::chrono::milliseconds(0))
            , current_rto_(std::chrono::milliseconds(200))
            , stats_{0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 4, CongestionState::SlowStart}
        {
            // Initialize with default values
        }

        auto ReliableUDP::configure(const Config& config) -> void
        {
            config_ = config;
            send_window_size_ = config.initial_window_size;
            congestion_window_ = config.initial_window_size;
            smooth_rtt_ = config.initial_rtt;
            current_rto_ = config.min_rto;
        }

        auto ReliableUDP::send_packet(std::unique_ptr<GamePacket> packet) 
            -> std::tuple<bool, std::optional<std::string>>
        {
            if (!packet)
            {
                return {false, "Invalid packet"};
            }

            std::lock_guard<std::mutex> lock(mutex_);

            uint32_t outstanding_packets = sent_packets_.size();
            if (outstanding_packets >= send_window_size_)
            {
                return {false, "Send window full"};
            }

            uint32_t sequence = get_next_sequence();
            
            packet->metadata["sequence"] = std::to_string(sequence);
            packet->metadata["reliable"] = "true";
            
            PacketInfo info;
            info.sequence = sequence;
            info.packet = packet->clone();
            info.sent_time = std::chrono::steady_clock::now();
            info.last_retry_time = info.sent_time;
            info.retry_count = 0;
            info.is_acknowledged = false;
            info.is_fast_retransmitted = false;
            
            sent_packets_[sequence] = std::move(info);
            
            stats_.packets_sent++;

            Logger::handle().write(LogTypes::Debug,
                fmt::format("ReliableUDP: Sent packet with sequence {}", sequence));

            return {true, std::nullopt};
        }

        auto ReliableUDP::get_statistics() const -> Statistics
        {
            std::lock_guard<std::mutex> lock(mutex_);
            Statistics current_stats = stats_;
            current_stats.current_window_size = send_window_size_;
            current_stats.congestion_state = congestion_state_;
            current_stats.average_rtt_ms = smooth_rtt_.count();
            
            if (stats_.packets_sent > 0)
            {
                current_stats.packet_loss_rate = 
                    static_cast<float>(stats_.packets_lost) / stats_.packets_sent;
            }
            
            return current_stats;
        }
        
        auto ReliableUDP::get_next_sequence() -> uint32_t
        {
            return next_send_sequence_++;
        }
    }
}