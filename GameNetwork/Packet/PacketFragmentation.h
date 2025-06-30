#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <tuple>
#include <optional>

namespace GameNetwork
{
	class PacketFragmenter
	{
	public:
		static constexpr size_t kDefaultMTU = 1400;
		static constexpr size_t kFragmentHeaderSize = 16;
		static constexpr size_t kMaxFragmentDataSize = kDefaultMTU - kFragmentHeaderSize;
        
		struct FragmentHeader
		{
			uint32_t message_id;
			uint16_t fragment_index;
			uint16_t total_fragments;
			uint32_t total_size;
			uint16_t fragment_size;
			uint16_t reserved;
		};
        
		PacketFragmenter();
		explicit PacketFragmenter(size_t mtu);
		~PacketFragmenter();
        
		// Fragment a large packet
		auto fragment_packet(const std::vector<uint8_t>& data) 
			-> std::tuple<bool, std::vector<std::vector<uint8_t>>>;
        
		// Set MTU
		auto set_mtu(size_t mtu) -> void;
		auto get_mtu() const -> size_t;
        
	private:
		size_t mtu_;
		size_t max_fragment_data_size_;
		uint32_t next_message_id_;
        
		auto generate_message_id() -> uint32_t;
	};
    
	class PacketReassembler
	{
	public:
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
        
		PacketReassembler();
		~PacketReassembler();
        
		// Process a fragment
		auto process_fragment(const std::vector<uint8_t>& fragment_data) 
			-> std::tuple<bool, std::optional<std::vector<uint8_t>>>;
        
		// Cleanup old messages
		auto cleanup_timeout_messages(std::chrono::seconds timeout = std::chrono::seconds(30)) 
			-> size_t;
        
		// Get statistics
		auto get_pending_count() const -> size_t;
        
	private:
		mutable std::mutex mutex_;
		std::unordered_map<uint32_t, PendingMessage> pending_messages_;
        
		auto extract_header(const std::vector<uint8_t>& data) 
			-> std::tuple<bool, PacketFragmenter::FragmentHeader>;
	};
}