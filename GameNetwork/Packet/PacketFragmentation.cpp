#include "PacketFragmentation.h"
#include "../../Utilities/Logger.h"
#include <algorithm>
#include <cstring>

namespace GameNetwork
{
    namespace Packet
    {
        // PacketFragmenter implementation
        PacketFragmenter::PacketFragmenter(size_t mtu)
            : mtu_(mtu)
        {
            max_fragment_data_size_ = mtu_ - kFragmentHeaderSize - 20;  // 20 for IP header overhead
        }

        PacketFragmenter::~PacketFragmenter() = default;

        auto PacketFragmenter::fragment_packet(const std::vector<uint8_t>& data, uint32_t message_id) 
            -> std::vector<std::vector<uint8_t>>
        {
            std::vector<std::vector<uint8_t>> fragments;
            
            // Check if fragmentation is needed
            if (data.size() <= max_fragment_data_size_)
            {
                // No fragmentation needed, but still add header for consistency
                FragmentHeader header;
                header.message_id = message_id;
                header.fragment_index = 0;
                header.total_fragments = 1;
                header.total_size = static_cast<uint32_t>(data.size());
                header.fragment_size = static_cast<uint16_t>(data.size());
                header.flags = 0;
                header.reserved = 0;
                
                std::vector<uint8_t> fragment;
                fragment.resize(kFragmentHeaderSize + data.size());
                std::memcpy(fragment.data(), &header, kFragmentHeaderSize);
                std::memcpy(fragment.data() + kFragmentHeaderSize, data.data(), data.size());
                
                fragments.push_back(std::move(fragment));
                return fragments;
            }
            
            // Calculate number of fragments needed
            size_t total_fragments = (data.size() + max_fragment_data_size_ - 1) / max_fragment_data_size_;
            if (total_fragments > std::numeric_limits<uint16_t>::max())
            {
                Utilities::Logger::error("Message too large to fragment: " + 
                    std::to_string(data.size()) + " bytes would require " + 
                    std::to_string(total_fragments) + " fragments");
                return {};
            }
            
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
                header.flags = 0;
                header.reserved = 0;
                
                std::vector<uint8_t> fragment;
                fragment.resize(kFragmentHeaderSize + fragment_data_size);
                std::memcpy(fragment.data(), &header, kFragmentHeaderSize);
                std::memcpy(fragment.data() + kFragmentHeaderSize, 
                           data.data() + offset, fragment_data_size);
                
                fragments.push_back(std::move(fragment));
            }
            
            return fragments;
        }

        auto PacketFragmenter::set_mtu(size_t mtu) -> void
        {
            mtu_ = mtu;
            max_fragment_data_size_ = mtu_ - kFragmentHeaderSize - 20;
        }

        auto PacketFragmenter::get_mtu() const -> size_t
        {
            return mtu_;
        }

        auto PacketFragmenter::get_max_fragment_size() const -> size_t
        {
            return max_fragment_data_size_;
        }

        // PacketReassembler implementation
        PacketReassembler::PacketReassembler()
            : total_fragments_received_(0)
            , total_messages_completed_(0)
            , total_messages_timeout_(0)
        {
        }

        PacketReassembler::~PacketReassembler() = default;

        auto PacketReassembler::process_fragment(const std::vector<uint8_t>& fragment_data) 
            -> std::tuple<bool, std::optional<std::vector<uint8_t>>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Extract header
            auto [header, header_valid] = extract_header(fragment_data);
            if (!header_valid)
            {
                Utilities::Logger::warning("Invalid fragment header");
                return {false, std::nullopt};
            }
            
            // Validate fragment
            if (header.fragment_index >= header.total_fragments)
            {
                Utilities::Logger::warning("Invalid fragment index: " + 
                    std::to_string(header.fragment_index) + " >= " + 
                    std::to_string(header.total_fragments));
                return {false, std::nullopt};
            }
            
            if (fragment_data.size() != kFragmentHeaderSize + header.fragment_size)
            {
                Utilities::Logger::warning("Fragment size mismatch");
                return {false, std::nullopt};
            }
            
