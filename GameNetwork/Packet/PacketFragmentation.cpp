#include "PacketFragmentation.h"

#include <Logger.h>
#include <Converter.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <cstring>
#include <algorithm>

using namespace Utilities;

namespace GameNetwork
{
    PacketFragmenter::PacketFragmenter()
        : mtu_(kDefaultMTU)
        , next_message_id_(1)
    {
        max_fragment_data_size_ = mtu_ - kFragmentHeaderSize;
    }
    
    PacketFragmenter::PacketFragmenter(size_t mtu)
        : mtu_(mtu)
        , next_message_id_(1)
    {
        max_fragment_data_size_ = mtu_ - kFragmentHeaderSize;
    }
    
    PacketFragmenter::~PacketFragmenter() = default;
    
    auto PacketFragmenter::fragment_packet(const std::vector<uint8_t>& data) 
        -> std::tuple<bool, std::vector<std::vector<uint8_t>>>
    {
        std::vector<std::vector<uint8_t>> fragments;
        
        if (data.empty())
        {
            Logger::handle().write(LogTypes::Error, fmt::format("PacketFragmenter: Empty data to fragment"));
            return {false, fragments};
        }
        
        // Check if fragmentation is needed
        if (data.size() <= max_fragment_data_size_)
        {
            // No fragmentation needed, but still add header
            FragmentHeader header;
            header.message_id = generate_message_id();
            header.fragment_index = 0;
            header.total_fragments = 1;
            header.total_size = static_cast<uint32_t>(data.size());
            header.fragment_size = static_cast<uint16_t>(data.size());
            header.reserved = 0;
            
            std::vector<uint8_t> fragment;
            fragment.resize(kFragmentHeaderSize + data.size());
            std::memcpy(fragment.data(), &header, kFragmentHeaderSize);
            std::memcpy(fragment.data() + kFragmentHeaderSize, data.data(), data.size());
            
            fragments.push_back(std::move(fragment));
            return {true, fragments};
        }
        
        // Calculate number of fragments
        size_t total_fragments = (data.size() + max_fragment_data_size_ - 1) / max_fragment_data_size_;
        
        if (total_fragments > 65535)
        {
            Logger::handle().write(LogTypes::Error, fmt::format("PacketFragmenter: Data too large, would require {} fragments", total_fragments));
            return {false, fragments};
        }
        
        uint32_t message_id = generate_message_id();
        fragments.reserve(total_fragments);
        
        for (size_t i = 0; i < total_fragments; ++i)
        {
            size_t offset = i * max_fragment_data_size_;
            size_t fragment_data_size = std::min(max_fragment_data_size_, data.size() - offset);
            
            FragmentHeader header;
            header.message_id = message_id;
            header.fragment_index = static_cast<uint16_t>(i);
            header.total_fragments = static_cast<uint16_t>(total_fragments);
            header.total_size = static_cast<uint32_t>(data.size());
            header.fragment_size = static_cast<uint16_t>(fragment_data_size);
            header.reserved = 0;
            
            std::vector<uint8_t> fragment;
            fragment.resize(kFragmentHeaderSize + fragment_data_size);
            std::memcpy(fragment.data(), &header, kFragmentHeaderSize);
            std::memcpy(fragment.data() + kFragmentHeaderSize, data.data() + offset, fragment_data_size);
            
            fragments.push_back(std::move(fragment));
        }
        
        Logger::handle().write(LogTypes::Debug, fmt::format("PacketFragmenter: Fragmented {} bytes into {} fragments", data.size(), fragments.size()));
        
        return {true, fragments};
    }
    
    auto PacketFragmenter::set_mtu(size_t mtu) -> void
    {
        mtu_ = mtu;
        max_fragment_data_size_ = mtu_ - kFragmentHeaderSize;
    }
    
    auto PacketFragmenter::get_mtu() const -> size_t
    {
        return mtu_;
    }
    
    auto PacketFragmenter::generate_message_id() -> uint32_t
    {
        return next_message_id_++;
    }

    // PacketReassembler implementation
    PacketReassembler::PacketReassembler()
    {
    }
    
