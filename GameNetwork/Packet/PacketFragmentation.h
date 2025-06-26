#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <mutex>
#include <tuple>
#include <optional>
#include "../GameNetworkConstants.h"

namespace GameNetwork
{
    namespace Packet
    {
        struct FragmentHeader
        {
            uint32_t message_id;        // Unique ID for the complete message
            uint16_t fragment_index;    // Index of this fragment (0-based)
            uint16_t total_fragments;   // Total number of fragments
            uint32_t total_size;        // Total size of the complete message
            uint16_t fragment_size;     // Size of this fragment's data
            uint8_t flags;              // Various flags (compression, encryption, etc.)
            uint8_t reserved;           // Reserved for future use
        };

        constexpr size_t kFragmentHeaderSize = sizeof(FragmentHeader);
        constexpr size_t kDefaultMTU = 1400;  // Conservative MTU for internet
        constexpr size_t kMaxFragmentDataSize = kDefaultMTU - kFragmentHeaderSize - 20;  // 20 for IP header

        enum class FragmentFlags : uint8_t
        {
            None = 0x00,
            Compressed = 0x01,
            Encrypted = 0x02,
            Priority = 0x04,
            Reliable = 0x08
        };

        class PacketFragmenter
        {
        public:
            explicit PacketFragmenter(size_t mtu = kDefaultMTU);
            ~PacketFragmenter();

            // Fragment a large packet into smaller pieces
            auto fragment_packet(const std::vector<uint8_t>& data, uint32_t message_id) 
                -> std::vector<std::vector<uint8_t>>;

            // Set MTU (Maximum Transmission Unit)
            auto set_mtu(size_t mtu) -> void;
            auto get_mtu() const -> size_t;

            // Get maximum data size per fragment
            auto get_max_fragment_size() const -> size_t;

        private:
            size_t mtu_;
            size_t max_fragment_data_size_;
        };

        struct PendingMessage
        {
            uint32_t message_id;
            uint16_t total_fragments;
            uint16_t received_fragments;
            uint32_t total_size;
            std::vector<std::vector<uint8_t>> fragments;
            std::vector<bool> fragment_received;
            std::chrono::steady_clock::time_point first_fragment_time;
            std::chrono::steady_clock::time_point last_fragment_time;
        };

        class PacketReassembler
        {
        public:
            PacketReassembler();
            ~PacketReassembler();

            // Process a fragment and check if message is complete
            auto process_fragment(const std::vector<uint8_t>& fragment_data) 
                -> std::tuple<bool, std::optional<std::vector<uint8_t>>>;

            // Clean up old incomplete messages
            auto cleanup_timeout_messages(std::chrono::seconds timeout = std::chrono::seconds(30)) 
                -> size_t;

            // Get statistics
            auto get_pending_message_count() const -> size_t;
            auto get_total_fragments_received() const -> uint64_t;
            auto get_total_messages_completed() const -> uint64_t;
            auto get_total_messages_timeout() const -> uint64_t;

            // Reset all state
            auto reset() -> void;

        private:
            auto extract_header(const std::vector<uint8_t>& data) 
                -> std::tuple<FragmentHeader, bool>;

            auto reassemble_message(const PendingMessage& pending) 
                -> std::vector<uint8_t>;

        private:
            mutable std::mutex mutex_;
            std::unordered_map<uint32_t, PendingMessage> pending_messages_;
            
            // Statistics
            uint64_t total_fragments_received_;
            uint64_t total_messages_completed_;
            uint64_t total_messages_timeout_;
        };

        // High-level fragmentation manager that combines fragmenter and reassembler
        class FragmentationManager
        {
        public:
            explicit FragmentationManager(size_t mtu = kDefaultMTU);
            ~FragmentationManager();

            // Send side: fragment if necessary
            auto prepare_for_send(const std::vector<uint8_t>& data) 
                -> std::vector<std::vector<uint8_t>>;

            // Receive side: reassemble fragments
            auto process_received(const std::vector<uint8_t>& data) 
                -> std::tuple<bool, std::optional<std::vector<uint8_t>>>;

            // Configuration
            auto set_mtu(size_t mtu) -> void;
            auto get_mtu() const -> size_t;

            // Maintenance
            auto cleanup() -> size_t;

            // Statistics
            struct Statistics
            {
                uint64_t messages_fragmented;
                uint64_t messages_reassembled;
                uint64_t fragments_sent;
                uint64_t fragments_received;
                uint64_t reassembly_timeouts;
                size_t pending_messages;
            };

            auto get_statistics() const -> Statistics;
            auto reset_statistics() -> void;

        private:
            auto generate_message_id() -> uint32_t;

        private:
            std::unique_ptr<PacketFragmenter> fragmenter_;
            std::unique_ptr<PacketReassembler> reassembler_;
            
            std::atomic<uint32_t> next_message_id_;
            
            // Statistics
            std::atomic<uint64_t> messages_fragmented_;
            std::atomic<uint64_t> fragments_sent_;
        };
    }
}