            total_fragments_received_++;
            
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
                Utilities::Logger::debug("Duplicate fragment received: message " + 
                    std::to_string(header.message_id) + ", fragment " + 
                    std::to_string(header.fragment_index));
                return {false, std::nullopt};
            }
            
            // Store fragment data (without header)
            pending.fragments[header.fragment_index].assign(
                fragment_data.begin() + kFragmentHeaderSize,
                fragment_data.end()
            );
            pending.fragment_received[header.fragment_index] = true;
            pending.received_fragments++;
            
            // Check if message is complete
            if (pending.received_fragments == pending.total_fragments)
            {
                auto complete_message = reassemble_message(pending);
                pending_messages_.erase(header.message_id);
                total_messages_completed_++;
                
                Utilities::Logger::debug("Message reassembled: " + 
                    std::to_string(header.message_id) + " (" + 
                    std::to_string(complete_message.size()) + " bytes)");
                
                return {true, complete_message};
            }
            
            return {false, std::nullopt};
        }

        auto PacketReassembler::cleanup_timeout_messages(std::chrono::seconds timeout) 
            -> size_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto now = std::chrono::steady_clock::now();
            size_t cleaned = 0;
            
            for (auto it = pending_messages_.begin(); it != pending_messages_.end();)
            {
                if (now - it->second.last_fragment_time > timeout)
                {
                    Utilities::Logger::debug("Message timeout: " + 
                        std::to_string(it->second.message_id) + " (" + 
                        std::to_string(it->second.received_fragments) + "/" + 
                        std::to_string(it->second.total_fragments) + " fragments)");
                    
                    it = pending_messages_.erase(it);
                    cleaned++;
                    total_messages_timeout_++;
                }
                else
                {
                    ++it;
                }
            }
            
            return cleaned;
        }

        auto PacketReassembler::get_pending_message_count() const -> size_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return pending_messages_.size();
        }

        auto PacketReassembler::get_total_fragments_received() const -> uint64_t
        {
            return total_fragments_received_;
        }

        auto PacketReassembler::get_total_messages_completed() const -> uint64_t
        {
            return total_messages_completed_;
        }

        auto PacketReassembler::get_total_messages_timeout() const -> uint64_t
        {
            return total_messages_timeout_;
        }

        auto PacketReassembler::reset() -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_messages_.clear();
            total_fragments_received_ = 0;
            total_messages_completed_ = 0;
            total_messages_timeout_ = 0;
        }

        auto PacketReassembler::extract_header(const std::vector<uint8_t>& data) 
            -> std::tuple<FragmentHeader, bool>
        {
            if (data.size() < kFragmentHeaderSize)
            {
                return {FragmentHeader{}, false};
            }
            
            FragmentHeader header;
            std::memcpy(&header, data.data(), kFragmentHeaderSize);
            return {header, true};
        }

        auto PacketReassembler::reassemble_message(const PendingMessage& pending) 
            -> std::vector<uint8_t>
        {
            std::vector<uint8_t> complete_message;
            complete_message.reserve(pending.total_size);
            
            for (const auto& fragment : pending.fragments)
            {
                complete_message.insert(complete_message.end(), 
                    fragment.begin(), fragment.end());
            }
            
            return complete_message;
        }

        // FragmentationManager implementation
        FragmentationManager::FragmentationManager(size_t mtu)
            : fragmenter_(std::make_unique<PacketFragmenter>(mtu))
            , reassembler_(std::make_unique<PacketReassembler>())
            , next_message_id_(1)
            , messages_fragmented_(0)
            , fragments_sent_(0)
        {
        }

        FragmentationManager::~FragmentationManager() = default;

        auto FragmentationManager::prepare_for_send(const std::vector<uint8_t>& data) 
            -> std::vector<std::vector<uint8_t>>
        {
            auto message_id = generate_message_id();
            auto fragments = fragmenter_->fragment_packet(data, message_id);
            
            if (fragments.size() > 1)
            {
                messages_fragmented_++;
                fragments_sent_ += fragments.size();
                
                Utilities::Logger::debug("Message fragmented: " + 
                    std::to_string(message_id) + " into " + 
                    std::to_string(fragments.size()) + " fragments");
            }
            
            return fragments;
        }

        auto FragmentationManager::process_received(const std::vector<uint8_t>& data) 
            -> std::tuple<bool, std::optional<std::vector<uint8_t>>>
        {
            return reassembler_->process_fragment(data);
        }

        auto FragmentationManager::set_mtu(size_t mtu) -> void
        {
            fragmenter_->set_mtu(mtu);
        }

        auto FragmentationManager::get_mtu() const -> size_t
        {
            return fragmenter_->get_mtu();
        }

        auto FragmentationManager::cleanup() -> size_t
        {
            return reassembler_->cleanup_timeout_messages();
        }

        auto FragmentationManager::get_statistics() const -> Statistics
        {
            Statistics stats;
            stats.messages_fragmented = messages_fragmented_;
            stats.messages_reassembled = reassembler_->get_total_messages_completed();
            stats.fragments_sent = fragments_sent_;
            stats.fragments_received = reassembler_->get_total_fragments_received();
            stats.reassembly_timeouts = reassembler_->get_total_messages_timeout();
            stats.pending_messages = reassembler_->get_pending_message_count();
            return stats;
        }

        auto FragmentationManager::reset_statistics() -> void
        {
            messages_fragmented_ = 0;
            fragments_sent_ = 0;
            reassembler_->reset();
        }

        auto FragmentationManager::generate_message_id() -> uint32_t
        {
            return next_message_id_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}