    PacketReassembler::~PacketReassembler() = default;
    
    auto PacketReassembler::process_fragment(const std::vector<uint8_t>& fragment_data) 
        -> std::tuple<bool, std::optional<std::vector<uint8_t>>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Extract header
        auto [header_valid, header] = extract_header(fragment_data);
        if (!header_valid)
        {
            Logger::handle().write(LogTypes::Error, "PacketReassembler: Invalid fragment header");
            return {false, std::nullopt};
        }
        
        // Validate fragment
        if (header.fragment_index >= header.total_fragments)
        {
            Logger::handle().write(LogTypes::Error, fmt::format("PacketReassembler: Invalid fragment index {} >= {}", header.fragment_index, header.total_fragments));
            return {false, std::nullopt};
        }
        
        if (fragment_data.size() != PacketFragmenter::kFragmentHeaderSize + header.fragment_size)
        {
            Logger::handle().write(LogTypes::Error, fmt::format("PacketReassembler: Fragment size mismatch ({} != {})", fragment_data.size(), PacketFragmenter::kFragmentHeaderSize + header.fragment_size));
            return {false, std::nullopt};
        }
        
        // Find or create pending message
        auto& pending = pending_messages_[header.message_id];
        
        if (pending.message_id == 0)
        {
            // New message
            pending.message_id = header.message_id;
            pending.total_fragments = header.total_fragments;
            pending.received_fragments = 0;
            pending.total_size = header.total_size;
            pending.fragments.resize(header.total_fragments);
            pending.fragment_received.resize(header.total_fragments, false);
            pending.first_fragment_time = std::chrono::steady_clock::now();
        }
        
        pending.last_fragment_time = std::chrono::steady_clock::now();
        
        // Check for duplicate fragment
        if (pending.fragment_received[header.fragment_index])
        {
            Logger::handle().write(LogTypes::Debug, fmt::format("PacketReassembler: Duplicate fragment received"));
            return {false, std::nullopt};
        }
        
        // Store fragment data (without header)
        pending.fragments[header.fragment_index].assign(
            fragment_data.begin() + PacketFragmenter::kFragmentHeaderSize,
            fragment_data.end()
        );
        pending.fragment_received[header.fragment_index] = true;
        pending.received_fragments++;
        
        // Check if message is complete
        if (pending.received_fragments == pending.total_fragments)
        {
            // Reassemble message
            std::vector<uint8_t> complete_message;
            complete_message.reserve(pending.total_size);
            
            for (const auto& fragment : pending.fragments)
            {
                complete_message.insert(complete_message.end(), fragment.begin(), fragment.end());
            }
            
            pending_messages_.erase(header.message_id);
            
            Logger::handle().write(LogTypes::Debug, fmt::format("PacketReassembler: Message {} reassembled ({} bytes)", header.message_id, complete_message.size()));
            
            return {true, complete_message};
        }
        
        return {false, std::nullopt};
    }
    
    auto PacketReassembler::cleanup_timeout_messages(std::chrono::seconds timeout) -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        size_t cleaned = 0;
        
        for (auto it = pending_messages_.begin(); it != pending_messages_.end();)
        {
            if (now - it->second.last_fragment_time > timeout)
            {
                Logger::handle().write(LogTypes::Debug, fmt::format("PacketReassembler: Message {} timed out ({} / {} fragments)", it->second.message_id, it->second.received_fragments, it->second.total_fragments));

                it = pending_messages_.erase(it);
                cleaned++;
            }
            else
            {
                ++it;
            }
        }
        
        return cleaned;
    }
    
    auto PacketReassembler::get_pending_count() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_messages_.size();
    }
    
    auto PacketReassembler::extract_header(const std::vector<uint8_t>& data) 
        -> std::tuple<bool, PacketFragmenter::FragmentHeader>
    {
        if (data.size() < PacketFragmenter::kFragmentHeaderSize)
        {
            return {false, PacketFragmenter::FragmentHeader{}};
        }
        
        PacketFragmenter::FragmentHeader header;
        std::memcpy(&header, data.data(), PacketFragmenter::kFragmentHeaderSize);
        return {true, header};
    }
}